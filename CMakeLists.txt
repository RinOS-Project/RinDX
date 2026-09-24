cmake_minimum_required(VERSION 3.16)
project(RinDX VERSION 1.0.0 LANGUAGES C)

set(RINGPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RinGPU" CACHE PATH
    "RinGPU source directory")
if(NOT EXISTS "${RINGPU_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RinGPU is required; set RINGPU_DIR to its checkout")
endif()

set(RINGPU_BUILD_TESTS OFF CACHE BOOL "Disable RinGPU tests in RinDX" FORCE)
set(RINGPU_BUILD_DIFFERENTIAL_TESTS OFF CACHE BOOL
    "Disable RinGPU differential tests in RinDX" FORCE)
add_subdirectory("${RINGPU_DIR}" "ringpu-build" EXCLUDE_FROM_ALL)
set(RINDX_SOURCES
    src/dxgi/catalog.c
    src/dxgi/com.c
    src/dxgi/swapchain.c)
add_library(RinDX STATIC ${RINDX_SOURCES})
add_library(RinDX::RinDX ALIAS RinDX)
target_compile_features(RinDX PUBLIC c_std_11)
target_include_directories(RinDX PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(RinDX PUBLIC RinGPU::RinGPU)
if(MSVC)
    target_compile_options(RinDX PRIVATE /W4)
else()
    target_compile_options(RinDX PRIVATE -Wall -Wextra -Wpedantic)
endif()
