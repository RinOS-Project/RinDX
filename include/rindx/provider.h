/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_PROVIDER_H
#define RINDX_PUBLIC_PROVIDER_H

#include <stdint.h>

#define RIN_DX_PROVIDER_VERSION 1u
#define RIN_DX_PROVIDER_MAX_MODULE_NAME 128u
#define RIN_DX_PROVIDER_MAX_EXPORT_NAME 128u

typedef enum RinDxProviderResult {
    RIN_DX_PROVIDER_OK = 0,
    RIN_DX_PROVIDER_INVALID_ARGUMENT = -1,
    RIN_DX_PROVIDER_NOT_FOUND = -2,
    RIN_DX_PROVIDER_FAILED = -3
} RinDxProviderResult;

typedef int (*RinDxResolveExportFn)(
    void* context, const char* module_name, const char* export_name,
    void** address_out);

/* RinNT owns DLL policy and calls this provider only after it has normalized
 * and policy-checked dxgi.dll, d3d11.dll, or d3d12.dll.  RinDX owns the
 * provider implementation; RinNT never embeds a DXGI/D3D implementation. */
typedef struct RinDxProviderV1 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    RinDxResolveExportFn resolve_export;
    uint64_t reserved[4];
} RinDxProviderV1;

#endif /* RINDX_PUBLIC_PROVIDER_H */
