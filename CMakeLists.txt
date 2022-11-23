cmake_minimum_required(VERSION 3.20)

project(gba-raycaster CXX)

# Add host tools
include("${CMAKE_CURRENT_LIST_DIR}/tools/Tools.cmake")
# Add gba-toolchain libraries
gba_add_library_subdirectory(rom agbabi seven gbfs)

add_executable(raycaster main.iwram.cpp assets.cpp camera.iwram.cpp renderer.iwram.cpp)
set_target_properties(raycaster PROPERTIES SUFFIX ".elf")
target_compile_features(raycaster PRIVATE cxx_std_20)

gba_target_sources_compile_options(raycaster
    IWRAM -g0 -O3 -marm
    EWRAM -g0 -O3 -mthumb
)

target_compile_options(raycaster PRIVATE -mthumb -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(raycaster PRIVATE -Wl,--gc-sections)

target_link_libraries(raycaster PRIVATE rom agbabi seven gbfs)

gba_target_objcopy(raycaster FIX_HEADER)

_add_texmaker(wolftextures assets/wolftextures.png)
_add_mapper(map "${CMAKE_SOURCE_DIR}/assets/map.txt")

set(GBFS_ASSETS
    "$<TARGET_PROPERTY:wolftextures,PAL_OUTPUT>"
    "$<TARGET_PROPERTY:wolftextures,BIN_OUTPUT>"
    "$<TARGET_PROPERTY:map,BIN_OUTPUT>"
    "$<TARGET_PROPERTY:map,FLD_OUTPUT>"
)

# GBFS assets
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(gbfsFlags GENERATE_ASM)
    enable_language(ASM)
    target_sources(raycaster PRIVATE "$<TARGET_PROPERTY:assets,ASM_OUTPUT>")
else()
    target_compile_definitions(raycaster PRIVATE ASSETS_GBFS)
endif()

gba_add_gbfs(assets ${gbfsFlags} ${GBFS_ASSETS})
set_target_properties(assets PROPERTIES SUFFIX ".gbfs")
add_dependencies(assets wolftextures map)
add_dependencies(raycaster assets)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # Debug build can just fix header
    gba_target_objcopy(raycaster FIX_HEADER)
else()
    # Release build needs to pad the binary and output an asset-less version
    gba_target_objcopy(raycaster FIX_HEADER PAD 256 OUTPUT raycaster_noassets.gba)

    # Concatenate with raycaster_noassets.gba with assets.gbfs into raycaster.gba
    add_custom_command(TARGET raycaster POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E cat raycaster_noassets.gba assets.gbfs ">" raycaster.gba
    )
endif()
