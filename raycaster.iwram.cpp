#include "raycaster.hpp"

using namespace gba;

static constexpr auto zero = static_cast<fixed_type>( 0.0f );
static constexpr auto one = static_cast<fixed_type>( 1.0f );
static constexpr auto recip_screen_width_half = static_cast<fixed_type>( 2.0f / 240.0f );
static constexpr auto screen_height = static_cast<fixed_type>( 160.0f );
static constexpr auto screen_height_half = static_cast<fixed_type>( 80.0f );
static constexpr auto texture_size = static_cast<fixed_type>( 64.0f );
static constexpr auto texture_size_two = static_cast<fixed_type>( 64.0f * 2.0f );
static constexpr auto texture_size_three = static_cast<fixed_type>( 64.0f * 3.0f );
static constexpr auto aspect_ratio = static_cast<fixed_type>( 120.0f / 160.0f );
static constexpr auto max = fixed_type::from_data( 0x7fffffff );

static constexpr auto texture_copy = dma_transfer_control { .transfers = uint16( ( 64 * 64 ) / 4 ), .control = { .type = dma_control::type::word, .enable = true } };

#if defined( NDEBUG )
static std::array<texture_type, 4> texture_cache = {};
static std::array<uint32, 4> texture_cache_ids = { -1u, -1u, -1u, -1u };
static std::array<raycaster::map_type, 1> map_cache;
#else
static texture_type * const texture_cache = new texture_type[8];
static std::array<uint32, 8> texture_cache_ids = { -1u, -1u, -1u, -1u, -1u, -1u, -1u, -1u };
static raycaster::map_type * const map_cache = new raycaster::map_type[1];
#endif

raycaster::raycaster( const map_type& map, const texture_type * textures ) noexcept : m_map { map }, m_textures { textures } {
    map_cache[0] = m_map; // Copy map into faster IWRAM
}

inline constexpr auto fx_floor( const fixed_type& x ) noexcept {
    return fixed_type::from_data( x.data() & static_cast<int>( 0xffffffff << fixed_type::fractional_digits ) );
}

inline constexpr auto fx_mul( const fixed_type& lhs, const fixed_type& rhs ) noexcept {
    if ( lhs == zero || rhs == zero ) {
        return zero;
    }

    using larger = std::conditional_t<std::is_signed_v<fixed_type::rep>,
            typename int_type<std::numeric_limits<fixed_type::rep>::digits + std::numeric_limits<fixed_type::rep>::digits>::fast,
            typename uint_type<std::numeric_limits<fixed_type::rep>::digits + std::numeric_limits<fixed_type::rep>::digits>::fast>;

    constexpr auto sum_exponent = fixed_type::exponent + fixed_type::exponent;

    const auto result = fixed_point<larger, sum_exponent>::from_data( fixed_point<larger, fixed_type::exponent>( lhs ).data() * fixed_point<larger, fixed_type::exponent>( rhs ).data() );

    return fixed_type( result );
}

inline constexpr auto fx_div( const fixed_type& lhs, const fixed_type& rhs ) noexcept {
    if ( rhs == zero ) {
        return max;
    }

    using larger = std::conditional_t<std::is_signed_v<fixed_type::rep>,
            typename int_type<std::numeric_limits<fixed_type::rep>::digits + std::numeric_limits<fixed_type::rep>::digits>::fast,
            typename uint_type<std::numeric_limits<fixed_type::rep>::digits + std::numeric_limits<fixed_type::rep>::digits>::fast>;

    constexpr auto sum_exponent = fixed_type::exponent + fixed_type::exponent;

    return fixed_type::from_data( static_cast<fixed_type::rep>( fixed_point<larger, sum_exponent>( lhs ).data() / static_cast<larger>( rhs.data() ) ) );
}

inline constexpr auto fx_div2( const fixed_type& lhs ) noexcept {
    return fixed_type::from_data( lhs.data() >> 1 ); // Divide by 2
}

inline constexpr auto fx_mul64( const fixed_type& lhs ) noexcept {
    return fixed_type::from_data( lhs.data() << 6 ); // Multiply by 64
}

void raycaster::render( const fixed_type& posX, const fixed_type& posY, const int32& angle, uint32 * buffer ) noexcept {
    const auto dirX = fixed_type( agbabi::cos( angle ) );
    const auto dirY = fixed_type( agbabi::sin( angle ) );

    const auto planeX = fx_mul( dirY, aspect_ratio );
    const auto planeY = -fx_mul( dirX, aspect_ratio );

    // Arrays for ray-casting results
    fixed_type perpWallDist[4];
    uint32 texX[4];
    uint32 texNum[4];
    fixed_type lineHeight[4];

    for ( uint32 xx = 0; xx < 240; xx += 4 ) {
        texNum[0] = ray_cast( xx + 0, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[0], texX[0] );
        lineHeight[0] = fx_div( screen_height, perpWallDist[0] );

        if ( lineHeight[0] > texture_size_three ) {
            // Use 1 ray across 4 pixels
            draw_line_1( xx, texNum[0], lineHeight[0], texX[0], buffer );
        } else {
            texNum[2] = ray_cast( xx + 2, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[2], texX[2] );
            lineHeight[2] = fx_div( screen_height, perpWallDist[2] );

            if ( lineHeight[2] > texture_size_two ) {
                // Use 2 rays across 4 pixels
                draw_line_2( xx, texNum, lineHeight, texX, buffer );
            } else if ( lineHeight[2] < texture_size ) {
                // Use 2 rays across 4 pixels
                draw_line_2x( xx, texNum, lineHeight, texX, buffer );
            } else {
                texNum[1] = ray_cast( xx + 1, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[1], texX[1] );
                texNum[3] = ray_cast( xx + 3, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[3], texX[3] );

                lineHeight[1] = fx_div( screen_height, perpWallDist[1] );
                lineHeight[3] = fx_div( screen_height, perpWallDist[3] );

                // Use 4 rays across 4 pixels
                draw_line_4( xx, texNum, lineHeight, texX, buffer );
            }
        }
    }
}

/**
 * Render 4 pixels from 1 ray
 * Fastest at quarter resolution
 */
void raycaster::draw_line_1( const uint32 xx, const uint32 texNum, const fixed_type lineHeight, const uint32 texX, uint32 * buffer ) noexcept {
    auto drawStart = -fx_div2( lineHeight ) + screen_height_half;
    auto drawEnd = drawStart + lineHeight;

    if ( drawStart < zero ) {
        drawStart = zero;
        drawEnd = screen_height;
    }

    if ( texture_cache_ids[0] != texNum ) {
        texture_cache_ids[0] = texNum;

        reg::dma3cnt_h::emplace();
        reg::dma3sad::emplace( reinterpret_cast<uint32>( &m_textures[texNum] ) );
        reg::dma3dad::emplace( reinterpret_cast<uint32>( &texture_cache[0] ) );
        reg::dma3cnt::write( texture_copy );
    }

    const auto drawStart32 = static_cast<int32>( drawStart );
    const auto drawEnd32 = static_cast<int32>( drawEnd );

    const auto step = fx_div( texture_size, lineHeight );
    auto texPos = fx_mul( ( drawStart - screen_height_half + fx_div2( lineHeight ) ), step );

    uint8 pixel[4];

    for ( auto yy = 0; yy < 160; ++yy ) {
        if ( yy >= drawStart32 && yy < drawEnd32 ) {
            const auto texY = static_cast<int32>( texPos ) & 63;
            texPos += step;

            const auto color = texture_cache[0].data[texX][texY];

            pixel[0] = pixel[1] = pixel[2] = pixel[3] = color;

            buffer[( ( yy * 240u ) + xx ) >> 2u] = uint_cast( pixel );
        } else {
            buffer[( ( yy * 240u ) + xx ) >> 2u] = 0;
        }
    }
}

/**
 * Render 4 pixels from 2 rays
 * 2 of the pixels are estimated based on the 2 pixels from the 2 rays
 */
void raycaster::draw_line_2x( const uint32 xx, const uint32 texNum[], const fixed_type lineHeight[], const uint32 texX[], uint32 * buffer ) noexcept {
    fixed_type drawStart[] = {
        -fx_div2( lineHeight[0] ) + screen_height_half,
        -fx_div2( lineHeight[2] ) + screen_height_half
    };
    fixed_type drawEnd[] = {
        drawStart[0] + lineHeight[0],
        drawStart[1] + lineHeight[2]
    };

    for ( uint32 ii = 0; ii < 2; ++ii ) {
        if ( texture_cache_ids[ii * 2] != texNum[ii * 2] ) {
            texture_cache_ids[ii * 2] = texNum[ii * 2];

            reg::dma3cnt_h::emplace();
            reg::dma3sad::emplace( reinterpret_cast<uint32>( &m_textures[texNum[ii * 2]] ) );
            reg::dma3dad::emplace( reinterpret_cast<uint32>( &texture_cache[ii * 2] ) );
            reg::dma3cnt::write( texture_copy );
        }
    }

    const int32 drawStart32[] = {
        static_cast<int32>( drawStart[0] ),
        static_cast<int32>( drawStart[1] )
    };
    const int32 drawEnd32[] = {
        static_cast<int32>( drawEnd[0] ),
        static_cast<int32>( drawEnd[1] )
    };

    const fixed_type step[] = {
        fx_div( texture_size, lineHeight[0] ),
        fx_div( texture_size, lineHeight[2] )
    };

    fixed_type texPos[] = {
        fx_mul( ( drawStart[0] - screen_height_half + fx_div2( lineHeight[0] ) ), step[0] ),
        fx_mul( ( drawStart[1] - screen_height_half + fx_div2( lineHeight[2] ) ), step[1] )
    };

    for ( auto yy = 0; yy < 160; ++yy ) {
        uint8 pixel[4] {};

        for ( int ii = 0; ii < 2; ++ii ) {
            if ( yy >= drawStart32[ii] && yy < drawEnd32[ii] ) {
                const auto texY = static_cast<int32>( texPos[ii] ) & 63;
                texPos[ii] += step[ii];

                pixel[ii * 2 + 0] = texture_cache[ii * 2].data[texX[ii * 2] + 0][texY];
                pixel[ii * 2 + 1] = texture_cache[ii * 2].data[std::min( texX[ii * 2] + 1, 63u )][texY];
            }
        }

        buffer[( ( yy * 240u ) + xx ) >> 2u] = uint_cast( pixel );
    }
}

/**
 * Render 2 pixels from 2 rays
 * Half resolution
 */
void raycaster::draw_line_2( const uint32 xx, const uint32 texNum[], const fixed_type lineHeight[], const uint32 texX[], uint32 * buffer ) noexcept {
    fixed_type drawStart[] = {
        -fx_div2( lineHeight[0] ) + screen_height_half,
        -fx_div2( lineHeight[2] ) + screen_height_half
    };
    fixed_type drawEnd[] = {
        drawStart[0] + lineHeight[0],
        drawStart[1] + lineHeight[2]
    };

    for ( uint32 ii = 0; ii < 2; ++ii ) {
        if ( drawStart[ii] < zero ) {
            drawStart[ii] = zero;
            drawEnd[ii] = screen_height;
        }

        if ( texture_cache_ids[ii * 2] != texNum[ii * 2] ) {
            texture_cache_ids[ii * 2] = texNum[ii * 2];

            reg::dma3cnt_h::emplace();
            reg::dma3sad::emplace( reinterpret_cast<uint32>( &m_textures[texNum[ii * 2]] ) );
            reg::dma3dad::emplace( reinterpret_cast<uint32>( &texture_cache[ii * 2] ) );
            reg::dma3cnt::write( texture_copy );
        }
    }

    const int32 drawStart32[] = {
        static_cast<int32>( drawStart[0] ),
        static_cast<int32>( drawStart[1] )
    };
    const int32 drawEnd32[] = {
        static_cast<int32>( drawEnd[0] ),
        static_cast<int32>( drawEnd[1] )
    };

    const fixed_type step[] = {
        fx_div( texture_size, lineHeight[0] ),
        fx_div( texture_size, lineHeight[2] )
    };

    fixed_type texPos[] = {
        fx_mul( ( drawStart[0] - screen_height_half + fx_div2( lineHeight[0] ) ), step[0] ),
        fx_mul( ( drawStart[1] - screen_height_half + fx_div2( lineHeight[2] ) ), step[1] )
    };

    for ( auto yy = 0; yy < 160; ++yy ) {
        uint8 pixel[4] {};

        for ( int ii = 0; ii < 2; ++ii ) {
            if ( yy >= drawStart32[ii] && yy < drawEnd32[ii] ) {
                const auto texY = static_cast<int32>( texPos[ii] ) & 63;
                texPos[ii] += step[ii];

                const auto color = texture_cache[ii * 2].data[texX[ii * 2]][texY];
                pixel[ii * 2 + 0] = pixel[ii * 2 + 1] = color;
            }
        }

        buffer[( ( yy * 240u ) + xx ) >> 2u] = uint_cast( pixel );
    }
}

/**
 * Render 4 pixels from 4 rays
 * Slowest, but full resolution
 */
void raycaster::draw_line_4( const uint32 xx, const uint32 texNum[], const fixed_type lineHeight[], const uint32 texX[], uint32 * buffer ) noexcept {
    const fixed_type drawStart[] = {
        -fx_div2( lineHeight[0] ) + screen_height_half,
        -fx_div2( lineHeight[1] ) + screen_height_half,
        -fx_div2( lineHeight[2] ) + screen_height_half,
        -fx_div2( lineHeight[3] ) + screen_height_half
    };
    const fixed_type drawEnd[] = {
        drawStart[0] + lineHeight[0],
        drawStart[1] + lineHeight[1],
        drawStart[2] + lineHeight[2],
        drawStart[3] + lineHeight[3]
    };

    for ( uint32 ii = 0; ii < 4; ++ii ) {
        if ( texture_cache_ids[ii] != texNum[ii] ) {
            texture_cache_ids[ii] = texNum[ii];

            reg::dma3cnt_h::emplace();
            reg::dma3sad::emplace( reinterpret_cast<uint32>( &m_textures[texNum[ii]] ) );
            reg::dma3dad::emplace( reinterpret_cast<uint32>( &texture_cache[ii] ) );
            reg::dma3cnt::write( texture_copy );
        }
    }

    const int32 drawStart32[] = {
        static_cast<int32>( drawStart[0] ),
        static_cast<int32>( drawStart[1] ),
        static_cast<int32>( drawStart[2] ),
        static_cast<int32>( drawStart[3] )
    };
    const int32 drawEnd32[] = {
        static_cast<int32>( drawEnd[0] ),
        static_cast<int32>( drawEnd[1] ),
        static_cast<int32>( drawEnd[2] ),
        static_cast<int32>( drawEnd[3] )
    };

    const fixed_type step[] = {
        fx_div( texture_size, lineHeight[0] ),
        fx_div( texture_size, lineHeight[1] ),
        fx_div( texture_size, lineHeight[2] ),
        fx_div( texture_size, lineHeight[3] )
    };

    fixed_type texPos[] = {
        fx_mul( ( drawStart[0] - screen_height_half + fx_div2( lineHeight[0] ) ), step[0] ),
        fx_mul( ( drawStart[1] - screen_height_half + fx_div2( lineHeight[1] ) ), step[1] ),
        fx_mul( ( drawStart[2] - screen_height_half + fx_div2( lineHeight[2] ) ), step[2] ),
        fx_mul( ( drawStart[3] - screen_height_half + fx_div2( lineHeight[3] ) ), step[3] )
    };

    for ( auto yy = 0; yy < 160; ++yy ) {
        uint8 pixel[4] {};

        for ( int ii = 0; ii < 4; ++ii ) {
            if ( yy >= drawStart32[ii] && yy < drawEnd32[ii] ) {
                const auto texY = static_cast<int32>( texPos[ii] ) & 63;
                texPos[ii] += step[ii];

                pixel[ii] = texture_cache[ii].data[texX[ii]][texY];
            }
        }

        buffer[( ( yy * 240u ) + xx ) >> 2u] = uint_cast( pixel );
    }
}

/**
 * https://lodev.org/cgtutor/raycasting.html
 */
uint32 raycaster::ray_cast( const fixed_type& xx, const fixed_type& dirX, const fixed_type& dirY, const fixed_type& planeX, const fixed_type& planeY, const fixed_type& posX, const fixed_type& posY, fixed_type& outPerpWallDist, uint32& outTexX ) noexcept {
    const auto cameraX = fx_mul( xx, recip_screen_width_half ) - one;
    const auto rayDirX = dirX + fx_mul( planeX, cameraX );
    const auto rayDirY = dirY + fx_mul( planeY, cameraX );

    auto mapX = static_cast<int>( posX );
    auto mapY = static_cast<int>( posY );

    fixed_type sideDistX;
    fixed_type sideDistY;

    const auto rayDirY2 = fx_mul( rayDirY, rayDirY );
    const auto rayDirX2 = fx_mul( rayDirX, rayDirX );

    const auto deltaDistX = sqrt( one + fx_div( rayDirY2, rayDirX2 ) );
    const auto deltaDistY = sqrt( one + fx_div( rayDirX2, rayDirY2 ) );
    fixed_type perpWallDist;

    int stepX;
    int stepY;

    uint32 hit = 0;
    uint32 side;

    if ( rayDirX < 0 ) {
        stepX = -1;
        sideDistX = fx_mul( ( posX - static_cast<fixed_type>( mapX ) ), deltaDistX );
    } else {
        stepX = 1;
        sideDistX = fx_mul( ( static_cast<fixed_type>( mapX ) + one - posX ), deltaDistX );
    }

    if ( rayDirY < 0 ) {
        stepY = -1;
        sideDistY = fx_mul( ( posY - static_cast<fixed_type>( mapY ) ), deltaDistY );
    } else {
        stepY = 1;
        sideDistY = fx_mul( ( static_cast<fixed_type>( mapY ) + one - posY ), deltaDistY );
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

        hit = map_cache[0][mapX][mapY];
    }

    if ( side == 0 ) {
        perpWallDist = fx_div( static_cast<fixed_type>( mapX ) - posX + fx_div2( one - static_cast<fixed_type>( stepX ) ), rayDirX );
    } else {
        perpWallDist = fx_div( static_cast<fixed_type>( mapY ) - posY + fx_div2( one - static_cast<fixed_type>( stepY ) ), rayDirY );
        hit += 8;
    }

    fixed_type wallX;
    if ( side == 0 ) {
        wallX = posY + fx_mul( perpWallDist, rayDirY );
    } else {
        wallX = posX + fx_mul( perpWallDist, rayDirX );
    }
    wallX -= fx_floor( wallX );

    auto texX = static_cast<int>( fx_mul64( wallX ) );
    if ( side == 0 && rayDirX > 0 ) {
        texX = 64 - texX - 1;
    }
    if ( side == 1 && rayDirY < 0 ) {
        texX = 64 - texX - 1;
    }

    outPerpWallDist = perpWallDist;
    outTexX = texX;

    return hit - 1;
}
