#include "raycaster.hpp"

#include <cstring>
#include <cmath>

using namespace gba;

static constexpr auto zero = static_cast<float>( 0.0f );
static constexpr auto one = static_cast<float>( 1.0f );
static constexpr auto two = static_cast<float>( 2.0f );
static constexpr auto screen_width = static_cast<float>( 240.0f );
static constexpr auto screen_height = static_cast<float>( 160.0f );
static constexpr auto texture_size = static_cast<float>( 64.0f );
static constexpr auto aspect_ratio = static_cast<float>( 120.0f / 160.0f );

raycaster::raycaster( const map_type& map, const texture_type * textures ) noexcept : m_map { map }, m_textures { textures } {}

void raycaster::render( const fixed_type& camX, const fixed_type& camY, const fixed_type& angle, uint16 * buffer ) noexcept {
    const auto posX = static_cast<float>( camX );
    const auto posY = static_cast<float>( camY );

    const auto dirX = static_cast<float>( cos( angle ) );
    const auto dirY = static_cast<float>( sin( angle ) );

    const auto planeX = dirY * aspect_ratio;
    const auto planeY = -dirX * aspect_ratio;

    std::memset( buffer, 0, 240 * 160 );

    for ( uint32 xx = 0; xx < 240; ++xx ) {
        const auto cameraX = two * static_cast<float>( xx ) / screen_width - one;
        const auto rayDirX = dirX + planeX * cameraX;
        const auto rayDirY = dirY + planeY * cameraX;

        auto mapX = static_cast<int>( posX );
        auto mapY = static_cast<int>( posY );

        float sideDistX;
        float sideDistY;

        const auto deltaDistX = std::sqrt( one + ( rayDirY * rayDirY ) / ( rayDirX * rayDirX ) );
        const auto deltaDistY = std::sqrt( one + ( rayDirX * rayDirX ) / ( rayDirY * rayDirY ) );
        float perpWallDist;

        int stepX;
        int stepY;

        uint32 hit = 0;
        uint32 side;

        if ( rayDirX < 0 ) {
            stepX = -1;
            sideDistX = ( posX - static_cast<float>( mapX ) ) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = ( static_cast<float>( mapX ) + one - posX ) * deltaDistX;
        }

        if ( rayDirY < 0 ) {
            stepY = -1;
            sideDistY = ( posY - static_cast<float>( mapY ) ) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = ( static_cast<float>( mapY ) + one - posY ) * deltaDistY;
        }

        while ( hit == 0 ) {
            if ( sideDistX < sideDistY ) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            hit = m_map[mapX][mapY];
        }

        if ( side == 0 ) {
            perpWallDist = ( static_cast<float>( mapX ) - posX + ( one - static_cast<float>( stepX ) ) / two ) / rayDirX;
        } else {
            perpWallDist = ( static_cast<float>( mapY ) - posY + ( one - static_cast<float>( stepY ) ) / two ) / rayDirY;
        }

        const auto lineHeight = screen_height / perpWallDist;

        auto drawStart = -lineHeight / two + screen_height / two;
        auto drawEnd = drawStart + lineHeight;

        if ( drawStart < zero ) {
            drawStart = zero;
        }
        if ( drawEnd > screen_height ) {
            drawEnd = screen_height;
        }

        const auto texNum = hit - 1;

        float wallX;
        if ( side == 0 ) {
            wallX = posY + perpWallDist * rayDirY;
        } else {
            wallX = posX + perpWallDist * rayDirX;
        }
        wallX -= std::floor( wallX );

        auto texX = static_cast<int>( wallX * texture_size );
        if ( side == 0 && rayDirX > 0 ) {
            texX = 64 - texX - 1;
        }
        if ( side == 1 && rayDirX < 0 ) {
            texX = 64 - texX - 1;
        }

        const auto step = texture_size / lineHeight;
        auto texPos = ( drawStart - ( screen_height / two ) + lineHeight / two ) * step;
        for ( auto yy = static_cast<uint32>( drawStart ); yy < static_cast<uint32>( drawEnd ); ++yy ) {
            const auto texY = static_cast<uint32>( texPos ) & 63u;
            texPos += step;

            union {
                uint16 s;
                uint8 p[2];
            } pixel { buffer[( ( yy * 240 ) + xx ) / 2] };

            pixel.p[xx & 1] = m_textures[texNum].data[texX][texY];

            buffer[( ( yy * 240 ) + xx ) / 2] = pixel.s;
        }
    }
}
