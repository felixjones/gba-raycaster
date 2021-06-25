#include "raycaster.hpp"

#include <cstring>
#include <cmath>

using namespace gba;

static constexpr auto zero = static_cast<fixed_type>( 0.0f );
static constexpr auto one = static_cast<fixed_type>( 1.0f );
static constexpr auto recip_screen_width_half = static_cast<fixed_type>( 2.0f / 240.0f );
static constexpr auto screen_width = static_cast<fixed_type>( 240.0f );
static constexpr auto screen_height = static_cast<fixed_type>( 160.0f );
static constexpr auto screen_height_half = static_cast<fixed_type>( 80.0f );
static constexpr auto texture_size = static_cast<fixed_type>( 64.0f );
static constexpr auto aspect_ratio = static_cast<fixed_type>( 120.0f / 160.0f );

raycaster::raycaster( const map_type& map, const texture_type * textures ) noexcept : m_map { map }, m_textures { textures } {}

static constexpr auto floor( const fixed_type& x ) noexcept {
    return fixed_type::from_data( x.data() & static_cast<int>( 0xffffffff << fixed_type::fractional_digits ) );
}

template <class LhsRep, int LhsExponent, class RhsRep, int RhsExponent>
static constexpr auto mul( const gba::fixed_point<LhsRep, LhsExponent>& lhs, const gba::fixed_point<RhsRep, RhsExponent>& rhs ) noexcept {
    using larger = std::conditional_t<std::is_signed_v<LhsRep> || std::is_signed_v<RhsRep>,
            typename gba::int_type<std::numeric_limits<LhsRep>::digits + std::numeric_limits<RhsRep>::digits>::fast,
            typename gba::uint_type<std::numeric_limits<LhsRep>::digits + std::numeric_limits<RhsRep>::digits>::fast>;
    using word = std::conditional_t<std::is_signed_v<LhsRep> || std::is_signed_v<RhsRep>,
            typename gba::int_type<std::max( std::numeric_limits<LhsRep>::digits, std::numeric_limits<RhsRep>::digits )>::fast,
            typename gba::uint_type<std::max( std::numeric_limits<LhsRep>::digits, std::numeric_limits<RhsRep>::digits )>::fast>;

    constexpr auto max_exponent = std::max( LhsExponent, RhsExponent );
    constexpr auto sum_exponent = LhsExponent + RhsExponent;

    const auto result = gba::fixed_point<larger, sum_exponent>::from_data( gba::fixed_point<larger, LhsExponent>( lhs ).data() * gba::fixed_point<larger, RhsExponent>( rhs ).data() );

    return gba::fixed_point<word, max_exponent>( result );
}

template <class LhsRep, int LhsExponent, class RhsRep, int RhsExponent>
static constexpr auto div( const gba::fixed_point<LhsRep, LhsExponent>& lhs, const gba::fixed_point<RhsRep, RhsExponent>& rhs ) noexcept {
    using larger = std::conditional_t<std::is_signed_v<LhsRep> || std::is_signed_v<RhsRep>,
            typename gba::int_type<std::numeric_limits<LhsRep>::digits + std::numeric_limits<RhsRep>::digits>::fast,
            typename gba::uint_type<std::numeric_limits<LhsRep>::digits + std::numeric_limits<RhsRep>::digits>::fast>;
    using word = std::conditional_t<std::is_signed_v<LhsRep> || std::is_signed_v<RhsRep>,
            typename gba::int_type<std::max( std::numeric_limits<LhsRep>::digits, std::numeric_limits<RhsRep>::digits )>::fast,
            typename gba::uint_type<std::max( std::numeric_limits<LhsRep>::digits, std::numeric_limits<RhsRep>::digits )>::fast>;

    constexpr auto sum_exponent = LhsExponent + RhsExponent;

    return gba::fixed_point<word, LhsExponent>::from_data( gba::fixed_point<larger, sum_exponent>( lhs ).data() / static_cast<larger>( rhs.data() ) );
}

template <class LhsRep, int LhsExponent>
static constexpr auto div2( const gba::fixed_point<LhsRep, LhsExponent>& lhs ) noexcept {
    return gba::fixed_point<LhsRep, LhsExponent>::from_data( lhs.data() >> 1 );
}

template <class LhsRep, int LhsExponent>
static constexpr auto mul2( const gba::fixed_point<LhsRep, LhsExponent>& lhs ) noexcept {
    return gba::fixed_point<LhsRep, LhsExponent>::from_data( lhs.data() << 1 );
}

template <class LhsRep, int LhsExponent>
static constexpr auto mul64( const gba::fixed_point<LhsRep, LhsExponent>& lhs ) noexcept {
    return gba::fixed_point<LhsRep, LhsExponent>::from_data( lhs.data() << 6 );
}

void raycaster::render( const fixed_type& camX, const fixed_type& camY, const int32& angle, uint32 * buffer ) noexcept {
    const auto posX = static_cast<fixed_type>( camX );
    const auto posY = static_cast<fixed_type>( camY );

    const auto dirX = static_cast<fixed_type>( agbabi::cos( angle + 1 ) );
    const auto dirY = static_cast<fixed_type>( agbabi::sin( angle + 1 ) );

    const auto planeX = mul( dirY, aspect_ratio );
    const auto planeY = -mul( dirX, aspect_ratio );

    std::memset( buffer, 0, 240 * 160 );

    for ( uint32 xx = 0; xx < 240; xx += 4 ) {
        fixed_type perpWallDist[4];
        uint32 texX[4];
        const uint32 texNum[] = {
            ray_cast( xx + 0, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[0], texX[0] ),
            ray_cast( xx + 1, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[1], texX[1] ),
            ray_cast( xx + 2, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[2], texX[2] ),
            ray_cast( xx + 3, dirX, dirY, planeX, planeY, posX, posY, perpWallDist[3], texX[3] )
        };

        const fixed_type lineHeight[] = {
            div( screen_height, perpWallDist[0] ),
            div( screen_height, perpWallDist[1] ),
            div( screen_height, perpWallDist[2] ),
            div( screen_height, perpWallDist[3] )
        };

        fixed_type drawStart[] = {
            -div2( lineHeight[0] ) + screen_height_half,
            -div2( lineHeight[1] ) + screen_height_half,
            -div2( lineHeight[2] ) + screen_height_half,
            -div2( lineHeight[3] ) + screen_height_half
        };
        fixed_type drawEnd[] = {
            drawStart[0] + lineHeight[0],
            drawStart[1] + lineHeight[1],
            drawStart[2] + lineHeight[2],
            drawStart[3] + lineHeight[3]
        };

        for ( uint32 ii = 0; ii < 4; ++ii ) {
            if ( drawStart[ii] < zero ) {
                drawStart[ii] = zero;
            }
            if ( drawEnd[ii] > screen_height ) {
                drawEnd[ii] = screen_height;
            }
        }

        const fixed_type step[] = {
            div( texture_size, lineHeight[0] ),
            div( texture_size, lineHeight[1] ),
            div( texture_size, lineHeight[2] ),
            div( texture_size, lineHeight[3] )
        };
        fixed_type texPos[] = {
            mul( ( drawStart[0] - screen_height_half + div2( lineHeight[0] ) ), step[0] ),
            mul( ( drawStart[1] - screen_height_half + div2( lineHeight[1] ) ), step[1] ),
            mul( ( drawStart[2] - screen_height_half + div2( lineHeight[2] ) ), step[2] ),
            mul( ( drawStart[3] - screen_height_half + div2( lineHeight[3] ) ), step[3] )
        };

        const auto drawStartX = std::min(
                static_cast<uint32>( drawStart[0] ), std::min(
                        static_cast<uint32>( drawStart[1] ), std::min(
                                static_cast<uint32>( drawStart[2] ), static_cast<uint32>( drawStart[3] ) ) ) );
        const auto drawEndX = std::max(
                static_cast<uint32>( drawEnd[0] ), std::max(
                    static_cast<uint32>( drawEnd[1] ), std::max(
                            static_cast<uint32>( drawEnd[2] ), static_cast<uint32>( drawEnd[3] ) ) ) );

        for ( auto yy = drawStartX; yy < drawEndX; ++yy ) {
            union {
                uint8 p[4];
                uint32 s;
            } pixel {};

            if ( yy >= static_cast<uint32>( drawStart[0] ) && yy < static_cast<uint32>( drawEnd[0] ) ) {
                const auto texY = static_cast<uint32>( texPos[0] ) & 63u;
                texPos[0] += step[0];
                pixel.p[0] = m_textures[texNum[0]].data[texX[0]][texY];
            }

            if ( yy >= static_cast<uint32>( drawStart[1] ) && yy < static_cast<uint32>( drawEnd[1] ) ) {
                const auto texY = static_cast<uint32>( texPos[1] ) & 63u;
                texPos[1] += step[1];
                pixel.p[1] = m_textures[texNum[1]].data[texX[1]][texY];
            }

            if ( yy >= static_cast<uint32>( drawStart[2] ) && yy < static_cast<uint32>( drawEnd[2] ) ) {
                const auto texY = static_cast<uint32>( texPos[2] ) & 63u;
                texPos[2] += step[2];
                pixel.p[2] = m_textures[texNum[2]].data[texX[2]][texY];
            }

            if ( yy >= static_cast<uint32>( drawStart[3] ) && yy < static_cast<uint32>( drawEnd[3] ) ) {
                const auto texY = static_cast<uint32>( texPos[3] ) & 63u;
                texPos[3] += step[3];
                pixel.p[3] = m_textures[texNum[3]].data[texX[3]][texY];
            }

            buffer[( ( yy * 240u ) + xx ) >> 2u] = pixel.s;
        }
    }
}

uint32 raycaster::ray_cast( const fixed_type& xx, const fixed_type& dirX, const fixed_type& dirY, const fixed_type& planeX, const fixed_type& planeY, const fixed_type& posX, const fixed_type& posY, fixed_type& outPerpWallDist, gba::uint32& outTexX ) const noexcept {
    const auto cameraX = mul( xx, recip_screen_width_half ) - one;
    const auto rayDirX = dirX + mul( planeX, cameraX );
    const auto rayDirY = dirY + mul( planeY, cameraX );

    auto mapX = static_cast<int>( posX );
    auto mapY = static_cast<int>( posY );

    fixed_type sideDistX;
    fixed_type sideDistY;

    const auto rayDirY2 = mul( rayDirY, rayDirY );
    const auto rayDirX2 = mul( rayDirX, rayDirX );

    const auto deltaDistX = sqrt( one + div( rayDirY2, rayDirX2 ) );
    const auto deltaDistY = sqrt( one + div( rayDirX2, rayDirY2 ) );
    fixed_type perpWallDist;

    int stepX;
    int stepY;

    uint32 hit = 0;
    uint32 side;

    if ( rayDirX < 0 ) {
        stepX = -1;
        sideDistX = mul( ( posX - static_cast<fixed_type>( mapX ) ), deltaDistX );
    } else {
        stepX = 1;
        sideDistX = mul( ( static_cast<fixed_type>( mapX ) + one - posX ), deltaDistX );
    }

    if ( rayDirY < 0 ) {
        stepY = -1;
        sideDistY = mul( ( posY - static_cast<fixed_type>( mapY ) ), deltaDistY );
    } else {
        stepY = 1;
        sideDistY = mul( ( static_cast<fixed_type>( mapY ) + one - posY ), deltaDistY );
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
        perpWallDist = div( static_cast<fixed_type>( mapX ) - posX + div2( one - static_cast<fixed_type>( stepX ) ), rayDirX );
    } else {
        perpWallDist = div( static_cast<fixed_type>( mapY ) - posY + div2( one - static_cast<fixed_type>( stepY ) ), rayDirY );
    }

    fixed_type wallX;
    if ( side == 0 ) {
        wallX = posY + mul( perpWallDist, rayDirY );
    } else {
        wallX = posX + mul( perpWallDist, rayDirX );
    }
    wallX -= floor( wallX );

    auto texX = static_cast<int>( mul64( wallX ) );
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
