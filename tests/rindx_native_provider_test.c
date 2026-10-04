/* SPDX-License-Identifier: MIT */
#if defined(_WIN32)

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include <dxgi1_2.h>

#include <rindx/provider.h>

typedef int (*RinDxInitializeNativeProviderFn)(
    void* loaded_module, RinDxProviderV1* provider_out);
typedef HRESULT(WINAPI* RinDxCreateDXGIFactory1Fn)(
    REFIID iid, void** factory_out);

typedef struct ExpectedExport {
    const char* module_name;
    const char* export_name;
} ExpectedExport;

static int bytes_are_zero(const void* memory, size_t size)
{
    const unsigned char* bytes = (const unsigned char*)memory;
    size_t index;
    if (memory == NULL) return 0;
    for (index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static int matches_proc_address(void* address, FARPROC expected)
{
    void* expected_address = NULL;
    _Static_assert(sizeof(expected_address) == sizeof(expected),
                   "Windows function and object pointer ABI drift");
    memcpy(&expected_address, &expected, sizeof(expected_address));
    return address == expected_address;
}

int main(void)
{
    static const ExpectedExport exports[] = {
        {"dxgi.dll", "CreateDXGIFactory"},
        {"dxgi.dll", "CreateDXGIFactory1"},
        {"dxgi.dll", "CreateDXGIFactory2"},
        {"d3d11.dll", "D3D11CreateDevice"},
        {"d3d11.dll", "D3D11CreateDeviceAndSwapChain"},
        {"d3d12.dll", "D3D12CreateDevice"},
    };
    WCHAR executable_path[MAX_PATH];
    WCHAR* executable_directory_end;
    DWORD executable_path_length;
    size_t executable_length;
    static const WCHAR native_library_name[] = L"RinDXNative.dll";
    HMODULE module;
    HMODULE unrelated_module = GetModuleHandleW(L"kernel32.dll");
    RinDxProviderV1 provider;
    RinDxInitializeNativeProviderFn initialize_provider;
    size_t index;
    void* address = NULL;
    FARPROC initialize_export;
    IDXGIFactory1* factory = NULL;
    executable_path_length =
        GetModuleFileNameW(NULL, executable_path, MAX_PATH);
    if (executable_path_length == 0u || executable_path_length >= MAX_PATH)
        return 1;
    executable_directory_end = wcsrchr(executable_path, L'\\');
    if (executable_directory_end == NULL) return 1;
    executable_directory_end[1] = L'\0';
    executable_length = wcslen(executable_path);
    if (executable_length +
            sizeof(native_library_name) / sizeof(native_library_name[0]) >
        MAX_PATH)
        return 1;
    memcpy(executable_path + executable_length, native_library_name,
           sizeof(native_library_name));
    module = LoadLibraryExW(executable_path, NULL,
                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module == NULL) return 1;
    initialize_export = GetProcAddress(
        module, "rindx_native_provider_v1_initialize");
    _Static_assert(sizeof(initialize_provider) == sizeof(initialize_export),
                   "Windows function and object pointer ABI drift");
    if (initialize_export == NULL) {
        FreeLibrary(module);
        return 1;
    }
    memcpy(&initialize_provider, &initialize_export,
           sizeof(initialize_provider));
    memset(&provider, 0, sizeof(provider));
    if (initialize_provider(module, &provider) !=
            RIN_DX_PROVIDER_OK ||
        provider.struct_size != sizeof(provider) ||
        provider.version != RIN_DX_PROVIDER_VERSION ||
        provider.context != module || provider.resolve_export == NULL) {
        FreeLibrary(module);
        return 2;
    }
    for (index = 0u; index < sizeof(exports) / sizeof(exports[0]); ++index) {
        FARPROC expected = GetProcAddress(module, exports[index].export_name);
        address = NULL;
        if (expected == NULL ||
            provider.resolve_export(provider.context,
                                    exports[index].module_name,
                                    exports[index].export_name, &address) !=
                RIN_DX_PROVIDER_OK ||
            !matches_proc_address(address, expected)) {
            FreeLibrary(module);
            return 3;
        }
    }
    {
        RinDxCreateDXGIFactory1Fn create_factory = NULL;
        address = NULL;
        if (provider.resolve_export(provider.context, "dxgi.dll",
                                    "CreateDXGIFactory1", &address) !=
                RIN_DX_PROVIDER_OK ||
            address == NULL) {
            FreeLibrary(module);
            return 10;
        }
        _Static_assert(sizeof(create_factory) == sizeof(address),
                       "Windows function and object pointer ABI drift");
        memcpy(&create_factory, &address, sizeof(create_factory));
        if (create_factory == NULL ||
            FAILED(create_factory(&IID_IDXGIFactory1, (void**)&factory)) ||
            factory == NULL) {
            FreeLibrary(module);
            return 10;
        }
        factory->lpVtbl->Release(factory);
        factory = NULL;
    }
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "kernel32.dll",
                                "LoadLibraryA", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL) {
        FreeLibrary(module);
        return 4;
    }
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "dxgi.dll",
                                "D3D11CreateDevice", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL) {
        FreeLibrary(module);
        return 5;
    }
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "d3d11.dll",
                                "MissingRinDXExport", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL) {
        FreeLibrary(module);
        return 6;
    }
    memset(&provider, 0xa5, sizeof(provider));
    if (initialize_provider(NULL, &provider) !=
            RIN_DX_PROVIDER_INVALID_ARGUMENT ||
        !bytes_are_zero(&provider, sizeof(provider))) {
        FreeLibrary(module);
        return 7;
    }
    memset(&provider, 0xa5, sizeof(provider));
    if (unrelated_module == NULL ||
        initialize_provider(unrelated_module, &provider) !=
            RIN_DX_PROVIDER_FAILED ||
        !bytes_are_zero(&provider, sizeof(provider))) {
        FreeLibrary(module);
        return 8;
    }
    if (!FreeLibrary(module)) return 9;
    return 0;
}

#endif /* _WIN32 */
