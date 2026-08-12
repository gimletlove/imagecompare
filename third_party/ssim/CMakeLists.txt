# Local build wrapper for the vendored rmgr::ssim 2.1.0 sources.
cmake_minimum_required(VERSION 3.21)

project(rmgr-ssim VERSION 2.1.0 LANGUAGES CXX)

set(RMGR_SSIM_COMPILE_OPTIONS)

macro(rmgr_check_predef_macros)
    include(CheckCXXSymbolExists)
    foreach(macro_name ${ARGN})
        check_cxx_symbol_exists("${macro_name}" "" "${macro_name}")
        if(${macro_name})
            break()
        endif()
    endforeach()
endmacro()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    list(APPEND RMGR_SSIM_COMPILE_OPTIONS "-Wall")
    rmgr_check_predef_macros(__amd64__ __aarch64__ __i386__ __arm__)

    if(__i386__ OR __amd64__)
        set(RMGR_SSIM_SSE_FLAGS "-msse2")
        set(RMGR_SSIM_AVX_FLAGS "-mavx")
        set(RMGR_SSIM_FMA_FLAGS "-mfma")
    elseif(__arm__)
        set(RMGR_SSIM_NEON_FLAGS "-mfpu=neon")
        check_cxx_symbol_exists("__ARM_PCS_VFP" "" __ARM_PCS_VFP)
        if(__ARM_PCS_VFP)
            list(APPEND RMGR_SSIM_NEON_FLAGS "-mfloat-abi=hard")
        else()
            list(APPEND RMGR_SSIM_NEON_FLAGS "-mfloat-abi=softfp")
        endif()
    endif()
endif()

configure_file(src/ssim-version.h.in rmgr/ssim-version.h @ONLY)

add_library(rmgr-ssim STATIC
    src/ssim.cpp
    src/ssim_sse.cpp
    src/ssim_avx.cpp
    src/ssim_fma.cpp
    src/ssim_neon.cpp
)

include(CheckCXXCompilerFlag)
foreach(implementation sse avx fma neon)
    string(TOUPPER "${implementation}" implementation_upper)
    if(RMGR_SSIM_${implementation_upper}_FLAGS)
        check_cxx_compiler_flag(
            "${RMGR_SSIM_${implementation_upper}_FLAGS}"
            RMGR_SSIM_${implementation_upper}_FLAGS_SUPPORTED
        )
        if(RMGR_SSIM_${implementation_upper}_FLAGS_SUPPORTED)
            set_source_files_properties(
                "src/ssim_${implementation}.cpp"
                PROPERTIES COMPILE_OPTIONS "${RMGR_SSIM_${implementation_upper}_FLAGS}"
            )
        endif()
    endif()
endforeach()

set_target_properties(rmgr-ssim PROPERTIES AUTOMOC OFF)
target_include_directories(rmgr-ssim
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_options(rmgr-ssim PRIVATE ${RMGR_SSIM_COMPILE_OPTIONS})
target_compile_definitions(rmgr-ssim PRIVATE RMGR_SSIM_USE_DOUBLE=0 _USE_MATH_DEFINES)
