/* SPDX-License-Identifier: MIT */
/* Windows DXGI factory/adapter ABI over the RinDX software owner. */
#if defined(_WIN32)

#include <windows.h>
#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>
#include <d3d12.h>

#include <stdlib.h>
#include <string.h>

HRESULT rindx_native_d3d11_create_swapchain_for_device(
    ID3D11Device* device, const DXGI_SWAP_CHAIN_DESC* descriptor,
    IDXGISwapChain** swapchain_out);
HRESULT rindx_native_d3d12_create_swapchain_for_queue(
    ID3D12CommandQueue* queue, const DXGI_SWAP_CHAIN_DESC* descriptor,
    IDXGISwapChain** swapchain_out);

typedef struct NativeDxgiFactory NativeDxgiFactory;

typedef struct NativeDxgiPrivateData {
    GUID guid;
    BYTE* bytes;
    UINT size;
    IUnknown* interface_value;
    struct NativeDxgiPrivateData* next;
} NativeDxgiPrivateData;

typedef struct NativeDxgiObjectData {
    NativeDxgiPrivateData* private_data;
} NativeDxgiObjectData;

typedef struct NativeDxgiAdapter {
    IDXGIAdapter1 iface;
    LONG references;
    NativeDxgiFactory* factory;
    NativeDxgiObjectData object;
} NativeDxgiAdapter;

struct NativeDxgiFactory {
    IDXGIFactory1 iface;
    LONG references;
    HWND window;
    UINT association_flags;
    NativeDxgiObjectData object;
};

static void native_dxgi_object_destroy(NativeDxgiObjectData* object) {
    NativeDxgiPrivateData* entry;
    if (!object) return;
    entry = object->private_data;
    while (entry) {
        NativeDxgiPrivateData* next = entry->next;
        if (entry->interface_value)
            entry->interface_value->lpVtbl->Release(entry->interface_value);
        free(entry->bytes);
        free(entry);
        entry = next;
    }
    memset(object, 0, sizeof(*object));
}

static NativeDxgiPrivateData* native_dxgi_private_find(
    NativeDxgiObjectData* object, REFGUID guid) {
    NativeDxgiPrivateData* entry;
    if (!object || !guid) return NULL;
    for (entry = object->private_data; entry; entry = entry->next)
        if (IsEqualGUID(&entry->guid, guid)) return entry;
    return NULL;
}

static HRESULT native_dxgi_get_private(NativeDxgiObjectData* object,
                                       REFGUID guid, UINT* size, void* data) {
    NativeDxgiPrivateData* entry;
    UINT required;
    if (!object || !guid || !size) return E_INVALIDARG;
    entry = native_dxgi_private_find(object, guid);
    if (!entry) return DXGI_ERROR_NOT_FOUND;
    required = entry->interface_value ? sizeof(IUnknown*) : entry->size;
    if (!data || *size < required) {
        *size = required;
        return data ? DXGI_ERROR_MORE_DATA : S_OK;
    }
    if (entry->interface_value) {
        *(IUnknown**)data = entry->interface_value;
        entry->interface_value->lpVtbl->AddRef(entry->interface_value);
    } else if (required != 0u) {
        memcpy(data, entry->bytes, required);
    }
    *size = required;
    return S_OK;
}

static HRESULT native_dxgi_set_private(NativeDxgiObjectData* object,
                                       REFGUID guid, UINT size,
                                       const void* data, IUnknown* interface_value) {
    NativeDxgiPrivateData* entry;
    BYTE* copy = NULL;
    if (!object || !guid || (size != 0u && !data) ||
        (interface_value && size != sizeof(IUnknown*))) return E_INVALIDARG;
    if (size != 0u && !interface_value) {
        copy = (BYTE*)malloc(size);
        if (!copy) return E_OUTOFMEMORY;
        memcpy(copy, data, size);
    }
    entry = native_dxgi_private_find(object, guid);
    if (!entry) {
        entry = (NativeDxgiPrivateData*)calloc(1u, sizeof(*entry));
        if (!entry) {
            free(copy);
            return E_OUTOFMEMORY;
        }
        entry->guid = *guid;
        entry->next = object->private_data;
        object->private_data = entry;
    }
    if (entry->interface_value)
        entry->interface_value->lpVtbl->Release(entry->interface_value);
    free(entry->bytes);
    entry->bytes = copy;
    entry->size = interface_value ? sizeof(IUnknown*) : size;
    entry->interface_value = interface_value;
    if (interface_value) interface_value->lpVtbl->AddRef(interface_value);
    return S_OK;
}

static NativeDxgiFactory* native_factory(IDXGIFactory1* self) {
    return (NativeDxgiFactory*)(void*)self;
}
static NativeDxgiAdapter* native_adapter(IDXGIAdapter1* self) {
    return (NativeDxgiAdapter*)(void*)self;
}

static HRESULT WINAPI native_adapter_query_interface(
    IDXGIAdapter1* self, REFIID iid, void** out);
static ULONG WINAPI native_adapter_add_ref(IDXGIAdapter1* self);
static ULONG WINAPI native_adapter_release(IDXGIAdapter1* self);
static HRESULT WINAPI native_adapter_get_parent(
    IDXGIAdapter1* self, REFIID iid, void** out);
static HRESULT WINAPI native_factory_query_interface(
    IDXGIFactory1* self, REFIID iid, void** out);
static ULONG WINAPI native_factory_add_ref(IDXGIFactory1* self);
static ULONG WINAPI native_factory_release(IDXGIFactory1* self);

static HRESULT WINAPI native_adapter_set_private_data(
    IDXGIAdapter1*, REFGUID, UINT, const void*);
static HRESULT WINAPI native_adapter_set_private_interface(
    IDXGIAdapter1*, REFGUID, const IUnknown*);
static HRESULT WINAPI native_adapter_get_private_data(
    IDXGIAdapter1*, REFGUID, UINT*, void*);
static HRESULT WINAPI native_adapter_enum_outputs(
    IDXGIAdapter1*, UINT, IDXGIOutput**);
static HRESULT WINAPI native_adapter_get_desc(IDXGIAdapter1*, DXGI_ADAPTER_DESC*);
static HRESULT WINAPI native_adapter_check_interface_support(
    IDXGIAdapter1*, REFIID, LARGE_INTEGER*);
static HRESULT WINAPI native_adapter_get_desc1(
    IDXGIAdapter1*, DXGI_ADAPTER_DESC1*);
static HRESULT WINAPI native_factory_set_private_data(
    IDXGIFactory1*, REFGUID, UINT, const void*);
static HRESULT WINAPI native_factory_set_private_interface(
    IDXGIFactory1*, REFGUID, const IUnknown*);
static HRESULT WINAPI native_factory_get_private_data(
    IDXGIFactory1*, REFGUID, UINT*, void*);
static HRESULT WINAPI native_factory_get_parent(
    IDXGIFactory1*, REFIID, void**);
static HRESULT WINAPI native_factory_enum_adapters(
    IDXGIFactory1*, UINT, IDXGIAdapter**);
static HRESULT WINAPI native_factory_make_window_association(
    IDXGIFactory1*, HWND, UINT);
static HRESULT WINAPI native_factory_get_window_association(
    IDXGIFactory1*, HWND*);
static HRESULT WINAPI native_factory_create_swap_chain(
    IDXGIFactory1*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static HRESULT WINAPI native_factory_create_software_adapter(
    IDXGIFactory1*, HMODULE, IDXGIAdapter**);
static HRESULT WINAPI native_factory_enum_adapters1(
    IDXGIFactory1*, UINT, IDXGIAdapter1**);
static BOOL WINAPI native_factory_is_current(IDXGIFactory1*);

static IDXGIAdapter1Vtbl native_adapter_vtable = {
    .QueryInterface = native_adapter_query_interface,
    .AddRef = native_adapter_add_ref,
    .Release = native_adapter_release,
    .SetPrivateData = native_adapter_set_private_data,
    .SetPrivateDataInterface = native_adapter_set_private_interface,
    .GetPrivateData = native_adapter_get_private_data,
    .GetParent = native_adapter_get_parent,
    .EnumOutputs = native_adapter_enum_outputs,
    .GetDesc = native_adapter_get_desc,
    .CheckInterfaceSupport = native_adapter_check_interface_support,
    .GetDesc1 = native_adapter_get_desc1,
};

static IDXGIFactory1Vtbl native_factory_vtable = {
    .QueryInterface = native_factory_query_interface,
    .AddRef = native_factory_add_ref,
    .Release = native_factory_release,
    .SetPrivateData = native_factory_set_private_data,
    .SetPrivateDataInterface = native_factory_set_private_interface,
    .GetPrivateData = native_factory_get_private_data,
    .GetParent = native_factory_get_parent,
    .EnumAdapters = native_factory_enum_adapters,
    .MakeWindowAssociation = native_factory_make_window_association,
    .GetWindowAssociation = native_factory_get_window_association,
    .CreateSwapChain = native_factory_create_swap_chain,
    .CreateSoftwareAdapter = native_factory_create_software_adapter,
    .EnumAdapters1 = native_factory_enum_adapters1,
    .IsCurrent = native_factory_is_current,
};

static HRESULT WINAPI native_adapter_set_private_data(
    IDXGIAdapter1* self, REFGUID guid, UINT size, const void* data) {
    return native_dxgi_set_private(&native_adapter(self)->object, guid, size,
                                   data, NULL);
}
static HRESULT WINAPI native_adapter_set_private_interface(
    IDXGIAdapter1* self, REFGUID guid, const IUnknown* object) {
    return native_dxgi_set_private(&native_adapter(self)->object, guid,
                                   sizeof(IUnknown*), NULL, (IUnknown*)object);
}
static HRESULT WINAPI native_adapter_get_private_data(
    IDXGIAdapter1* self, REFGUID guid, UINT* size, void* data) {
    return native_dxgi_get_private(&native_adapter(self)->object, guid, size,
                                   data);
}
static HRESULT WINAPI native_adapter_enum_outputs(
    IDXGIAdapter1* self, UINT ordinal, IDXGIOutput** out) {
    (void)self; (void)ordinal;
    if (!out) return E_POINTER;
    *out = NULL;
    return DXGI_ERROR_NOT_FOUND;
}
static HRESULT WINAPI native_adapter_get_desc(
    IDXGIAdapter1* self, DXGI_ADAPTER_DESC* out) {
    (void)self;
    if (!out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    wcscpy_s(out->Description, ARRAYSIZE(out->Description), L"RinDX software");
    out->VendorId = 0x52494e44u;
    out->DeviceId = 0x00000001u;
    out->DedicatedSystemMemory = 256u * 1024u * 1024u;
    out->SharedSystemMemory = 256u * 1024u * 1024u;
    return S_OK;
}
static HRESULT WINAPI native_adapter_check_interface_support(
    IDXGIAdapter1* self, REFIID name, LARGE_INTEGER* version) {
    (void)self; (void)name;
    if (!version) return E_POINTER;
    version->QuadPart = 1;
    return S_OK;
}
static HRESULT WINAPI native_adapter_get_desc1(
    IDXGIAdapter1* self, DXGI_ADAPTER_DESC1* out) {
    (void)self;
    if (!out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    wcscpy_s(out->Description, ARRAYSIZE(out->Description), L"RinDX software");
    out->VendorId = 0x52494e44u;
    out->DeviceId = 0x00000001u;
    out->DedicatedSystemMemory = 256u * 1024u * 1024u;
    out->SharedSystemMemory = 256u * 1024u * 1024u;
    out->Flags = DXGI_ADAPTER_FLAG_SOFTWARE;
    return S_OK;
}

static HRESULT WINAPI native_adapter_query_interface(
    IDXGIAdapter1* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDXGIObject) ||
        IsEqualIID(iid, &IID_IDXGIAdapter) ||
        IsEqualIID(iid, &IID_IDXGIAdapter1)) {
        *out = self;
        IDXGIAdapter1_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI native_adapter_add_ref(IDXGIAdapter1* self) {
    return (ULONG)InterlockedIncrement(&native_adapter(self)->references);
}
static ULONG WINAPI native_adapter_release(IDXGIAdapter1* self) {
    NativeDxgiAdapter* adapter = native_adapter(self);
    LONG references = InterlockedDecrement(&adapter->references);
    if (references == 0) {
        native_dxgi_object_destroy(&adapter->object);
        IDXGIFactory1_Release(&adapter->factory->iface);
        free(adapter);
    }
    return (ULONG)references;
}
static HRESULT WINAPI native_adapter_get_parent(
    IDXGIAdapter1* self, REFIID iid, void** out) {
    return IDXGIFactory1_QueryInterface(&native_adapter(self)->factory->iface,
                                        iid, out);
}

static HRESULT WINAPI native_factory_set_private_data(
    IDXGIFactory1* self, REFGUID guid, UINT size, const void* data) {
    return native_dxgi_set_private(&native_factory(self)->object, guid, size,
                                   data, NULL);
}
static HRESULT WINAPI native_factory_set_private_interface(
    IDXGIFactory1* self, REFGUID guid, const IUnknown* object) {
    return native_dxgi_set_private(&native_factory(self)->object, guid,
                                   sizeof(IUnknown*), NULL, (IUnknown*)object);
}
static HRESULT WINAPI native_factory_get_private_data(
    IDXGIFactory1* self, REFGUID guid, UINT* size, void* data) {
    return native_dxgi_get_private(&native_factory(self)->object, guid, size,
                                   data);
}
static HRESULT WINAPI native_factory_get_parent(
    IDXGIFactory1* self, REFIID iid, void** out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = NULL;
    return IsEqualIID(iid, &IID_IUnknown) ? E_NOINTERFACE : E_NOINTERFACE;
}
static HRESULT WINAPI native_factory_enum_adapters1(
    IDXGIFactory1* self, UINT ordinal, IDXGIAdapter1** out) {
    NativeDxgiAdapter* adapter;
    if (!out) return E_POINTER;
    *out = NULL;
    if (ordinal != 0u) return DXGI_ERROR_NOT_FOUND;
    adapter = (NativeDxgiAdapter*)calloc(1u, sizeof(*adapter));
    if (!adapter) return E_OUTOFMEMORY;
    adapter->iface.lpVtbl = &native_adapter_vtable;
    adapter->references = 1;
    adapter->factory = native_factory(self);
    IDXGIFactory1_AddRef(self);
    *out = &adapter->iface;
    return S_OK;
}
static HRESULT WINAPI native_factory_enum_adapters(
    IDXGIFactory1* self, UINT ordinal, IDXGIAdapter** out) {
    IDXGIAdapter1* adapter = NULL;
    HRESULT result;
    if (!out) return E_POINTER;
    *out = NULL;
    result = native_factory_enum_adapters1(self, ordinal, &adapter);
    if (FAILED(result)) return result;
    *out = (IDXGIAdapter*)(void*)adapter;
    return S_OK;
}
static HRESULT WINAPI native_factory_make_window_association(
    IDXGIFactory1* self, HWND window, UINT flags) {
    NativeDxgiFactory* factory = native_factory(self);
    if (flags & ~DXGI_MWA_VALID) return E_INVALIDARG;
    factory->window = window;
    factory->association_flags = flags;
    return S_OK;
}
static HRESULT WINAPI native_factory_get_window_association(
    IDXGIFactory1* self, HWND* out) {
    if (!out) return E_POINTER;
    *out = native_factory(self)->window;
    return S_OK;
}
static HRESULT WINAPI native_factory_create_swap_chain(
    IDXGIFactory1* self, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** out) {
    ID3D11Device* d3d11_device = NULL;
    ID3D12CommandQueue* d3d12_queue = NULL;
    HRESULT result;
    (void)self;
    if (!device || !desc || !out) return E_INVALIDARG;
    *out = NULL;
    result = device->lpVtbl->QueryInterface(device, &IID_ID3D11Device,
                                             (void**)&d3d11_device);
    if (SUCCEEDED(result)) {
        result = rindx_native_d3d11_create_swapchain_for_device(
            d3d11_device, desc, out);
        ID3D11Device_Release(d3d11_device);
        return result;
    }
    result = device->lpVtbl->QueryInterface(device, &IID_ID3D12CommandQueue,
                                             (void**)&d3d12_queue);
    if (FAILED(result)) return result;
    result = rindx_native_d3d12_create_swapchain_for_queue(
        d3d12_queue, desc, out);
    ID3D12CommandQueue_Release(d3d12_queue);
    return result;
}
static HRESULT WINAPI native_factory_create_software_adapter(
    IDXGIFactory1* self, HMODULE module, IDXGIAdapter** out) {
    IDXGIAdapter1* adapter = NULL;
    HRESULT result;
    (void)module;
    if (!out) return E_POINTER;
    *out = NULL;
    result = native_factory_enum_adapters1(self, 0u, &adapter);
    if (FAILED(result)) return result;
    *out = (IDXGIAdapter*)(void*)adapter;
    return S_OK;
}
static BOOL WINAPI native_factory_is_current(IDXGIFactory1* self) {
    return native_factory(self)->references > 0 ? TRUE : FALSE;
}
static HRESULT WINAPI native_factory_query_interface(
    IDXGIFactory1* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDXGIObject) ||
        IsEqualIID(iid, &IID_IDXGIFactory) ||
        IsEqualIID(iid, &IID_IDXGIFactory1)) {
        *out = self;
        IDXGIFactory1_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI native_factory_add_ref(IDXGIFactory1* self) {
    return (ULONG)InterlockedIncrement(&native_factory(self)->references);
}
static ULONG WINAPI native_factory_release(IDXGIFactory1* self) {
    NativeDxgiFactory* factory = native_factory(self);
    LONG references = InterlockedDecrement(&factory->references);
    if (references == 0) {
        native_dxgi_object_destroy(&factory->object);
        free(factory);
    }
    return (ULONG)references;
}

HRESULT rindx_native_dxgi_create_factory(const void* iid, void** out) {
    NativeDxgiFactory* factory;
    if (!iid || !out) return E_INVALIDARG;
    *out = NULL;
    factory = (NativeDxgiFactory*)calloc(1u, sizeof(*factory));
    if (!factory) return E_OUTOFMEMORY;
    factory->iface.lpVtbl = &native_factory_vtable;
    factory->references = 1;
    if (IsEqualIID((REFIID)iid, &IID_IUnknown) ||
        IsEqualIID((REFIID)iid, &IID_IDXGIObject) ||
        IsEqualIID((REFIID)iid, &IID_IDXGIFactory) ||
        IsEqualIID((REFIID)iid, &IID_IDXGIFactory1)) {
        *out = &factory->iface;
        return S_OK;
    }
    free(factory);
    return E_NOINTERFACE;
}

HRESULT rindx_native_dxgi_create_factory2(UINT flags, const void* iid,
                                          void** out) {
    if (flags != 0u) {
        if (out) *out = NULL;
        return E_INVALIDARG;
    }
    return rindx_native_dxgi_create_factory(iid, out);
}

#endif /* _WIN32 */
