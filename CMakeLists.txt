cmake_minimum_required(VERSION 3.1)
project(raycaster ASM C CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(${CMAKE_PROJECT_NAME} main.cpp raycaster.iwram.cpp)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -fno-exceptions -fno-rtti -Wall -Wextra")

#====================
# ROM information
#====================

set(ROM_TITLE "Raycaster")
set(ROM_GAME_CODE "CRCE")
set(ROM_MAKER_CODE "XI")
set(ROM_VERSION 0)

#====================
# GBA toolchain
#====================

if(GBA_TOOLCHAIN)
    gba_target_link_agb_abi(${CMAKE_PROJECT_NAME})
    gba_target_link_gba_plusplus(${CMAKE_PROJECT_NAME})
    gba_target_link_gbfs(${CMAKE_PROJECT_NAME})
    gba_target_link_posprintf(${CMAKE_PROJECT_NAME})

    #====================
    # Assets
    #====================
    gba_add_gbfs_target( assets.gbfs "assets/wolftextures.bin" "assets/wolftextures.pal.bin" "assets/cgtutor.bin")
    gba_target_add_gbfs_dependency(${CMAKE_PROJECT_NAME} assets.gbfs)

    gba_target_sources_instruction_set(${CMAKE_PROJECT_NAME} thumb)
    gba_target_link_runtime(${CMAKE_PROJECT_NAME} rom)
    gba_target_object_copy(${CMAKE_PROJECT_NAME} "${CMAKE_PROJECT_NAME}.elf" "${CMAKE_PROJECT_NAME}.gba")
    gba_target_fix(${CMAKE_PROJECT_NAME} "${CMAKE_PROJECT_NAME}.gba" "${ROM_TITLE}" "${ROM_GAME_CODE}" "${ROM_MAKER_CODE}" ${ROM_VERSION})
endif()

#====================
# Texture compiler
#====================

ExternalProject_Add(tex
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/tex/"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/tex/"
    PREFIX "${CMAKE_SOURCE_DIR}/tex/"
    INSTALL_COMMAND ""
)

add_custom_command(TARGET tex
    POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/tex/tex" "${CMAKE_SOURCE_DIR}/tex/wolftextures.png"
    COMMENT "Compiling textures"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
)

add_dependencies(assets.gbfs tex)

#====================
# Map compiler
#====================

ExternalProject_Add(map
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/map/"
    BINARY_DIR "${CMAKE_SOURCE_DIR}/map/"
    PREFIX "${CMAKE_SOURCE_DIR}/map/"
    INSTALL_COMMAND ""
)

add_custom_command(TARGET map
    POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/map/map" "${CMAKE_SOURCE_DIR}/map/cgtutor.txt"
    COMMENT "Compiling map"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
)

add_dependencies(assets.gbfs map)
