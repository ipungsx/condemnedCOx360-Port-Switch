# condemned - Condemned: Criminal Origins (Xbox 360) on the Nintendo Switch, as a homebrew NRO.
#
# Modelled on ds2-nx (https://github.com/VasilkovND/ds2-nx). Unlike Dead Space 2, no recompiled tree of this game
# exists anywhere: tools/codegen.sh generates it from YOUR default.xex with ReXGlue v0.10.0 (recomp/).
#
# Configure (Switch; tools/build_linux.sh does all of this):
#   cmake -S condemned -B condemned/out/sw -G Ninja -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_TOOLCHAIN_FILE=tools/switch/cmake/switch-devkitA64.cmake \
#     -DREXSDK_DIR=$PWD/sdk -DCCO_GEN_DIR=$PWD/condemned/recomp/generated/default -DREXGLUE_SWITCH_DEKO3D=ON
#   cmake --build condemned/out/sw

cmake_minimum_required(VERSION 3.25)
project(condemned LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/cco_switch.cmake)

set(CCO_SOURCES
    src/main.cpp
    src/cco_cpu.cpp
    src/cco_shader_dump.cpp
    src/cco_shaders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ds_shader_library.cpp
)
if(REXGLUE_SWITCH_DEKO3D)
    list(APPEND CCO_SOURCES src/cco_gpu.cpp)
endif()

add_executable(condemned ${CCO_SOURCES})

if(REXGLUE_SWITCH_DEKO3D)
    cco_setup_target(condemned)
    rexglue_switch_add_deko3d(condemned)
    target_compile_definitions(condemned PRIVATE CCO_DEKO3D=1)
else()
    cco_setup_target(condemned GPU_PLUGINS xenos)
endif()

target_include_directories(condemned PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shaders
                           ${REXSDK_DIR}/thirdparty/xxHash)

set(CCO_VERSION "0.1.2")
execute_process(COMMAND git rev-parse --short=10 HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE CCO_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT CCO_COMMIT)
    set(CCO_COMMIT "unknown")
endif()
target_compile_definitions(condemned PRIVATE CCO_VERSION="${CCO_VERSION}" CCO_COMMIT="${CCO_COMMIT}")

# Debug modes (ZL + ZR shortcuts, telemetria.flag) are ON while the port is being brought up.
option(CCO_DEPURACION "Debug modes: ZL + ZR shortcuts, telemetria.flag" ON)
target_compile_definitions(condemned PRIVATE CCO_DEPURACION=$<BOOL:${CCO_DEPURACION}>)

if(REXGLUE_PLATFORM_SWITCH)
    rexglue_switch_add_nro(condemned
        NAME "Condemned: Criminal Origins"
        AUTHOR "condemned-nx"
        VERSION "${CCO_VERSION}"
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.jpg")
endif()
