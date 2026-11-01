# cco_switch.cmake - the recompiled tree and the SDK for the Condemned build (from ds2-nx's ds2_switch.cmake).
#
#   CCO_GEN_DIR   generated/default of tools/codegen.sh (made from YOUR default.xex; never committed)
#   REXSDK_DIR    this repository's sdk/ (rexglue-sdk-nx, with sdk-patches/ applied)
#
# The tree is compiled as a mirror of symlinks (tools/prepare_tree.py) whose PCH is the Switch one
# (tools/make_nx_pch.py: GCC's DEFINE_REX_FUNC, per-thread mtmsrd regions).

set(CCO_GEN_DIR "" CACHE PATH "generated/default of tools/codegen.sh (holds sources.cmake)")
if(NOT EXISTS "${CCO_GEN_DIR}/sources.cmake")
    message(FATAL_ERROR
        "CCO_GEN_DIR must point at the codegen output of your default.xex "
        "(no sources.cmake under '${CCO_GEN_DIR}'). Run tools/codegen.sh first.")
endif()

set(REXSDK_DIR "" CACHE PATH "Path to the rexglue-sdk-nx source tree")
if(NOT REXSDK_DIR)
    get_filename_component(REXSDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../sdk" ABSOLUTE)
endif()
add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
message(STATUS "condemned: ReXGlue SDK from ${REXSDK_DIR}")

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(CCO_TREE "${CMAKE_BINARY_DIR}/generated_nx")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/prepare_tree.py"
            "${CCO_GEN_DIR}" "${CCO_TREE}"
    RESULT_VARIABLE _cco_tree_rc)
if(NOT _cco_tree_rc EQUAL 0)
    message(FATAL_ERROR "condemned: tools/prepare_tree.py failed (${_cco_tree_rc})")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CCO_GEN_DIR}/condemned_pch.h"
    "${CCO_GEN_DIR}/sources.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../tools/prepare_tree.py"
    "${CMAKE_CURRENT_LIST_DIR}/../tools/make_nx_pch.py")

set(REXGLUE_RECOMP_DEBUG_INFO "none" CACHE STRING
    "Debug info level for generated code: line-tables-only, full, or none")
set(REXGLUE_RECOMP_OPTIONS "")
if(REXGLUE_RECOMP_DEBUG_INFO STREQUAL "none")
    list(APPEND REXGLUE_RECOMP_OPTIONS $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-g0>)
endif()

include("${CCO_TREE}/sources.cmake")
set(CCO_GUEST_SOURCES ${GENERATED_SOURCES})
set_source_files_properties(${CCO_GUEST_SOURCES} PROPERTIES COMPILE_OPTIONS "${REXGLUE_RECOMP_OPTIONS}")
# condemned_register.cpp is one function of ~92,000 SetFunction calls, run once at boot: GCC at -O3 spent over an
# hour on it. Unoptimized it builds in minutes and costs nothing at run time.
set_property(SOURCE "${CCO_TREE}/condemned_register.cpp" APPEND PROPERTY COMPILE_OPTIONS -O0)
list(LENGTH CCO_GUEST_SOURCES _cco_n)
message(STATUS "condemned: ${_cco_n} recompiled sources from ${CCO_GEN_DIR}")

# Usage: cco_setup_target(<target> [GPU_PLUGINS xenos])
macro(cco_setup_target target_name)
    add_library(${target_name}_recomp OBJECT ${CCO_GUEST_SOURCES})
    target_include_directories(${target_name}_recomp PRIVATE "${CCO_TREE}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(${target_name}_recomp PRIVATE rex::runtime)
    rexglue_apply_target_settings(${target_name}_recomp)
    target_precompile_headers(${target_name}_recomp PRIVATE "${CCO_TREE}/condemned_pch.h")
    target_compile_options(${target_name}_recomp PRIVATE ${REXGLUE_RECOMP_OPTIONS})
    target_link_libraries(${target_name} PRIVATE ${target_name}_recomp)
    target_include_directories(${target_name} PRIVATE "${CCO_TREE}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(${target_name} PRIVATE rex::runtime)
    rexglue_configure_target(${target_name} ${ARGN})
endmacro()
