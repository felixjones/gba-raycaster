#pragma once

#include <cstdint>

using texture_row = std::uint8_t[64];
using texture_type = texture_row[64];
using texture_side = texture_type[2];

using map_row = std::uint8_t[24];
using map_type = map_row;

namespace assets {

extern const texture_side* wall_textures;
extern const map_type* world_map;
extern const map_type* field_map;

void load();
void load_texture(int num);

}
