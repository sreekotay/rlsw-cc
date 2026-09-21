# rlsw-cc — Concurrent-C / optimized fork of raylib's rlsw software rasterizer.
#
# Consume from a parent that already has FetchContent raylib (Software / RGFW):
#
#   add_subdirectory(rlsw-cc)
#   rlsw_cc_overlay_raylib(raylib "${raylib_SOURCE_DIR}")  # header + soft present
#   target_link_libraries(my_app PRIVATE raylib rlsw_cc)   # rlsw_cc if RLSW_CC_PARALLEL
#
# Overlay is required: rlgl.h does #include "external/rlsw.h" relative to raylib src/.
# Soft present (default ON) patches RGFW SwapScreenBuffer for a stable/fast blit.
#
# @parallel links against vendor/cccportable (from `ccc portable-install`) — consumers
# need only a C compiler, not `ccc`. Authors refresh with tools/vendor-ccc.sh.

cmake_minimum_required(VERSION 3.16)

set(RLSW_CC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "rlsw-cc package root")
set(RLSW_CC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/rlsw.h" CACHE INTERNAL "")
set(RLSW_CC_SPAN_KERNELS "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/span_kernels.c" CACHE INTERNAL "")
set(RLSW_CC_BIN_PAR "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/bin_par.c" CACHE INTERNAL "")
set(RLSW_CC_STOCK_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/stock/rlsw.h" CACHE INTERNAL "")
set(RLSW_CC_PORTABLE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cccportable" CACHE INTERNAL "")

option(RLSW_CC_PARALLEL "Build @parallel stripe flush via vendor/cccportable (default ON; no ccc needed)" ON)
option(RLSW_CC_USE_STOCK "Overlay stock rlsw 1.5 instead of the cc fork" OFF)
option(RLSW_CC_SOFT_PRESENT "Patch RGFW SwapScreenBuffer for fast/stable software present (default ON)" ON)

set(RLSW_CC_SOFT_PRESENT_APPLY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/overlays/apply_sw_present.py" CACHE INTERNAL "")
set(RLSW_CC_SOFT_PRESENT_C "${CMAKE_CURRENT_SOURCE_DIR}/cmake/overlays/macos_sw_present.c" CACHE INTERNAL "")

# -----------------------------------------------------------------------------
# Soft present: patch raylib RGFW swap + (Apple) CGImage staging present
# -----------------------------------------------------------------------------
function(rlsw_cc_apply_soft_present raylib_target raylib_src_dir)
    if(NOT RLSW_CC_SOFT_PRESENT)
        return()
    endif()
    if(NOT EXISTS "${RLSW_CC_SOFT_PRESENT_APPLY}")
        message(FATAL_ERROR "rlsw-cc: missing ${RLSW_CC_SOFT_PRESENT_APPLY}")
    endif()

    set(_platform_c "${raylib_src_dir}/src/platforms/rcore_desktop_rgfw.c")
    set(_rgfw_h "${raylib_src_dir}/src/external/RGFW/RGFW.h")
    set(_stamp "${CMAKE_BINARY_DIR}/rlsw_cc_soft_present.stamp")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${RLSW_CC_SOFT_PRESENT_APPLY}" "${raylib_src_dir}"
        RESULT_VARIABLE _overlay_rc
    )
    if(NOT _overlay_rc EQUAL 0)
        message(FATAL_ERROR "rlsw-cc: soft present overlay failed (${_overlay_rc})")
    endif()

    add_custom_command(
        OUTPUT "${_stamp}"
        COMMAND "${Python3_EXECUTABLE}" "${RLSW_CC_SOFT_PRESENT_APPLY}" "${raylib_src_dir}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
        DEPENDS "${RLSW_CC_SOFT_PRESENT_APPLY}" "${_platform_c}" "${_rgfw_h}" "${RLSW_CC_SOFT_PRESENT_C}"
        COMMENT "rlsw-cc: apply soft present overlay"
        VERBATIM
    )
    add_custom_target(rlsw_cc_soft_present DEPENDS "${_stamp}")
    add_dependencies(${raylib_target} rlsw_cc_soft_present)

    set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
        OBJECT_DEPENDS "${_platform_c};${_rgfw_h};${_stamp}"
    )

    if(APPLE)
        target_sources(${raylib_target} PRIVATE "${RLSW_CC_SOFT_PRESENT_C}")
        target_link_libraries(${raylib_target} PRIVATE
            "-framework CoreGraphics" "-framework CoreFoundation")
        set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
            OBJECT_DEPENDS "${_platform_c};${_rgfw_h};${RLSW_CC_SOFT_PRESENT_C};${_stamp}"
        )
    endif()
endfunction()

# Attach macos_sw_present.c to an extra raylib target (e.g. raylib_stock) that
# shares the already-patched RGFW sources.
function(rlsw_cc_soft_present_attach extra_target)
    if(NOT RLSW_CC_SOFT_PRESENT OR NOT APPLE)
        return()
    endif()
    if(NOT TARGET ${extra_target})
        return()
    endif()
    target_sources(${extra_target} PRIVATE "${RLSW_CC_SOFT_PRESENT_C}")
    target_link_libraries(${extra_target} PRIVATE
        "-framework CoreGraphics" "-framework CoreFoundation")
    if(TARGET rlsw_cc_soft_present)
        add_dependencies(${extra_target} rlsw_cc_soft_present)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Overlay fork (or stock) onto a raylib source tree's src/external/
# -----------------------------------------------------------------------------
function(rlsw_cc_overlay_raylib raylib_target raylib_src_dir)
    if(RLSW_CC_USE_STOCK)
        set(_hdr "${RLSW_CC_STOCK_HEADER}")
        set(_kernels OFF)
    else()
        set(_hdr "${RLSW_CC_HEADER}")
        set(_kernels ON)
    endif()

    configure_file("${_hdr}" "${raylib_src_dir}/src/external/rlsw.h" COPYONLY)
    if(_kernels)
        configure_file("${RLSW_CC_SPAN_KERNELS}" "${raylib_src_dir}/src/external/span_kernels.c" COPYONLY)
        add_custom_target(rlsw_cc_overlay
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_hdr}" "${raylib_src_dir}/src/external/rlsw.h"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RLSW_CC_SPAN_KERNELS}" "${raylib_src_dir}/src/external/span_kernels.c"
            DEPENDS "${_hdr}" "${RLSW_CC_SPAN_KERNELS}"
            COMMENT "rlsw-cc: overlay rlsw.h + span_kernels.c → ${raylib_src_dir}/src/external"
        )
        set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
            OBJECT_DEPENDS "${_hdr};${RLSW_CC_SPAN_KERNELS}"
        )
    else()
        add_custom_target(rlsw_cc_overlay
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_hdr}" "${raylib_src_dir}/src/external/rlsw.h"
            DEPENDS "${_hdr}"
            COMMENT "rlsw-cc: overlay stock rlsw.h → ${raylib_src_dir}/src/external"
        )
        set_source_files_properties("${raylib_src_dir}/src/rcore.c" PROPERTIES
            OBJECT_DEPENDS "${_hdr}"
        )
    endif()
    add_dependencies(${raylib_target} rlsw_cc_overlay)

    # Present path is part of the consumer patch (stable window blit on desktop).
    rlsw_cc_apply_soft_present(${raylib_target} "${raylib_src_dir}")
endfunction()

# -----------------------------------------------------------------------------
# Concurrent-C @parallel stripe TU (checked-in emit + vendored host-C runtime)
# Regenerated with: ./rlsw-cc/tools/gen.sh bin_par
# Runtime refresh:   ./rlsw-cc/tools/vendor-ccc.sh
# -----------------------------------------------------------------------------
if(RLSW_CC_PARALLEL AND NOT RLSW_CC_USE_STOCK)
    set(_port "${RLSW_CC_PORTABLE}")
    set(_port_inc "${_port}/include")
    set(_port_rt "${_port}/runtime/concurrent_c.c")
    if(NOT EXISTS "${_port}/CCCPORTABLE.txt" OR NOT EXISTS "${_port_rt}")
        message(FATAL_ERROR
            "rlsw-cc: missing vendor/cccportable (need CCCPORTABLE.txt + runtime/concurrent_c.c).\n"
            "  Author with ccc:  ./rlsw-cc/tools/vendor-ccc.sh\n"
            "  Or: ccc portable-install ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cccportable")
    endif()

    add_library(rlsw_cc STATIC
        "${RLSW_CC_BIN_PAR}"
        "${_port_rt}"
    )
    add_library(rlsw::cc ALIAS rlsw_cc)
    set_target_properties(rlsw_cc PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME rlsw_cc
    )
    target_include_directories(rlsw_cc PUBLIC "${_port_inc}")
    # Runtime ABI matches ccc itself; define only when compiling the runtime .c
    # (and bin_par, which is fine — same TU set).
    target_compile_definitions(rlsw_cc PRIVATE CC_ENABLE_ASYNC)
    find_package(Threads REQUIRED)
    target_link_libraries(rlsw_cc PUBLIC Threads::Threads)
    if(NOT APPLE)
        target_link_libraries(rlsw_cc PUBLIC m)
    endif()
    file(READ "${_port}/CCCPORTABLE.txt" _port_stamp)
    string(STRIP "${_port_stamp}" _port_stamp)
    message(STATUS "rlsw-cc: @parallel via ${_port} (${_port_stamp})")
else()
    add_library(rlsw_cc INTERFACE)
    add_library(rlsw::cc ALIAS rlsw_cc)
endif()

message(STATUS "rlsw-cc: header ${RLSW_CC_HEADER}")
