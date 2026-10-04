cmake_minimum_required(VERSION 3.16)
project(RinDX VERSION 1.0.0 LANGUAGES C)

set(RINGPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../RinGPU" CACHE PATH
    "RinGPU source directory")
if(NOT EXISTS "${RINGPU_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RinGPU is required; set RINGPU_DIR to its checkout")
endif()

if(NOT TARGET RinGPU::RinGPU)
    set(RINGPU_BUILD_TESTS OFF CACHE BOOL "Disable RinGPU tests in RinDX" FORCE)
    set(RINGPU_BUILD_DIFFERENTIAL_TESTS OFF CACHE BOOL
        "Disable RinGPU differential tests in RinDX" FORCE)
    add_subdirectory("${RINGPU_DIR}" "ringpu-build" EXCLUDE_FROM_ALL)
endif()
set(RINDX_SOURCES
    src/dxgi/catalog.c
    src/dxgi/com.c
    src/dxgi/swapchain.c
    src/d3d11/d3d11.c
    src/d3d12/d3d12.c)
if(WIN32)
    list(APPEND RINDX_SOURCES
        src/d3d11/native_d3d11.c
        src/d3d12/native_d3d12.c
        src/dxgi/native_dxgi.c)
endif()
add_library(RinDX STATIC ${RINDX_SOURCES})
add_library(RinDX::RinDX ALIAS RinDX)
target_compile_features(RinDX PUBLIC c_std_11)
target_include_directories(RinDX PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(RinDX PUBLIC RinGPU::RinGPU)
if(WIN32)
    target_link_libraries(RinDX PUBLIC uuid dxguid d3d11 d3d12 dxgi)

    # Keep the SDK-facing entry points in an actual loadable Windows module as
    # well as in the static owner.  This is the ABI boundary used by native
    # clients; the module links the same validated RinGPU/RinDX owner and does
    # not fall back to the platform D3D implementation.
    add_library(RinDXNative SHARED
        src/d3d11/native_d3d11.c
        src/d3d12/native_d3d12.c
        src/dxgi/native_dxgi.c
        src/dxgi/com.c
        src/provider_win32.c
        src/native_exports.def)
    target_compile_features(RinDXNative PRIVATE c_std_11)
    target_compile_definitions(RinDXNative PRIVATE RINDX_BUILD_DLL=1)
    target_include_directories(RinDXNative PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(RinDXNative PRIVATE RinDX::RinDX
        uuid dxguid d3d11 d3d12 dxgi user32 gdi32)
    if(MSVC)
        target_compile_options(RinDXNative PRIVATE /W4)
    else()
        target_compile_options(RinDXNative PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
if(MSVC)
    target_compile_options(RinDX PRIVATE /W4)
else()
    target_compile_options(RinDX PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    if(NOT WIN32)
        find_package(Threads REQUIRED)
    endif()
    add_executable(rindx-d3d11-runtime-test
        tests/rindx_d3d11_runtime_test.c)
    target_link_libraries(rindx-d3d11-runtime-test PRIVATE RinDX::RinDX)
    if(NOT WIN32)
        target_link_libraries(rindx-d3d11-runtime-test PRIVATE
            Threads::Threads)
    endif()
    add_test(NAME rindx-d3d11-runtime COMMAND rindx-d3d11-runtime-test)
    add_executable(rindx-d3d12-runtime-test
        tests/rindx_d3d12_runtime_test.c)
    target_link_libraries(rindx-d3d12-runtime-test PRIVATE RinDX::RinDX)
    add_test(NAME rindx-d3d12-runtime COMMAND rindx-d3d12-runtime-test)
    add_executable(rindx-d3d12-failure-test
        tests/rindx_d3d12_failure_test.c)
    target_link_libraries(rindx-d3d12-failure-test PRIVATE RinDX::RinDX)
    add_test(NAME rindx-d3d12-failure COMMAND rindx-d3d12-failure-test)
    add_executable(rindx-d3d11-failure-test
        tests/rindx_d3d11_failure_test.c)
    target_link_libraries(rindx-d3d11-failure-test PRIVATE RinDX::RinDX)
    add_test(NAME rindx-d3d11-failure COMMAND rindx-d3d11-failure-test)
    add_executable(rindx-d3d12-thread-test
        tests/rindx_d3d12_thread_test.c)
    target_link_libraries(rindx-d3d12-thread-test PRIVATE RinDX::RinDX)
    if(NOT WIN32)
        target_link_libraries(rindx-d3d12-thread-test PRIVATE Threads::Threads)
    endif()
    add_test(NAME rindx-d3d12-thread COMMAND rindx-d3d12-thread-test)
    add_executable(rindx-d3d12-lifetime-test
        tests/rindx_d3d12_lifetime_test.c)
    target_link_libraries(rindx-d3d12-lifetime-test PRIVATE RinDX::RinDX)
    add_test(NAME rindx-d3d12-lifetime COMMAND rindx-d3d12-lifetime-test)
    if(RINGPU_BUILD_SOFTWARE_BACKEND)
        add_executable(rindx-cross-backend-test
            tests/rindx_differential_test.c
            ${RINGPU_DIR}/tests/differential/differential.c)
        target_include_directories(rindx-cross-backend-test PRIVATE
            ${RINGPU_DIR}/tests/differential)
        target_link_libraries(rindx-cross-backend-test PRIVATE RinDX::RinDX)
        add_test(NAME rindx-cross-backend COMMAND rindx-cross-backend-test)
    endif()
    if(WIN32)
        add_executable(rindx-native-d3d-runtime-test
            tests/rindx_native_d3d_runtime_test.c)
        target_link_libraries(rindx-native-d3d-runtime-test PRIVATE RinDX::RinDX)
        add_test(NAME rindx-native-d3d-runtime COMMAND rindx-native-d3d-runtime-test)
        add_executable(rindx-native-provider-test
            tests/rindx_native_provider_test.c)
        target_compile_features(rindx-native-provider-test PRIVATE c_std_11)
        target_include_directories(rindx-native-provider-test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
        add_dependencies(rindx-native-provider-test RinDXNative)
        target_link_libraries(rindx-native-provider-test PRIVATE dxguid)
        add_test(NAME rindx-native-provider COMMAND rindx-native-provider-test)
    endif()
endif()
