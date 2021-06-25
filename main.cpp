#include <gba/gba.hpp>
#include <gba/ext/agbabi.hpp>

typedef unsigned short u16;
typedef unsigned int u32;
#include <gbfs.h>

extern const GBFS_FILE assets_gbfs;

#include "raycaster.hpp"

using namespace gba;
using namespace agbabi;

static constexpr auto reset_keys = key::button_a | key::button_b | key::select | key::start;
static constexpr auto reset_flags = bios::reset_flags { .ewram = true, .iwram = true, .palette = true, .vram = true, .oam = true, .reg_sio = true, .reg_sound = true, .reg = true };

static void load_palette();

struct camera_type {
    vec2<fixed_type> pos;
    int32 angle;
};

static uint32 simulation_frames = 0;
static void irq_handler( const interrupt_mask mask ) noexcept {
    if ( mask.vblank ) {
        simulation_frames++;
    }
}

int main() {
    agbabi::interrupt_handler::set( irq_handler );

    reg::dispstat::write( display_status { .vblank_irq = true } );
    reg::ie::write( interrupt_mask { .vblank = true } );
    reg::ime::emplace( true );

    auto * wolfTextures = reinterpret_cast<const texture_type *>( gbfs_get_obj( &assets_gbfs, "wolftextures.bin", nullptr ) );
    load_palette();

    auto * map = reinterpret_cast<const raycaster::map_type *>( gbfs_get_obj( &assets_gbfs, "cgtutor.bin", nullptr ) );
    auto level = raycaster( *map, wolfTextures );

    auto camera = camera_type {};
    camera.pos.y = fixed_type { 11.5 };
    camera.pos.x = fixed_type { 22.5 };
    camera.angle = 0x4000;

    auto displayControl = io::mode<4>::display_control().set_layer_background_2( true );
    reg::dispcnt::write( displayControl );

    uint32 frameIndex = 0;
    gba::uint32 * const frameBuffers[] = {
        reinterpret_cast<gba::uint32 *>( 0x06000000 ),
        reinterpret_cast<gba::uint32 *>( 0x0600A000 )
    };

    reg::waitcnt::write( waitstate::control { .use_game_pak_prefetch = true } );

    io::keypad_manager keypad;
    while ( keypad.is_up( reset_keys ) ) {
        keypad.poll();

        while ( simulation_frames ) {
            if ( keypad.is_down( key::left ) ) {
                camera.angle += 0x80;
            } else if ( keypad.is_down( key::right ) ) {
                camera.angle -= 0x80;
            }

            if ( keypad.is_down( key::up ) ) {
                auto px = camera.pos.x + agbabi::cos( camera.angle ) / 16;
                if ( level.map()[static_cast<int>( px )][static_cast<int>( camera.pos.y )] ) {
                    px = camera.pos.x;
                }
                auto py = camera.pos.y + agbabi::sin( camera.angle ) / 16;
                if ( level.map()[static_cast<int>( px )][static_cast<int>( py )] ) {
                    py = camera.pos.y;
                }
                camera.pos.x = px;
                camera.pos.y = py;
            } else if ( keypad.is_down( key::down ) ) {
                auto px = camera.pos.x - agbabi::cos( camera.angle ) / 16;
                if ( level.map()[static_cast<int>( px )][static_cast<int>( camera.pos.y )] ) {
                    px = camera.pos.x;
                }
                auto py = camera.pos.y - agbabi::sin( camera.angle ) / 16;
                if ( level.map()[static_cast<int>( px )][static_cast<int>( py )] ) {
                    py = camera.pos.y;
                }
                camera.pos.x = px;
                camera.pos.y = py;
            }
            simulation_frames -= 1;
        }

        displayControl.flip_page();
        reg::dispcnt::write( displayControl );

        level.render( camera.pos.x, camera.pos.y, camera.angle, frameBuffers[frameIndex] );
        frameIndex = 1 - frameIndex;
    }

    reg::ime::emplace( false );
    while ( keypad.is_down( reset_keys ) ) {
        keypad.poll();
    }
    bios::register_ram_reset( reset_flags );
}

static void load_palette() {
    u32 wolfPaletteLen;
    auto * wolfPalette = gbfs_get_obj( &assets_gbfs, "wolftextures.pal.bin", &wolfPaletteLen );
    auto palette = allocator::palette();
    auto backgroundPalette = palette.allocate_background( 256 );
    backgroundPalette.dma3_data( wolfPaletteLen, wolfPalette );
}
