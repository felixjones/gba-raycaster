#pragma once

#include <array>

#include <gba/gba.hpp>

using fixed_type = gba::make_fixed<15, 16>;

struct texture_type {
    static constexpr auto width = 64;
    static constexpr auto height = 64;

    std::array<std::array<gba::uint8, width>, height> data;
};

using buffer_type = std::array<std::array<gba::uint8, 240>, 160>;

class raycaster {
public:
    static constexpr auto width = 24;
    static constexpr auto height = 24;

    using map_type = std::array<std::array<gba::uint8, width>, height>;

            raycaster( const map_type& map, const texture_type * textures ) noexcept;
    void    render( const fixed_type& posX, const fixed_type& posY, const gba::int32& angle, gba::uint32 * buffer ) noexcept;

    [[nodiscard]]
    const map_type& map() const noexcept {
        return m_map;
    }

protected:
    const map_type& m_map;
    const texture_type * m_textures;

private:
    [[nodiscard]]
    static gba::uint32 ray_cast( const fixed_type& xx, const fixed_type& dirX, const fixed_type& dirY, const fixed_type& planeX, const fixed_type& planeY, const fixed_type& posX, const fixed_type& posY, fixed_type& outPerpWallDist, gba::uint32& outTexX ) noexcept;

    void draw_line_4( gba::uint32 xx, const gba::uint32 texNum[], const fixed_type lineHeight[], const gba::uint32 texX[], gba::uint32 * buffer ) noexcept;
    void draw_line_2x( gba::uint32 xx, const gba::uint32 texNum[], const fixed_type lineHeight[], const gba::uint32 texX[], gba::uint32 * buffer ) noexcept;
    void draw_line_2( gba::uint32 xx, const gba::uint32 texNum[], const fixed_type lineHeight[], const gba::uint32 texX[], gba::uint32 * buffer ) noexcept;
    void draw_line_1( gba::uint32 xx, gba::uint32 texNum, fixed_type lineHeight, gba::uint32 texX, gba::uint32 * buffer ) noexcept;

};
