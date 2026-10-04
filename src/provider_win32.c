/* SPDX-License-Identifier: MIT */
#if defined(_WIN32)

#include <windows.h>

#include <stddef.h>
#include <string.h>

#include <rindx/provider.h>

static int provider_ascii_name_equal(const char* left, const char* right,
                                     int ignore_case)
{
    size_t index;
    if (left == NULL || right == NULL) return 0;
    for (index = 0u; index < RIN_DX_PROVIDER_MAX_MODULE_NAME; ++index) {
        unsigned char a = (unsigned char)left[index];
        unsigned char b = (unsigned char)right[index];
        if (ignore_case && a >= (unsigned char)'A' &&
            a <= (unsigned char)'Z')
            a = (unsigned char)(a + ((unsigned char)'a' - (unsigned char)'A'));
        if (ignore_case && b >= (unsigned char)'A' &&
            b <= (unsigned char)'Z')
            b = (unsigned char)(b + ((unsigned char)'a' - (unsigned char)'A'));
        if (a != b) return 0;
        if (a == 0u) return 1;
    }
    return 0;
}

static int provider_export_name_equal(const char* left, const char* right)
{
    size_t index;
    if (left == NULL || right == NULL) return 0;
    for (index = 0u; index < RIN_DX_PROVIDER_MAX_EXPORT_NAME; ++index) {
        if (left[index] != right[index]) return 0;
        if (left[index] == '\0') return 1;
    }
    return 0;
}

static int provider_export_allowed(const char* module_name,
                                   const char* export_name)
{
    if (module_name == NULL || export_name == NULL) return 0;
    if (provider_ascii_name_equal(module_name, "dxgi.dll", 1)) {
        return provider_export_name_equal(export_name, "CreateDXGIFactory") ||
               provider_export_name_equal(export_name, "CreateDXGIFactory1") ||
               provider_export_name_equal(export_name, "CreateDXGIFactory2");
    }
    if (provider_ascii_name_equal(module_name, "d3d11.dll", 1)) {
        return provider_export_name_equal(export_name, "D3D11CreateDevice") ||
               provider_export_name_equal(
                   export_name, "D3D11CreateDeviceAndSwapChain");
    }
    if (provider_ascii_name_equal(module_name, "d3d12.dll", 1))
        return provider_export_name_equal(export_name, "D3D12CreateDevice");
    return 0;
}

static int provider_resolve_export(void* context, const char* module_name,
                                   const char* export_name,
                                   void** address_out)
{
    FARPROC address;
    if (address_out != NULL) *address_out = NULL;
    if (context == NULL || module_name == NULL || export_name == NULL ||
        address_out == NULL)
        return RIN_DX_PROVIDER_INVALID_ARGUMENT;
    if (!provider_export_allowed(module_name, export_name))
        return RIN_DX_PROVIDER_NOT_FOUND;
    address = GetProcAddress((HMODULE)context, export_name);
    if (address == NULL) return RIN_DX_PROVIDER_NOT_FOUND;
    _Static_assert(sizeof(address) == sizeof(*address_out),
                   "Windows function and object pointer ABI drift");
    memcpy(address_out, &address, sizeof(address));
    return RIN_DX_PROVIDER_OK;
}

int rindx_native_provider_v1_initialize(void* loaded_module,
                                        RinDxProviderV1* provider_out)
{
    static const char* required_exports[] = {
        "rindx_native_provider_v1_initialize",
        "D3D11CreateDevice",
        "D3D11CreateDeviceAndSwapChain",
        "D3D12CreateDevice",
        "CreateDXGIFactory",
        "CreateDXGIFactory1",
        "CreateDXGIFactory2",
    };
    HMODULE module = (HMODULE)loaded_module;
    size_t index;
    if (provider_out == NULL) return RIN_DX_PROVIDER_INVALID_ARGUMENT;
    memset(provider_out, 0, sizeof(*provider_out));
    if (module == NULL) return RIN_DX_PROVIDER_INVALID_ARGUMENT;
    for (index = 0u;
         index < sizeof(required_exports) / sizeof(required_exports[0]);
         ++index) {
        if (GetProcAddress(module, required_exports[index]) == NULL)
            return RIN_DX_PROVIDER_FAILED;
    }
    provider_out->struct_size = sizeof(*provider_out);
    provider_out->version = RIN_DX_PROVIDER_VERSION;
    provider_out->context = module;
    provider_out->resolve_export = provider_resolve_export;
    return RIN_DX_PROVIDER_OK;
}

#endif /* _WIN32 */
