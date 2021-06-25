#include "raycaster.hpp"

#include <cstring>
#include <cmath>

using namespace gba;

static constexpr auto zero = static_cast<fixed_type>( 0.0f );
static constexpr auto one = static_cast<fixed_type>( 1.0f );
static constexpr auto two = static_cast<fixed_type>( 2.0f );
static constexpr auto screen_width = static_cast<fixed_type>( 240.0f );
static constexpr auto screen_height = static_cast<fixed_type>( 160.0f );
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

void raycaster::render( const fixed_type& camX, const fixed_type& camY, const fixed_type& angle, uint16 * buffer ) noexcept {
    const auto posX = static_cast<fixed_type>( camX );
    const auto posY = static_cast<fixed_type>( camY );

    const auto dirX = static_cast<fixed_type>( cos( angle ) );
    const auto dirY = static_cast<fixed_type>( sin( angle ) );

    const auto planeX = mul( dirY, aspect_ratio );
    const auto planeY = -mul( dirX, aspect_ratio );

    std::memset( buffer, 0, 240 * 160 );

    for ( uint32 xx = 0; xx < 240; xx += 2 ) {
        fixed_type perpWallDist0;
        uint32 texX0;
        const auto texNum0 = ray_cast( xx, dirX, dirY, planeX, planeY, posX, posY, perpWallDist0, texX0 );

        const auto lineHeight0 = div( screen_height, perpWallDist0 );

        auto drawStart0 = -div( lineHeight0, two ) + div( screen_height, two );
        auto drawEnd0 = drawStart0 + lineHeight0;

        if ( drawStart0 < zero ) {
            drawStart0 = zero;
        }
        if ( drawEnd0 > screen_height ) {
            drawEnd0 = screen_height;
        }

        const auto step0 = div( texture_size, lineHeight0 );
        auto texPos0 = mul( ( drawStart0 - div( screen_height, two ) + div( lineHeight0, two ) ), step0 );

        fixed_type perpWallDist1;
        uint32 texX1;
        const auto texNum1 = ray_cast( xx + 1, dirX, dirY, planeX, planeY, posX, posY, perpWallDist1, texX1 );

        const auto lineHeight1 = div( screen_height, perpWallDist0 );

        auto drawStart1 = -div( lineHeight1, two ) + div( screen_height, two );
        auto drawEnd1 = drawStart1 + lineHeight1;

        if ( drawStart1 < zero ) {
            drawStart1 = zero;
        }
        if ( drawEnd1 > screen_height ) {
            drawEnd1 = screen_height;
        }

        const auto step1 = div( texture_size, lineHeight1 );
        auto texPos1 = mul( ( drawStart1 - div( screen_height, two ) + div( lineHeight1, two ) ), step1 );

        const auto drawStart = std::min( static_cast<uint32>( drawStart0 ), static_cast<uint32>( drawStart1 ) );
        const auto drawEnd = std::max( static_cast<uint32>( drawEnd0 ), static_cast<uint32>( drawEnd1 ) );

        for ( auto yy = static_cast<uint32>( drawStart ); yy < static_cast<uint32>( drawEnd ); ++yy ) {
            const auto texY0 = static_cast<uint32>( texPos0 ) & 63u;
            texPos0 += step0;

            const auto texY1 = static_cast<uint32>( texPos1 ) & 63u;
            texPos1 += step1;

            const union {
                uint8 p[2];
                uint16 s;
            } pixel { {
                m_textures[texNum0].data[texX0][texY0],
                m_textures[texNum1].data[texX1][texY1]
            } };

            buffer[( ( yy * 240 ) + xx ) / 2] = pixel.s;
        }
    }
}

uint32 raycaster::ray_cast( const fixed_type& xx, const fixed_type& dirX, const fixed_type& dirY, const fixed_type& planeX, const fixed_type& planeY, const fixed_type& posX, const fixed_type& posY, fixed_type& outPerpWallDist, gba::uint32& outTexX ) const noexcept {
    const auto cameraX = mul( two, div( xx, screen_width ) ) - one;
    const auto rayDirX = dirX + mul( planeX, cameraX );
    const auto rayDirY = dirY + mul( planeY, cameraX );

    auto mapX = static_cast<int>( posX );
    auto mapY = static_cast<int>( posY );

    fixed_type sideDistX;
    fixed_type sideDistY;

    const auto deltaDistX = sqrt( one + div( mul( rayDirY, rayDirY ), mul( rayDirX, rayDirX ) ) );
    const auto deltaDistY = sqrt( one + div( mul( rayDirX, rayDirX ), mul( rayDirY, rayDirY ) ) );
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
        perpWallDist = div( ( static_cast<fixed_type>( mapX ) - posX + div( ( one - static_cast<fixed_type>( stepX ) ), two ) ), rayDirX );
    } else {
        perpWallDist = div( ( static_cast<fixed_type>( mapY ) - posY + div( ( one - static_cast<fixed_type>( stepY ) ), two ) ), rayDirY );
    }

    fixed_type wallX;
    if ( side == 0 ) {
        wallX = posY + mul( perpWallDist, rayDirY );
    } else {
        wallX = posX + mul( perpWallDist, rayDirX );
    }
    wallX -= floor( wallX );

    auto texX = static_cast<int>( mul( wallX, texture_size ) );
    if ( side == 0 && rayDirX > 0 ) {
        texX = 64 - texX - 1;
    }
    if ( side == 1 && rayDirX < 0 ) {
        texX = 64 - texX - 1;
    }

    outPerpWallDist = perpWallDist;
    outTexX = texX;

    return hit - 1;
}
