/* SPDX-License-Identifier: MIT */
#if defined(_WIN32)

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rindx/provider.h>

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
    HMODULE module = GetModuleHandleW(L"RinDXNative.dll");
    HMODULE unrelated_module = GetModuleHandleW(L"kernel32.dll");
    RinDxProviderV1 provider;
    size_t index;
    void* address = NULL;
    if (module == NULL) return 1;
    memset(&provider, 0, sizeof(provider));
    if (rindx_native_provider_v1_initialize(module, &provider) !=
            RIN_DX_PROVIDER_OK ||
        provider.struct_size != sizeof(provider) ||
        provider.version != RIN_DX_PROVIDER_VERSION ||
        provider.context != module || provider.resolve_export == NULL)
        return 2;
    for (index = 0u; index < sizeof(exports) / sizeof(exports[0]); ++index) {
        FARPROC expected = GetProcAddress(module, exports[index].export_name);
        address = NULL;
        if (expected == NULL ||
            provider.resolve_export(provider.context,
                                    exports[index].module_name,
                                    exports[index].export_name, &address) !=
                RIN_DX_PROVIDER_OK ||
            !matches_proc_address(address, expected))
            return 3;
    }
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "kernel32.dll",
                                "LoadLibraryA", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL)
        return 4;
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "dxgi.dll",
                                "D3D11CreateDevice", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL)
        return 5;
    address = (void*)(uintptr_t)1u;
    if (provider.resolve_export(provider.context, "d3d11.dll",
                                "MissingRinDXExport", &address) !=
            RIN_DX_PROVIDER_NOT_FOUND || address != NULL)
        return 6;
    memset(&provider, 0xa5, sizeof(provider));
    if (rindx_native_provider_v1_initialize(NULL, &provider) !=
            RIN_DX_PROVIDER_INVALID_ARGUMENT ||
        !bytes_are_zero(&provider, sizeof(provider)))
        return 7;
    memset(&provider, 0xa5, sizeof(provider));
    if (unrelated_module == NULL ||
        rindx_native_provider_v1_initialize(unrelated_module, &provider) !=
            RIN_DX_PROVIDER_FAILED ||
        !bytes_are_zero(&provider, sizeof(provider)))
        return 8;
    return 0;
}

#endif /* _WIN32 */
