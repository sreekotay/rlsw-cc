# rlsw-cc — Concurrent-C / optimized fork of raylib's rlsw software rasterizer.
#
# Consume from a parent that already has FetchContent raylib (Software / RGFW):
#
#   add_subdirectory(rlsw-cc)
#   rlsw_cc_overlay_raylib(raylib "${raylib_SOURCE_DIR}")
#   target_link_libraries(my_app PRIVATE raylib rlsw_cc)   # rlsw_cc if RLSW_CC_PARALLEL
#
# Overlay is required: rlgl.h does #include "external/rlsw.h" relative to raylib src/.
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
