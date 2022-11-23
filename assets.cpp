#include "assets.hpp"

#include <seven/prelude.h>
#include <seven/hw/dma.h>
#include <gbfs.h>

#ifndef ASSETS_GBFS
extern const GBFS_FILE assets_gbfs[];
#else
#include <seven/util/log.h>
#include <seven/svc/system.h>

extern const char __rom_end[];
#endif

const texture_side* assets::wall_textures;
const map_type* assets::world_map;
const map_type* assets::field_map;

void assets::load() {
#ifdef ASSETS_GBFS
    const GBFS_FILE* const assets_gbfs = find_first_gbfs_file(__rom_end);
    if (!assets_gbfs) {
        logOutput(LOG_FATAL, "Could not find assets_gbfs (forgot to concat?)");
        svcHalt();
        __builtin_unreachable();
    }
#endif

    u32 palSize;
    auto* palBinary = gbfs_get_obj(assets_gbfs, "wolftextures.pal", &palSize);
    dmaCopy16(palBinary, MEM_PALETTE, palSize);

    u32 fieldSize;
    field_map = reinterpret_cast<const map_type*>(gbfs_get_obj(assets_gbfs, "map.fld", &fieldSize));

    u32 mapSize;
    world_map = reinterpret_cast<const map_type*>(gbfs_get_obj(assets_gbfs, "map.bin", &mapSize));

    u32 texSize;
    wall_textures = reinterpret_cast<const texture_side*>(gbfs_get_obj(assets_gbfs, "wolftextures.bin", &texSize));
}
