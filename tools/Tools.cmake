#===============================================================================
#
# Host tools for 3D raycaster assets
#
# Copyright (C) 2021-2022 gba-toolchain contributors
# For conditions of distribution and use, see copyright notice in LICENSE.md
#
#===============================================================================

cmake_minimum_required(VERSION 3.20)

set(TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR})

function(_find_or_build_program _outVar _name _sourceDir)
    find_program(${_outVar} NAMES "${_name}" PATHS "${TOOLS_DIR}")
    if(${_outVar})
        return()
    endif()

    # Compile in _sourceDir
    get_filename_component(binaryDirName "${CMAKE_CURRENT_BINARY_DIR}" NAME)
    file(MAKE_DIRECTORY "${_sourceDir}/${binaryDirName}")

    # Configure program
    if(NOT WIN32)
        set(hostOptions -DCMAKE_C_COMPILER=cc)
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" .. -DCMAKE_INSTALL_PREFIX=.. ${hostOptions}
        WORKING_DIRECTORY "${_sourceDir}/${binaryDirName}"
        RESULT_VARIABLE cmakeResult
    )
    if(NOT ${cmakeResult} EQUAL 0)
        message(FATAL_ERROR "CMake configure failed for ${_name} (${cmakeResult})")
    endif()

    # Build program
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --target install
        WORKING_DIRECTORY "${_sourceDir}/${binaryDirName}"
        RESULT_VARIABLE cmakeResult
    )
    if(NOT ${cmakeResult} EQUAL 0)
        message(FATAL_ERROR "CMake build failed for ${_name} (${cmakeResult})")
    endif()

    find_program(${_outVar} NAMES "${_name}" PATHS "${TOOLS_DIR}/${_name}")
    if(NOT ${_outVar})
        message(FATAL_ERROR "Failed to install ${_name}")
    endif()
endfunction()

function(_add_texmaker _target _input)
    file(LOCK "${GBA_TOOLCHAIN_LOCK}" GUARD FILE)
    _find_or_build_program(TEXMAKER "texmaker" "${TOOLS_DIR}/texmaker")
    file(LOCK "${GBA_TOOLCHAIN_LOCK}" RELEASE)

    get_filename_component(outPath "${_input}" DIRECTORY)
    get_filename_component(outName "${_input}" NAME_WE)

    add_custom_target(${_target})
    add_custom_command(TARGET ${_target}
        POST_BUILD
        COMMAND "${TEXMAKER}" "${_input}"
        BYPRODUCTS "${outPath}/${outName}.pal" "${outPath}/${outName}.bin"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    set_property(TARGET ${_target} PROPERTY PAL_OUTPUT "${outPath}/${outName}.pal")
    set_property(TARGET ${_target} PROPERTY BIN_OUTPUT "${outPath}/${outName}.bin")
endfunction()

function(_add_mapper _target _input)
    file(LOCK "${GBA_TOOLCHAIN_LOCK}" GUARD FILE)
    _find_or_build_program(MAPPER "mapper" "${TOOLS_DIR}/mapper")
    file(LOCK "${GBA_TOOLCHAIN_LOCK}" RELEASE)

    get_filename_component(outPath "${_input}" DIRECTORY)
    get_filename_component(outName "${_input}" NAME_WE)

    add_custom_target(${_target})
    add_custom_command(TARGET ${_target}
        POST_BUILD
        COMMAND "${MAPPER}" "${_input}"
        BYPRODUCTS "${outPath}/${outName}.bin"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    set_property(TARGET ${_target} PROPERTY BIN_OUTPUT "${outPath}/${outName}.bin")
    set_property(TARGET ${_target} PROPERTY FLD_OUTPUT "${outPath}/${outName}.fld")
endfunction()
