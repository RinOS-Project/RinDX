/* SPDX-License-Identifier: MIT */
#ifndef RINDX_SRC_DXGI_NATIVE_ADAPTER_H
#define RINDX_SRC_DXGI_NATIVE_ADAPTER_H

#include <dxgi1_2.h>

#include "native_adapter_ids.h"

static int rindx_native_is_software_adapter(IUnknown* candidate) {
    IDXGIAdapter1* adapter = NULL;
    DXGI_ADAPTER_DESC1 description;
    HRESULT result;

    if (candidate == NULL) return 0;
    result = candidate->lpVtbl->QueryInterface(candidate, &IID_IDXGIAdapter1,
                                                (void**)&adapter);
    if (FAILED(result) || adapter == NULL) return 0;
    result = adapter->lpVtbl->GetDesc1(adapter, &description);
    adapter->lpVtbl->Release(adapter);
    if (FAILED(result)) return 0;
    return (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u &&
           description.VendorId == RINDX_NATIVE_SOFTWARE_ADAPTER_VENDOR_ID &&
           description.DeviceId == RINDX_NATIVE_SOFTWARE_ADAPTER_DEVICE_ID;
}

#endif /* RINDX_SRC_DXGI_NATIVE_ADAPTER_H */
