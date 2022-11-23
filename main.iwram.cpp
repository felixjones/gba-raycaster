#include <seven/prelude.h>
#include <seven/hw/waitstate.h>
#include <seven/util/log.h>

#include "assets.hpp"
#include "camera.hpp"
#include "renderer.hpp"

static constinit auto reset_keys = KEY_A | KEY_B | KEY_SELECT | KEY_START;

static int update_ticks = 0;
static void vblank_callback(u16 flags) IRQ_HANDLER;

int main() {
    REG_WAITCNT = WAIT_SRAM_8 | WAIT_ROM_N_3 | WAIT_ROM_S_1 | WAIT_PREFETCH_ENABLE;

    logInit();
    logSetMaxLevel(LOG_TRACE);

    irqInitDefault();
    irqEnableFull(IRQ_VBLANK);
    irqCallbackSet(IRQ_VBLANK, vblank_callback, 0);

    auto player = camera_type { 11.5, 22.0, 0x6000 };

    assets::load();
    renderer::initialize();

    do {
        svcVBlankIntrWait();
        renderer::flip();
        renderer::draw(player);

        const int updateTicks = update_ticks;
        update_ticks -= updateTicks;
        for (int i = 0; i < updateTicks; ++i) {
            inputPoll();
            player.update();
        }
    } while (inputKeysDown(reset_keys) != reset_keys);

    // Show black screen
    BG_PALETTE[0] = 0;
    REG_DISPCNT = 0;
    irqDisableFull(IRQ_VBLANK);

    // Trap whilst reset_keys are held
    do {
        inputPoll();
    } while (inputKeysDown(reset_keys) == reset_keys);

    return 0;
}

void vblank_callback([[maybe_unused]] u16 flags) {
    ++update_ticks;
}
