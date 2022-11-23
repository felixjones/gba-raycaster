#include "renderer.hpp"

#include <cstddef>

#include <seven/video/bg_bitmap.h>
#include <seven/hw/dma.h>
#include <aeabi.h>
#include "assets.hpp"
#include "lut.hpp"

static constexpr auto wall_height = 1.2;
static constexpr auto aspect_ratio = (wall_height * 240.0) / 320.0;
static constexpr auto error_offset = 1.0 / 2048.0;

[[gnu::section(".ewram.sorted.0"), gnu::used]]
constexpr auto camera_x = lut<MODE4_WIDTH>([](const std::size_t x) {
    return fixed_type { ((error_offset + 2.0) * x / double(MODE4_WIDTH) - 1.0) * aspect_ratio };
});
static const auto& ewram_camera_x = *(const fixed_type(*)[MODE4_WIDTH]) 0x2000000;

[[gnu::section(".ewram.sorted.1"), gnu::used]]
constexpr auto distance_z = lut<MODE4_HEIGHT / 2>([](const std::size_t y) {
    return fixed_type { -80.0 / (y - 80.0) };
});
static const auto& ewram_distance_z = *(const fixed_type(*)[MODE4_HEIGHT / 2]) (0x2000000 + (sizeof(fixed_type) * MODE4_WIDTH));

using mode4_row = std::uint32_t[MODE4_WIDTH / 4];

static mode4_row* draw_buffer;

static constexpr auto half_height = MODE4_HEIGHT / 2;
static constinit auto fixed_height = fixed_type { MODE4_HEIGHT };
static constinit auto half_fixed_height = fixed_type { half_height };

static constexpr auto reciprocal_half_fixed_width = fixed_type {2.0 / MODE4_WIDTH };

union pixel_quad {
    std::uint8_t quad[4];
    std::uint32_t value;
};

[[gnu::section(".iwram.bss.column")]]
static pixel_quad column_quad[MODE4_HEIGHT];

[[gnu::section(".iwram.bss.map")]]
static map_type iwram_map[24];

[[gnu::section(".iwram.bss.field")]]
static map_type iwram_field[24];

[[gnu::section(".iwram.sorted.0")]]
static texture_side iwram_cache;
static const auto& iwram_texture = *(const texture_side*) 0x3000000;

static const auto* iwram_texture_lo = iwram_texture[0];
static const auto* iwram_texture_hi = iwram_texture[1];

void renderer::initialize() {
    draw_buffer = reinterpret_cast<mode4_row*>(bmpInitMode4());
    __agbabi_wordset4(column_quad, sizeof(column_quad), -1);
    dmaCopy32(assets::world_map, iwram_map, sizeof(iwram_map));
    dmaCopy32(assets::field_map, iwram_field, sizeof(iwram_field));

    dmaCopy32(&assets::wall_textures[6][0], &iwram_cache[0], sizeof(texture_type));
    dmaCopy32(&assets::wall_textures[3][1], &iwram_cache[1], sizeof(texture_type));
}

void renderer::draw(const camera_type& camera) {
    const auto planeX = -camera.dir_y();
    const auto planeY = camera.dir_x();

    const auto rayDirX0 = camera.dir_x() - planeX;
    const auto rayDirY0 = camera.dir_y() - planeY;

    const auto dirDiffX = planeX * reciprocal_half_fixed_width;
    const auto dirDiffY = planeY * reciprocal_half_fixed_width;

    for (auto y = 0; y < half_height - 3; ++y) {
        auto rowDistance = ewram_distance_z[y];

        auto floorStepX = rowDistance * dirDiffX;
        auto floorStepY = rowDistance * dirDiffY;

        auto floorX = camera.x + (rowDistance * rayDirX0);
        auto floorY = camera.y + (rowDistance * rayDirY0);

        int xx = 0;
        pixel_quad bufferFloor = {};
        pixel_quad bufferCeil = {};
        for (auto x = 0; x < MODE4_WIDTH; ++x) {
            auto tx = (floorX.data >> 10) & 63;
            auto ty = (floorY.data >> 10) & 63;

            floorX += floorStepX;
            floorY += floorStepY;

//            if (y >= 60) {
//                bufferCeil.quad[xx] = iwram_texture_lo[tx][ty];
//                bufferFloor.quad[xx++] = iwram_texture_hi[tx][ty];
//                bufferCeil.quad[xx] = iwram_texture_lo[(tx + 1) & 63][(ty + 1) & 63];
//                bufferFloor.quad[xx++] = iwram_texture_hi[(tx + 1) & 63][(ty + 1) & 63];
//            } else if (y <= 8) {
//                const auto lo = iwram_texture_lo[tx][ty];
//                const auto hi = iwram_texture_hi[tx][ty];
//                bufferCeil.quad[xx] = lo;
//                bufferFloor.quad[xx++] = hi;
//                bufferCeil.quad[xx] = lo;
//                bufferFloor.quad[xx++] = hi;
//            } else {
                bufferCeil.quad[xx] = iwram_texture_lo[ty][tx];
                bufferFloor.quad[xx++] = iwram_texture_hi[ty][tx];
//            }

            if (xx == 4) {
                draw_buffer[y][x >> 2] = bufferCeil.value;
                draw_buffer[(MODE4_HEIGHT - 1) - y][x >> 2] = bufferFloor.value;
                xx = 0;
            }
        }
    }

    const auto cameraMapX = static_cast<int>(camera.x);
    const auto cameraMapY = static_cast<int>(camera.y);

    int quadHeight = 0;
    for (std::uint32_t x = 0; x < MODE4_WIDTH; ++x) {
        if ((x % 4) == 0) {
            for (int y = 0; y < MODE4_HEIGHT; ++y) {
                column_quad[y].value = draw_buffer[y][x >> 2];
            }
        }

        const auto cameraX = ewram_camera_x[x];
        const auto rayDirX = camera.dir_x() + planeX * cameraX;
        const auto rayDirY = camera.dir_y() + planeY * cameraX;

        const auto deltaDistX = fixed_abs(1 / rayDirX);
        const auto deltaDistY = fixed_abs(1 / rayDirY);

        int stepX;
        fixed_type sideDistX;
        if (fixed_negative(rayDirX)) {
            stepX = -1;
            sideDistX = fixed_frac(camera.x) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (1 - fixed_frac(camera.x)) * deltaDistX;
        }

        int stepY;
        fixed_type sideDistY;
        if (fixed_negative(rayDirY))  {
            stepY = -1;
            sideDistY = fixed_frac(camera.y) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (1 - fixed_frac(camera.y)) * deltaDistY;
        }

        auto mapX = cameraMapX;
        auto mapY = cameraMapY;

        int hit, side;
        do {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 1;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 0;
            }
            hit = iwram_map[mapY][mapX];
        } while (!hit);

        hit -= 1;

        fixed_type perpWallDist, wallX;
        if (side) {
            perpWallDist = sideDistX - deltaDistX;
            wallX = fixed_frac(camera.y + perpWallDist * rayDirY);
        } else {
            perpWallDist = sideDistY - deltaDistY;
            wallX = fixed_frac(camera.x + perpWallDist * rayDirX);
        }

        auto texX = wallX.data >> 10;
        if ((side == 0 && rayDirY > 0) || (side == 1 && rayDirX < 0)) {
            texX = 63 - texX;
        }

        const auto lineHeight = static_cast<int>(fixed_height / perpWallDist);
        const auto halfLineHeight = lineHeight >> 1;

        auto drawStart = -halfLineHeight + half_height;
        if (drawStart < 0) {
            drawStart = 0;
        }
        quadHeight = std::max(quadHeight, half_height - drawStart);

        const auto xx = x & 3;
        const auto texture = assets::wall_textures[hit][side][texX];

        const auto step = fixed_type(64) / lineHeight;
        auto texPos = lineHeight < 160 ? fixed_type{} : (drawStart - half_fixed_height + halfLineHeight) * step;
//        if (lineHeight >= 256) [[unlikely]] {
//            const auto step4 = step * 4;
//            const auto off = ~drawStart & 3;
//
//            for (int y = drawStart; y < half_height; y += 4) {
//                const auto itexPos = static_cast<int>(texPos);
//
//                column_quad[y].quad[xx] = column_quad[y + 1].quad[xx] = column_quad[y + 2].quad[xx] = column_quad[y + 3].quad[xx] = texture[itexPos];
//                column_quad[(MODE4_HEIGHT - 1 - off) - y].quad[xx] = column_quad[(MODE4_HEIGHT - off) - y].quad[xx] = column_quad[(MODE4_HEIGHT + 1 - off) - y].quad[xx] = column_quad[(MODE4_HEIGHT + 2 - off) - y].quad[xx] = texture[63 - itexPos];
//                texPos += step4;
//            }
//        } else if (lineHeight >= 128) {
//            const auto step2 = step * 2;
//            const auto off = ~drawStart & 1;
//
//            for (int y = drawStart; y < half_height; y += 2) {
//                const auto itexPos = static_cast<int>(texPos);
//
//                column_quad[y].quad[xx] = column_quad[y + 1].quad[xx] = texture[itexPos];
//                column_quad[(MODE4_HEIGHT - 1 - off) - y].quad[xx] = column_quad[(MODE4_HEIGHT - off) - y].quad[xx] = texture[63 - itexPos];
//                texPos += step2;
//            }
//        } else {
            for (int y = drawStart; y < half_height; ++y) {
                const auto itexPos = static_cast<int>(texPos);

                column_quad[y].quad[xx] = texture[itexPos];
                column_quad[(MODE4_HEIGHT - 1) - y].quad[xx] = texture[63 - itexPos];
                texPos += step;
            }
//        }

        if (xx == 3) {
            const auto x4 = x >> 2;

            const auto start = half_height - quadHeight;
            for (int y = 0; y <= quadHeight; ++y) {
                draw_buffer[start + y][x4] = column_quad[start + y].value;
                column_quad[start + y].value = -1;
                draw_buffer[(half_height + 1) + y][x4] = column_quad[(half_height + 1) + y].value;
                column_quad[(half_height + 1) + y].value = -1;
            }
            quadHeight = 0;
        }
    }
}

void renderer::flip() {
    draw_buffer = reinterpret_cast<mode4_row*>(bmpSwapBuffers());
}
