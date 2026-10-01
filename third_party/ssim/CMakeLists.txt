# Local build wrapper for the vendored rmgr::ssim 2.1.0 sources.
cmake_minimum_required(VERSION 3.21)

project(rmgr-ssim VERSION 2.1.0 LANGUAGES CXX)

configure_file(src/ssim-version.h.in rmgr/ssim-version.h @ONLY)

add_library(rmgr-ssim STATIC
    src/ssim.cpp
    src/ssim_sse.cpp
    src/ssim_avx.cpp
    src/ssim_fma.cpp
    src/ssim_neon.cpp
)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__x86_64__ "" RMGR_SSIM_X86_64)
if(RMGR_SSIM_X86_64)
    set_source_files_properties(src/ssim_avx.cpp PROPERTIES COMPILE_OPTIONS -mavx)
    set_source_files_properties(src/ssim_fma.cpp PROPERTIES COMPILE_OPTIONS -mfma)
endif()

set_target_properties(rmgr-ssim PROPERTIES AUTOMOC OFF)
target_include_directories(rmgr-ssim
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_options(rmgr-ssim PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>)
target_compile_definitions(rmgr-ssim PRIVATE _USE_MATH_DEFINES)
