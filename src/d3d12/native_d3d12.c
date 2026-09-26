/* SPDX-License-Identifier: MIT */
/* Windows native ABI entry point for the RinDX D3D12 software owner. */
#if defined(_WIN32)

#include <windows.h>
#define COBJMACROS
#include <dxgi.h>
#include <d3d12.h>

#include <rindx/d3d12.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct NativeD3d12PrivateData {
    GUID guid;
    BYTE* bytes;
    UINT size;
    IUnknown* interface_value;
    struct NativeD3d12PrivateData* next;
} NativeD3d12PrivateData;

typedef struct NativeD3d12ObjectData {
    NativeD3d12PrivateData* private_data;
    WCHAR* name;
} NativeD3d12ObjectData;

typedef struct NativeD3d12Device NativeD3d12Device;
typedef struct NativeD3d12Queue NativeD3d12Queue;
typedef struct NativeD3d12Allocator NativeD3d12Allocator;
typedef struct NativeD3d12List NativeD3d12List;
typedef struct NativeD3d12Fence NativeD3d12Fence;
typedef struct NativeD3d12Resource NativeD3d12Resource;
typedef struct NativeD3d12DescriptorHeap NativeD3d12DescriptorHeap;
typedef struct NativeD3d12Pipeline NativeD3d12Pipeline;
typedef struct NativeD3d12Swapchain NativeD3d12Swapchain;
typedef struct NativeD3d12Blob NativeD3d12Blob;

struct NativeD3d12Device {
    ID3D12Device iface;
    LONG references;
    NativeD3d12ObjectData object;
    RinDxD3d12Device core;
};

struct NativeD3d12Queue {
    ID3D12CommandQueue iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    D3D12_COMMAND_QUEUE_DESC desc;
    int last_result;
};

struct NativeD3d12Allocator {
    ID3D12CommandAllocator iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    RinDxD3d12CommandAllocator core;
};

struct NativeD3d12List {
    ID3D12GraphicsCommandList iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    NativeD3d12Allocator* allocator;
    RinDxD3d12CommandList core;
    D3D12_COMMAND_LIST_TYPE type;
    NativeD3d12Resource* color_target;
    NativeD3d12Resource* depth_target;
    NativeD3d12Resource* vertex_buffer;
    NativeD3d12Resource* index_buffer;
    NativeD3d12Pipeline* pipeline;
    UINT vertex_stride;
    UINT vertex_offset;
    UINT index_offset;
    DXGI_FORMAT index_format;
    D3D12_PRIMITIVE_TOPOLOGY topology;
    int last_result;
};

struct NativeD3d12Fence {
    ID3D12Fence iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    RinGpuHandle handle;
};

struct NativeD3d12Resource {
    ID3D12Resource iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    RinGpuHandle handle;
    D3D12_RESOURCE_DESC desc;
    D3D12_HEAP_TYPE heap_type;
    D3D12_RESOURCE_STATES state;
    BOOL rin_present_state;
    RinDxD3d12MappedResource mapped;
};

typedef struct NativeD3d12Descriptor {
    NativeD3d12Resource* resource;
    uint32_t kind;
} NativeD3d12Descriptor;

struct NativeD3d12DescriptorHeap {
    ID3D12DescriptorHeap iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    SIZE_T cpu_base;
    SIZE_T gpu_base;
    NativeD3d12Descriptor* descriptors;
};

struct NativeD3d12Pipeline {
    ID3D12PipelineState iface;
    LONG references;
    NativeD3d12ObjectData object;
    NativeD3d12Device* device;
    RinGpuHandle handle;
    RinGpuHandle bind_group;
    BOOL compute;
};

struct NativeD3d12Swapchain {
    IDXGISwapChain iface;
    LONG references;
    NativeD3d12Device* device;
    NativeD3d12Queue* queue;
    DXGI_SWAP_CHAIN_DESC desc;
    NativeD3d12Resource* buffers[RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS];
    UINT buffer_count;
    UINT current_buffer;
    UINT present_count;
    NativeD3d12ObjectData object;
};

struct NativeD3d12Blob {
    ID3DBlob iface;
    LONG references;
    BYTE* bytes;
    SIZE_T size;
};

#define NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS 64u
static NativeD3d12DescriptorHeap*
    native_descriptor_heaps[NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS];

static ID3D12CommandQueueVtbl native_queue_vtable;
static ID3D12CommandAllocatorVtbl native_allocator_vtable;
static ID3D12GraphicsCommandListVtbl native_list_vtable;
static ID3D12FenceVtbl native_fence_vtable;
static ID3D12ResourceVtbl native_resource_vtable;
static ID3D12DescriptorHeapVtbl native_descriptor_heap_vtable;
static ID3D12PipelineStateVtbl native_pipeline_vtable;
static IDXGISwapChainVtbl native_d3d12_swapchain_vtable;
static ID3D10BlobVtbl native_blob_vtable;

static NativeD3d12Device* native_device(ID3D12Device* self) {
    return (NativeD3d12Device*)(void*)self;
}

static NativeD3d12Queue* native_queue(ID3D12CommandQueue* self) {
    return (NativeD3d12Queue*)(void*)self;
}

static NativeD3d12Allocator* native_allocator(ID3D12CommandAllocator* self) {
    return (NativeD3d12Allocator*)(void*)self;
}

static NativeD3d12List* native_list(ID3D12GraphicsCommandList* self) {
    return (NativeD3d12List*)(void*)self;
}

static NativeD3d12Fence* native_fence(ID3D12Fence* self) {
    return (NativeD3d12Fence*)(void*)self;
}

static NativeD3d12Pipeline* native_pipeline(ID3D12PipelineState* self) {
    return (NativeD3d12Pipeline*)(void*)self;
}

static NativeD3d12Blob* native_blob(ID3DBlob* self) {
    return (NativeD3d12Blob*)(void*)self;
}

static void native_object_data_destroy(NativeD3d12ObjectData* object) {
    NativeD3d12PrivateData* entry;
    if (!object) return;
    entry = object->private_data;
    while (entry) {
        NativeD3d12PrivateData* next = entry->next;
        if (entry->interface_value)
            entry->interface_value->lpVtbl->Release(entry->interface_value);
        free(entry->bytes);
        free(entry);
        entry = next;
    }
    free(object->name);
    memset(object, 0, sizeof(*object));
}

static NativeD3d12PrivateData* native_private_find(
    NativeD3d12ObjectData* object, REFGUID guid) {
    NativeD3d12PrivateData* entry;
    if (!object || !guid) return NULL;
    for (entry = object->private_data; entry; entry = entry->next)
        if (IsEqualGUID(&entry->guid, guid)) return entry;
    return NULL;
}

static HRESULT native_get_private(NativeD3d12ObjectData* object,
                                  REFGUID guid, UINT* size, void* data) {
    NativeD3d12PrivateData* entry;
    UINT required;
    if (!size || !guid) return E_INVALIDARG;
    entry = native_private_find(object, guid);
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

static HRESULT native_set_private(NativeD3d12ObjectData* object,
                                  REFGUID guid, UINT size, const void* data,
                                  IUnknown* interface_value) {
    NativeD3d12PrivateData* entry;
    BYTE* copy = NULL;
    if (!object || !guid || (size != 0u && !data) ||
        (interface_value && size != sizeof(IUnknown*)))
        return E_INVALIDARG;
    if (size != 0u && !interface_value) {
        copy = (BYTE*)malloc(size);
        if (!copy) return E_OUTOFMEMORY;
        memcpy(copy, data, size);
    }
    entry = native_private_find(object, guid);
    if (!entry) {
        entry = (NativeD3d12PrivateData*)calloc(1u, sizeof(*entry));
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

static HRESULT native_set_name(NativeD3d12ObjectData* object, LPCWSTR name) {
    size_t length;
    WCHAR* copy;
    if (!object || !name) return E_INVALIDARG;
    length = wcslen(name) + 1u;
    if (length > SIZE_MAX / sizeof(WCHAR)) return E_OUTOFMEMORY;
    copy = (WCHAR*)malloc(length * sizeof(WCHAR));
    if (!copy) return E_OUTOFMEMORY;
    memcpy(copy, name, length * sizeof(WCHAR));
    free(object->name);
    object->name = copy;
    return S_OK;
}

static HRESULT native_get_device(NativeD3d12Device* device, REFIID iid,
                                 void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) &&
        !IsEqualIID(iid, &IID_ID3D12Device)) return E_NOINTERFACE;
    *out = &device->iface;
    ID3D12Device_AddRef(&device->iface);
    return S_OK;
}

static HRESULT native_result(int result) {
    if (result == RIN_GPU_OK) return S_OK;
    if (result == RIN_GPU_ERROR_INVALID_ARGUMENT ||
        result == RIN_GPU_ERROR_BOUNDS) return E_INVALIDARG;
    if (result == RIN_GPU_ERROR_DEVICE_LOST) return DXGI_ERROR_DEVICE_REMOVED;
    if (result == RIN_GPU_ERROR_UNSUPPORTED) return E_NOTIMPL;
    return E_FAIL;
}

static uint32_t native_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8_UNORM: return RIN_GPU_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return RIN_GPU_FORMAT_RGBA8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return RIN_GPU_FORMAT_BGRA8_UNORM;
    case DXGI_FORMAT_D32_FLOAT: return RIN_GPU_FORMAT_D32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return RIN_GPU_FORMAT_RGBA16_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return RIN_GPU_FORMAT_RGBA32_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return RIN_GPU_FORMAT_RGBA8_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return RIN_GPU_FORMAT_BGRA8_SRGB;
    default: return 0u;
    }
}

static uint32_t native_vertex_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R32_UINT: return RIN_GPU_VERTEX_UINT32;
    case DXGI_FORMAT_R32_SINT: return RIN_GPU_VERTEX_SINT32;
    case DXGI_FORMAT_R32_FLOAT: return RIN_GPU_VERTEX_FLOAT32;
    case DXGI_FORMAT_R8_UINT: return RIN_GPU_VERTEX_UINT8;
    case DXGI_FORMAT_R8_SINT: return RIN_GPU_VERTEX_SINT8;
    case DXGI_FORMAT_R8_UNORM: return RIN_GPU_VERTEX_UNORM8;
    case DXGI_FORMAT_R8_SNORM: return RIN_GPU_VERTEX_SNORM8;
    case DXGI_FORMAT_R16_UINT: return RIN_GPU_VERTEX_UINT16;
    case DXGI_FORMAT_R16_SINT: return RIN_GPU_VERTEX_SINT16;
    case DXGI_FORMAT_R16_UNORM: return RIN_GPU_VERTEX_UNORM16;
    case DXGI_FORMAT_R16_SNORM: return RIN_GPU_VERTEX_SNORM16;
    default: return 0u;
    }
}

static uint32_t native_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    switch (type) {
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:
        return RIN_GPU_PRIMITIVE_POINT_LIST;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:
        return RIN_GPU_PRIMITIVE_LINE_LIST;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
        return RIN_GPU_PRIMITIVE_TRIANGLE_LIST;
    default: return 0u;
    }
}

static uint32_t native_compare(D3D12_COMPARISON_FUNC value) {
    switch (value) {
    case D3D12_COMPARISON_FUNC_LESS: return RIN_GPU_COMPARE_LESS;
    case D3D12_COMPARISON_FUNC_LESS_EQUAL:
        return RIN_GPU_COMPARE_LESS_EQUAL;
    case D3D12_COMPARISON_FUNC_ALWAYS: return RIN_GPU_COMPARE_ALWAYS;
    case D3D12_COMPARISON_FUNC_NEVER: return RIN_GPU_COMPARE_NEVER;
    case D3D12_COMPARISON_FUNC_EQUAL: return RIN_GPU_COMPARE_EQUAL;
    case D3D12_COMPARISON_FUNC_GREATER: return RIN_GPU_COMPARE_GREATER;
    case D3D12_COMPARISON_FUNC_NOT_EQUAL:
        return RIN_GPU_COMPARE_NOT_EQUAL;
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL:
        return RIN_GPU_COMPARE_GREATER_EQUAL;
    default: return 0u;
    }
}

static uint32_t native_blend_factor(D3D12_BLEND value) {
    switch (value) {
    case D3D12_BLEND_ZERO: return RIN_GPU_BLEND_ZERO;
    case D3D12_BLEND_ONE: return RIN_GPU_BLEND_ONE;
    case D3D12_BLEND_SRC_ALPHA: return RIN_GPU_BLEND_SOURCE_ALPHA;
    case D3D12_BLEND_INV_SRC_ALPHA:
        return RIN_GPU_BLEND_ONE_MINUS_SOURCE_ALPHA;
    case D3D12_BLEND_DEST_ALPHA: return RIN_GPU_BLEND_DESTINATION_ALPHA;
    case D3D12_BLEND_INV_DEST_ALPHA:
        return RIN_GPU_BLEND_ONE_MINUS_DESTINATION_ALPHA;
    case D3D12_BLEND_SRC_COLOR: return RIN_GPU_BLEND_SOURCE_COLOR;
    case D3D12_BLEND_INV_SRC_COLOR:
        return RIN_GPU_BLEND_ONE_MINUS_SOURCE_COLOR;
    case D3D12_BLEND_DEST_COLOR: return RIN_GPU_BLEND_DESTINATION_COLOR;
    case D3D12_BLEND_INV_DEST_COLOR:
        return RIN_GPU_BLEND_ONE_MINUS_DESTINATION_COLOR;
    case D3D12_BLEND_SRC_ALPHA_SAT:
        return RIN_GPU_BLEND_SOURCE_ALPHA_SATURATE;
    case D3D12_BLEND_BLEND_FACTOR: return RIN_GPU_BLEND_CONSTANT_COLOR;
    case D3D12_BLEND_INV_BLEND_FACTOR:
        return RIN_GPU_BLEND_ONE_MINUS_CONSTANT_COLOR;
    case D3D12_BLEND_SRC1_ALPHA:
    case D3D12_BLEND_INV_SRC1_ALPHA:
    case D3D12_BLEND_SRC1_COLOR:
    case D3D12_BLEND_INV_SRC1_COLOR:
    default: return 0u;
    }
}

static uint32_t native_blend_op(D3D12_BLEND_OP value) {
    switch (value) {
    case D3D12_BLEND_OP_ADD: return RIN_GPU_BLEND_ADD;
    case D3D12_BLEND_OP_SUBTRACT: return RIN_GPU_BLEND_SUBTRACT;
    case D3D12_BLEND_OP_REV_SUBTRACT:
        return RIN_GPU_BLEND_REVERSE_SUBTRACT;
    case D3D12_BLEND_OP_MIN: return RIN_GPU_BLEND_MINIMUM;
    case D3D12_BLEND_OP_MAX: return RIN_GPU_BLEND_MAXIMUM;
    default: return 0u;
    }
}

static uint32_t native_image_usage(const D3D12_RESOURCE_DESC* desc) {
    uint32_t usage = RIN_GPU_IMAGE_COPY_SOURCE |
                     RIN_GPU_IMAGE_COPY_DESTINATION |
                     RIN_GPU_IMAGE_SAMPLED;
    if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        usage |= RIN_GPU_IMAGE_COLOR_TARGET;
    if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        usage |= RIN_GPU_IMAGE_DEPTH_STENCIL;
    if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        usage |= RIN_GPU_IMAGE_STORAGE;
    return usage;
}

static uint32_t native_resource_state(D3D12_RESOURCE_STATES state) {
    if (state & D3D12_RESOURCE_STATE_PRESENT)
        return RIN_GPU_IMAGE_STATE_PRESENT;
    if (state & D3D12_RESOURCE_STATE_RENDER_TARGET)
        return RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    if (state & D3D12_RESOURCE_STATE_DEPTH_WRITE)
        return RIN_GPU_IMAGE_STATE_DEPTH_TARGET;
    if (state & D3D12_RESOURCE_STATE_COPY_SOURCE)
        return RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    if (state & D3D12_RESOURCE_STATE_COPY_DEST)
        return RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    if (state & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
        return RIN_GPU_IMAGE_STATE_SHADER_READ;
    return RIN_GPU_IMAGE_STATE_UNDEFINED;
}

static uint32_t native_resource_target_state(D3D12_RESOURCE_STATES state) {
    /* PRESENT is the zero-valued D3D12 COMMON alias; it still has a distinct
     * RinGPU state so queue validation can enforce the present contract. */
    if (state == D3D12_RESOURCE_STATE_PRESENT)
        return RIN_GPU_IMAGE_STATE_PRESENT;
    return native_resource_state(state);
}

static HRESULT native_resource_query(ID3D12Resource* self, REFIID iid,
                                     void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) ||
        IsEqualIID(iid, &IID_ID3D12Resource)) {
        *out = self;
        ID3D12Resource_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_resource_add_ref(ID3D12Resource* self) {
    return (ULONG)InterlockedIncrement(&((NativeD3d12Resource*)(void*)self)->references);
}
static ULONG WINAPI native_resource_release(ID3D12Resource* self) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    LONG references = InterlockedDecrement(&resource->references);
    if (references == 0) {
        if (resource->mapped.data)
            (void)rindx_d3d12_unmap_buffer(&resource->device->core,
                                           resource->handle, &resource->mapped);
        (void)rindx_d3d12_destroy_object(&resource->device->core,
                                         resource->handle);
        ID3D12Device_Release(&resource->device->iface);
        native_object_data_destroy(&resource->object);
        free(resource);
    }
    return (ULONG)references;
}

static HRESULT WINAPI native_resource_get_private_data(
    ID3D12Resource* self, REFGUID guid, UINT* size, void* data) {
    return native_get_private(&((NativeD3d12Resource*)(void*)self)->object,
                              guid, size, data);
}
static HRESULT WINAPI native_resource_set_private_data(
    ID3D12Resource* self, REFGUID guid, UINT size, const void* data) {
    return native_set_private(&((NativeD3d12Resource*)(void*)self)->object,
                              guid, size, data, NULL);
}
static HRESULT WINAPI native_resource_set_private_interface(
    ID3D12Resource* self, REFGUID guid, const IUnknown* data) {
    return native_set_private(&((NativeD3d12Resource*)(void*)self)->object,
                              guid, sizeof(IUnknown*), NULL, (IUnknown*)data);
}
static HRESULT WINAPI native_resource_set_name(ID3D12Resource* self,
                                               LPCWSTR name) {
    return native_set_name(&((NativeD3d12Resource*)(void*)self)->object, name);
}
static HRESULT WINAPI native_resource_get_device(ID3D12Resource* self,
                                                 REFIID iid, void** out) {
    return native_get_device(((NativeD3d12Resource*)(void*)self)->device, iid,
                             out);
}
static HRESULT WINAPI native_resource_map(ID3D12Resource* self, UINT subresource,
                                          const D3D12_RANGE* read_range,
                                          void** data) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    uint32_t map_type;
    int result;
    if (!data || subresource != 0u || resource->desc.Dimension !=
            D3D12_RESOURCE_DIMENSION_BUFFER || resource->mapped.data)
        return E_INVALIDARG;
    if (read_range && read_range->End < read_range->Begin) return E_INVALIDARG;
    if (resource->heap_type == D3D12_HEAP_TYPE_READBACK)
        map_type = RIN_DX_D3D12_MAP_READ;
    else if (resource->heap_type == D3D12_HEAP_TYPE_UPLOAD)
        map_type = RIN_DX_D3D12_MAP_WRITE;
    else
        return E_INVALIDARG;
    memset(&resource->mapped, 0, sizeof(resource->mapped));
    resource->mapped.struct_size = sizeof(resource->mapped);
    resource->mapped.version = RIN_DX_D3D12_VERSION;
    result = rindx_d3d12_map_buffer(&resource->device->core, resource->handle,
                                    map_type, 0u, &resource->mapped);
    if (result != RIN_GPU_OK) return native_result(result);
    *data = resource->mapped.data;
    return S_OK;
}
static void WINAPI native_resource_unmap(ID3D12Resource* self, UINT subresource,
                                         const D3D12_RANGE* written_range) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    (void)written_range;
    if (subresource == 0u && resource->mapped.data)
        (void)rindx_d3d12_unmap_buffer(&resource->device->core,
                                       resource->handle, &resource->mapped);
}
static D3D12_RESOURCE_DESC* WINAPI native_resource_get_desc(
    ID3D12Resource* self, D3D12_RESOURCE_DESC* out) {
    if (!out) return NULL;
    *out = ((NativeD3d12Resource*)(void*)self)->desc;
    return out;
}
static D3D12_GPU_VIRTUAL_ADDRESS WINAPI native_resource_gpu_address(
    ID3D12Resource* self) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    return resource->desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
               ? (D3D12_GPU_VIRTUAL_ADDRESS)(uintptr_t)resource : 0u;
}
static HRESULT WINAPI native_resource_write_subresource(
    ID3D12Resource* self, UINT subresource, const D3D12_BOX* dst_box,
    const void* source, UINT row_pitch, UINT depth_pitch) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    if (!source || subresource != 0u) return E_INVALIDARG;
    if (resource->desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        UINT64 offset = dst_box ? dst_box->left : 0u;
        UINT64 size = dst_box ? (dst_box->right - dst_box->left)
                              : resource->desc.Width;
        return native_result(rindx_d3d12_upload_buffer(
            &resource->device->core, resource->handle, offset, source, size));
    }
    if (resource->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        return E_INVALIDARG;
    {
        RinGpuImageUploadV1 upload;
        uint32_t width = dst_box ? dst_box->right - dst_box->left
                                 : (uint32_t)resource->desc.Width;
        uint32_t height = dst_box ? dst_box->bottom - dst_box->top
                                  : resource->desc.Height;
        uint32_t depth = dst_box ? dst_box->back - dst_box->front : 1u;
        uint64_t slice = depth_pitch ? depth_pitch : (uint64_t)row_pitch * height;
        memset(&upload, 0, sizeof(upload));
        upload.abi_version = RIN_GPU_ABI_VERSION;
        upload.struct_size = sizeof(upload);
        upload.x = dst_box ? dst_box->left : 0u;
        upload.y = dst_box ? dst_box->top : 0u;
        upload.z = dst_box ? dst_box->front : 0u;
        upload.width = width;
        upload.height = height;
        upload.depth = depth;
        upload.source_row_pitch_bytes = row_pitch;
        upload.source_slice_pitch_bytes = slice;
        return native_result(rindx_d3d12_upload_image(
            &resource->device->core, resource->handle, &upload, source, slice * depth));
    }
}
static HRESULT WINAPI native_resource_read_subresource(
    ID3D12Resource* self, void* destination, UINT dst_row_pitch,
    UINT dst_depth_pitch, UINT subresource, const D3D12_BOX* src_box) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    if (!destination || subresource != 0u ||
        resource->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        return E_INVALIDARG;
    {
        RinGpuImageReadbackV1 readback;
        uint32_t width = src_box ? src_box->right - src_box->left
                                 : (uint32_t)resource->desc.Width;
        uint32_t height = src_box ? src_box->bottom - src_box->top
                                  : resource->desc.Height;
        uint64_t slice = dst_depth_pitch ? dst_depth_pitch
                                         : (uint64_t)dst_row_pitch * height;
        uint64_t size = slice;
        memset(&readback, 0, sizeof(readback));
        readback.abi_version = RIN_GPU_ABI_VERSION;
        readback.struct_size = sizeof(readback);
        readback.x = src_box ? src_box->left : 0u;
        readback.y = src_box ? src_box->top : 0u;
        readback.width = width;
        readback.height = height;
        readback.depth = 1u;
        readback.destination_row_pitch_bytes = dst_row_pitch;
        readback.destination_slice_pitch_bytes = slice;
        return native_result(rindx_d3d12_readback_image(
            &resource->device->core, resource->handle, &readback,
            destination, size));
    }
}
static HRESULT WINAPI native_resource_get_heap_properties(
    ID3D12Resource* self, D3D12_HEAP_PROPERTIES* properties,
    D3D12_HEAP_FLAGS* flags) {
    NativeD3d12Resource* resource = (NativeD3d12Resource*)(void*)self;
    if (!properties && !flags) return E_INVALIDARG;
    if (properties) {
        memset(properties, 0, sizeof(*properties));
        properties->Type = resource->heap_type;
        properties->CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        properties->MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        properties->CreationNodeMask = 1u;
        properties->VisibleNodeMask = 1u;
    }
    if (flags) *flags = D3D12_HEAP_FLAG_NONE;
    return S_OK;
}

static HRESULT WINAPI native_descriptor_heap_query(
    ID3D12DescriptorHeap* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) ||
        IsEqualIID(iid, &IID_ID3D12DescriptorHeap)) {
        *out = self;
        ID3D12DescriptorHeap_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI native_descriptor_heap_add_ref(ID3D12DescriptorHeap* self) {
    return (ULONG)InterlockedIncrement(
        &((NativeD3d12DescriptorHeap*)(void*)self)->references);
}
static ULONG WINAPI native_descriptor_heap_release(ID3D12DescriptorHeap* self) {
    NativeD3d12DescriptorHeap* heap =
        (NativeD3d12DescriptorHeap*)(void*)self;
    LONG references = InterlockedDecrement(&heap->references);
    if (references == 0) {
        UINT index;
        for (index = 0u; index < heap->desc.NumDescriptors; ++index)
            if (heap->descriptors[index].resource)
                ID3D12Resource_Release(
                    &heap->descriptors[index].resource->iface);
        for (index = 0u; index < NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS; ++index)
            if (native_descriptor_heaps[index] == heap)
                native_descriptor_heaps[index] = NULL;
        free(heap->descriptors);
        ID3D12Device_Release(&heap->device->iface);
        native_object_data_destroy(&heap->object);
        free(heap);
    }
    return (ULONG)references;
}
static HRESULT WINAPI native_descriptor_heap_get_private_data(
    ID3D12DescriptorHeap* self, REFGUID guid, UINT* size, void* data) {
    return native_get_private(
        &((NativeD3d12DescriptorHeap*)(void*)self)->object, guid, size, data);
}
static HRESULT WINAPI native_descriptor_heap_set_private_data(
    ID3D12DescriptorHeap* self, REFGUID guid, UINT size, const void* data) {
    return native_set_private(
        &((NativeD3d12DescriptorHeap*)(void*)self)->object, guid, size, data,
        NULL);
}
static HRESULT WINAPI native_descriptor_heap_set_private_interface(
    ID3D12DescriptorHeap* self, REFGUID guid, const IUnknown* data) {
    return native_set_private(
        &((NativeD3d12DescriptorHeap*)(void*)self)->object, guid,
        sizeof(IUnknown*), NULL, (IUnknown*)data);
}
static HRESULT WINAPI native_descriptor_heap_set_name(
    ID3D12DescriptorHeap* self, LPCWSTR name) {
    return native_set_name(
        &((NativeD3d12DescriptorHeap*)(void*)self)->object, name);
}
static HRESULT WINAPI native_descriptor_heap_get_device(
    ID3D12DescriptorHeap* self, REFIID iid, void** out) {
    return native_get_device(
        ((NativeD3d12DescriptorHeap*)(void*)self)->device, iid, out);
}
static D3D12_DESCRIPTOR_HEAP_DESC* WINAPI native_descriptor_heap_get_desc(
    ID3D12DescriptorHeap* self, D3D12_DESCRIPTOR_HEAP_DESC* out) {
    if (!out) return NULL;
    *out = ((NativeD3d12DescriptorHeap*)(void*)self)->desc;
    return out;
}
static D3D12_CPU_DESCRIPTOR_HANDLE* WINAPI native_descriptor_heap_get_cpu_start(
    ID3D12DescriptorHeap* self, D3D12_CPU_DESCRIPTOR_HANDLE* out) {
    if (!out) return NULL;
    out->ptr = ((NativeD3d12DescriptorHeap*)(void*)self)->cpu_base;
    return out;
}
static D3D12_GPU_DESCRIPTOR_HANDLE* WINAPI native_descriptor_heap_get_gpu_start(
    ID3D12DescriptorHeap* self, D3D12_GPU_DESCRIPTOR_HANDLE* out) {
    if (!out) return NULL;
    out->ptr = ((NativeD3d12DescriptorHeap*)(void*)self)->gpu_base;
    return out;
}

static NativeD3d12Descriptor* native_descriptor_resolve(
    D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t kind) {
    NativeD3d12DescriptorHeap* heap = NULL;
    SIZE_T delta;
    SIZE_T index;
    uint32_t heap_index;
    for (heap_index = 0u; heap_index < NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS;
         ++heap_index) {
        NativeD3d12DescriptorHeap* candidate =
            native_descriptor_heaps[heap_index];
        if (candidate && handle.ptr >= candidate->cpu_base &&
            handle.ptr - candidate->cpu_base <
                (SIZE_T)candidate->desc.NumDescriptors * 32u) {
            heap = candidate;
            break;
        }
    }
    if (!heap || heap->desc.NumDescriptors == 0u)
        return NULL;
    delta = handle.ptr - heap->cpu_base;
    if (delta % 32u != 0u) return NULL;
    index = delta / 32u;
    if (index >= heap->desc.NumDescriptors) return NULL;
    if (kind != 0u && heap->descriptors[index].kind != kind) return NULL;
    return &heap->descriptors[index];
}

static HRESULT WINAPI native_device_create_committed_resource(
    ID3D12Device* self, const D3D12_HEAP_PROPERTIES* heap_properties,
    D3D12_HEAP_FLAGS heap_flags, const D3D12_RESOURCE_DESC* desc,
    D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* clear_value,
    REFIID iid, void** out) {
    NativeD3d12Device* device = native_device(self);
    NativeD3d12Resource* resource;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuImageDescV1 image_desc;
    uint32_t format;
    int result;
    (void)heap_flags;
    (void)clear_value;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!heap_properties || !desc ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12Resource)) ||
        desc->Alignment != 0u && desc->Alignment != D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
        return E_INVALIDARG;
    if (desc->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
        desc->Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        return E_INVALIDARG;
    if (desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
        (desc->Width == 0u || desc->Height != 1u || desc->DepthOrArraySize != 1u ||
         desc->MipLevels != 1u || desc->Format != DXGI_FORMAT_UNKNOWN))
        return E_INVALIDARG;
    format = desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
                 ? native_format(desc->Format) : 0u;
    if (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        (format == 0u || desc->Width == 0u || desc->Height == 0u ||
         desc->DepthOrArraySize != 1u || desc->MipLevels > 1u ||
         desc->SampleDesc.Count == 0u || desc->SampleDesc.Count > 8u))
        return E_INVALIDARG;
    if (heap_properties->Type != D3D12_HEAP_TYPE_DEFAULT &&
        heap_properties->Type != D3D12_HEAP_TYPE_UPLOAD &&
        heap_properties->Type != D3D12_HEAP_TYPE_READBACK)
        return E_INVALIDARG;
    resource = (NativeD3d12Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    if (desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        memset(&buffer_desc, 0, sizeof(buffer_desc));
        buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
        buffer_desc.struct_size = sizeof(buffer_desc);
        buffer_desc.size_bytes = desc->Width;
        buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                            RIN_GPU_BUFFER_COPY_DESTINATION |
                            RIN_GPU_BUFFER_STORAGE | RIN_GPU_BUFFER_VERTEX |
                            RIN_GPU_BUFFER_INDEX | RIN_GPU_BUFFER_INDIRECT;
        if (heap_properties->Type != D3D12_HEAP_TYPE_DEFAULT)
            buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
        result = rindx_d3d12_create_buffer(&device->core, &buffer_desc,
                                           &resource->handle);
    } else {
        memset(&image_desc, 0, sizeof(image_desc));
        image_desc.abi_version = RIN_GPU_ABI_VERSION;
        image_desc.struct_size = sizeof(image_desc);
        image_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
        image_desc.format = format;
        image_desc.width = (uint32_t)desc->Width;
        image_desc.height = desc->Height;
        image_desc.depth = 1u;
        image_desc.array_layers = 1u;
        image_desc.mip_levels = 1u;
        image_desc.sample_count = desc->SampleDesc.Count;
        image_desc.usage = native_image_usage(desc);
        if (initial_state & D3D12_RESOURCE_STATE_PRESENT)
            image_desc.usage |= RIN_GPU_IMAGE_PRESENT;
        if (heap_properties->Type != D3D12_HEAP_TYPE_DEFAULT)
            image_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE |
                               RIN_GPU_IMAGE_CPU_READABLE;
        result = rindx_d3d12_create_texture2d(&device->core, &image_desc,
                                              &resource->handle);
    }
    if (result != RIN_GPU_OK) {
        free(resource);
        return native_result(result);
    }
    resource->iface.lpVtbl = (ID3D12ResourceVtbl*)(void*)&native_resource_vtable;
    resource->references = 1;
    resource->device = device;
    resource->desc = *desc;
    resource->heap_type = heap_properties->Type;
    resource->state = initial_state;
    ID3D12Device_AddRef(self);
    *out = &resource->iface;
    return S_OK;
}

static HRESULT native_d3d12_create_swapchain_resource(
    NativeD3d12Device* device, const D3D12_RESOURCE_DESC* desc,
    ID3D12Resource** out) {
    NativeD3d12Resource* resource;
    RinGpuImageDescV1 image_desc;
    int result;
    if (!device || !desc || !out) return E_INVALIDARG;
    *out = NULL;
    resource = (NativeD3d12Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    memset(&image_desc, 0, sizeof(image_desc));
    image_desc.abi_version = RIN_GPU_ABI_VERSION;
    image_desc.struct_size = sizeof(image_desc);
    image_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
    image_desc.format = native_format(desc->Format);
    image_desc.width = (uint32_t)desc->Width;
    image_desc.height = desc->Height;
    image_desc.depth = 1u;
    image_desc.array_layers = 1u;
    image_desc.mip_levels = 1u;
    image_desc.sample_count = desc->SampleDesc.Count;
    image_desc.usage = native_image_usage(desc) | RIN_GPU_IMAGE_PRESENT;
    image_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    result = rindx_d3d12_create_texture2d(&device->core, &image_desc,
                                          &resource->handle);
    if (result != RIN_GPU_OK) {
        free(resource);
        return native_result(result);
    }
    resource->iface.lpVtbl = (ID3D12ResourceVtbl*)(void*)&native_resource_vtable;
    resource->references = 1;
    resource->device = device;
    resource->desc = *desc;
    resource->heap_type = D3D12_HEAP_TYPE_DEFAULT;
    /* RinGPU images are created in UNDEFINED.  D3D12's PRESENT value aliases
     * COMMON (zero), so keep the native state at COMMON until the first
     * explicit render-target transition records PRESENT ownership. */
    resource->state = D3D12_RESOURCE_STATE_COMMON;
    ID3D12Device_AddRef(&device->iface);
    *out = &resource->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_descriptor_heap(
    ID3D12Device* self, const D3D12_DESCRIPTOR_HEAP_DESC* desc, REFIID iid,
    void** out) {
    NativeD3d12DescriptorHeap* heap;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->NumDescriptors == 0u || desc->NumDescriptors > 1024u ||
        desc->NodeMask != 1u || desc->Type > D3D12_DESCRIPTOR_HEAP_TYPE_DSV ||
        (desc->Flags & ~D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0u ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12DescriptorHeap)))
        return E_INVALIDARG;
    heap = (NativeD3d12DescriptorHeap*)calloc(1u, sizeof(*heap));
    if (!heap) return E_OUTOFMEMORY;
    heap->descriptors = (NativeD3d12Descriptor*)calloc(
        desc->NumDescriptors, sizeof(*heap->descriptors));
    if (!heap->descriptors) {
        free(heap);
        return E_OUTOFMEMORY;
    }
    heap->iface.lpVtbl = (ID3D12DescriptorHeapVtbl*)(void*)&native_descriptor_heap_vtable;
    heap->references = 1;
    heap->device = native_device(self);
    heap->desc = *desc;
    heap->cpu_base = (SIZE_T)(uintptr_t)heap;
    heap->gpu_base = heap->cpu_base ^ (SIZE_T)UINT64_C(0x4000000000000000);
    {
        uint32_t index;
        for (index = 0u; index < NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS; ++index) {
            if (!native_descriptor_heaps[index]) {
                native_descriptor_heaps[index] = heap;
                break;
            }
        }
        if (index == NATIVE_D3D12_MAX_DESCRIPTOR_HEAPS) {
            free(heap->descriptors);
            free(heap);
            return E_OUTOFMEMORY;
        }
    }
    ID3D12Device_AddRef(self);
    *out = &heap->iface;
    return S_OK;
}

static void native_descriptor_store(D3D12_CPU_DESCRIPTOR_HANDLE handle,
                                    NativeD3d12Resource* resource,
                                    uint32_t kind) {
    NativeD3d12Descriptor* descriptor = native_descriptor_resolve(handle, 0u);
    if (!descriptor) return;
    if (descriptor->resource)
        ID3D12Resource_Release(&descriptor->resource->iface);
    descriptor->resource = resource;
    descriptor->kind = kind;
    if (resource) ID3D12Resource_AddRef(&resource->iface);
}

static void WINAPI native_device_create_rtv(
    ID3D12Device* self, ID3D12Resource* resource_interface,
    const D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    NativeD3d12Resource* resource = resource_interface
        ? (NativeD3d12Resource*)(void*)resource_interface : NULL;
    (void)self; (void)desc;
    native_descriptor_store(dest, resource, 1u);
}
static void WINAPI native_device_create_dsv(
    ID3D12Device* self, ID3D12Resource* resource_interface,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    NativeD3d12Resource* resource = resource_interface
        ? (NativeD3d12Resource*)(void*)resource_interface : NULL;
    (void)self; (void)desc;
    native_descriptor_store(dest, resource, 2u);
}
static void WINAPI native_device_create_srv(
    ID3D12Device* self, ID3D12Resource* resource_interface,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    NativeD3d12Resource* resource = resource_interface
        ? (NativeD3d12Resource*)(void*)resource_interface : NULL;
    (void)self; (void)desc;
    native_descriptor_store(dest, resource, 3u);
}
static void WINAPI native_device_create_uav(
    ID3D12Device* self, ID3D12Resource* resource_interface,
    ID3D12Resource* counter, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,
    D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    NativeD3d12Resource* resource = resource_interface
        ? (NativeD3d12Resource*)(void*)resource_interface : NULL;
    (void)self; (void)counter; (void)desc;
    native_descriptor_store(dest, resource, 4u);
}
static void WINAPI native_device_create_cbv(
    ID3D12Device* self, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
    D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    (void)self; (void)desc;
    native_descriptor_store(dest, NULL, 5u);
}
static void WINAPI native_device_create_sampler(
    ID3D12Device* self, const D3D12_SAMPLER_DESC* desc,
    D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    (void)self; (void)desc;
    native_descriptor_store(dest, NULL, 6u);
}

static int native_surface_present(
    void* context, const RinGpuSoftwarePresentedImageV1* image) {
    (void)context;
    return image && image->pixels && image->size_bytes != 0u ? 0 : -1;
}

static int native_surface_acquire(void* context,
                                  const RinGpuImageDescV1* descriptor,
                                  uint64_t allocation_bytes,
                                  RinGpuSoftwareExternalImageV1* storage) {
    (void)context;
    (void)descriptor;
    (void)allocation_bytes;
    if (!storage) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(storage, 0, sizeof(*storage));
    return RIN_GPU_OK;
}

static void native_surface_init(RinGpuRuntimeDescV1* surface) {
    memset(surface, 0, sizeof(*surface));
    surface->struct_size = sizeof(*surface);
    surface->version = RIN_GPU_RUNTIME_VERSION;
    surface->device_generation = 1u;
    surface->handle_secret = UINT64_C(0x52494e4458333132);
    surface->max_buffer_size = 64u * 1024u * 1024u;
    surface->max_image_size = 64u * 1024u * 1024u;
    surface->max_total_allocation_size = 256u * 1024u * 1024u;
    surface->max_image_dimension = 4096u;
    surface->max_image_layers = 16u;
    surface->max_image_mip_levels = 16u;
    surface->max_image_sample_count = 8u;
    surface->adapter.abi_version = RIN_GPU_ABI_VERSION;
    surface->adapter.struct_size = sizeof(surface->adapter);
    surface->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                           RIN_GPU_QUEUE_COMPUTE |
                                           RIN_GPU_QUEUE_GRAPHICS;
    memcpy(surface->adapter.name, "RinDX D3D12 software", 20u);
    surface->display.abi_version = RIN_GPU_ABI_VERSION;
    surface->display.struct_size = sizeof(surface->display);
    surface->display.display_id = RIN_GPU_PRIMARY_DISPLAY;
    surface->display.flags = RIN_GPU_DISPLAY_CONNECTED |
                             RIN_GPU_DISPLAY_PRIMARY;
    surface->display.width = 2u;
    surface->display.height = 2u;
    surface->display.refresh_millihertz = 60000u;
    surface->display.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    surface->display.scale_milli = 1000u;
    memcpy(surface->display.name, "RinDX software", 15u);
    surface->present_callback = native_surface_present;
    surface->acquire_image = native_surface_acquire;
}

static HRESULT WINAPI native_query_interface(
    ID3D12Device* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Device)) {
        *out = self;
        ID3D12Device_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_add_ref(ID3D12Device* self) {
    return (ULONG)InterlockedIncrement(&native_device(self)->references);
}

static ULONG WINAPI native_release(ID3D12Device* self) {
    NativeD3d12Device* device = native_device(self);
    LONG references = InterlockedDecrement(&device->references);
    if (references == 0) {
        (void)rindx_d3d12_destroy_device(&device->core);
        native_object_data_destroy(&device->object);
        free(device);
    }
    return (ULONG)references;
}

static UINT WINAPI native_get_node_count(ID3D12Device* self) {
    uint32_t count = 0u;
    if (rindx_d3d12_get_node_count(&native_device(self)->core, &count) !=
        RIN_GPU_OK) return 0u;
    return count;
}

static HRESULT WINAPI native_check_feature_support(
    ID3D12Device* self, D3D12_FEATURE feature, void* data, UINT size) {
    uint32_t supported = 0u;
    uint32_t mapped;
    if (!data) return E_POINTER;
    if (feature == D3D12_FEATURE_D3D12_OPTIONS &&
        size >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS)) {
        memset(data, 0, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS));
        return S_OK;
    }
    if (feature == D3D12_FEATURE_ROOT_SIGNATURE &&
        size >= sizeof(D3D12_FEATURE_DATA_ROOT_SIGNATURE)) {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE* root =
            (D3D12_FEATURE_DATA_ROOT_SIGNATURE*)data;
        root->HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        return S_OK;
    }
    if (feature == D3D12_FEATURE_D3D12_OPTIONS) return E_INVALIDARG;
    mapped = feature == D3D12_FEATURE_ARCHITECTURE ?
        RIN_DX_D3D12_FEATURE_GRAPHICS_COMMANDS :
        RIN_DX_D3D12_FEATURE_COMPUTE_COMMANDS;
    if (rindx_d3d12_check_feature_support(&native_device(self)->core, mapped,
                                          &supported) != RIN_GPU_OK)
        return E_INVALIDARG;
    if (size < sizeof(uint32_t)) return E_INVALIDARG;
    *(uint32_t*)data = supported;
    return S_OK;
}

static HRESULT WINAPI native_removed_reason(ID3D12Device* self) {
    return rindx_d3d12_get_device_removed_reason_hresult(
        &native_device(self)->core);
}

static HRESULT WINAPI native_device_get_private_data(
    ID3D12Device* self, REFGUID guid, UINT* size, void* data) {
    return native_get_private(&native_device(self)->object, guid, size, data);
}
static HRESULT WINAPI native_device_set_private_data(
    ID3D12Device* self, REFGUID guid, UINT size, const void* data) {
    return native_set_private(&native_device(self)->object, guid, size, data,
                              NULL);
}
static HRESULT WINAPI native_device_set_private_interface(
    ID3D12Device* self, REFGUID guid, const IUnknown* data) {
    return native_set_private(&native_device(self)->object, guid,
                              sizeof(IUnknown*), NULL, (IUnknown*)data);
}
static HRESULT WINAPI native_device_set_name(ID3D12Device* self,
                                             LPCWSTR name) {
    return native_set_name(&native_device(self)->object, name);
}

static UINT WINAPI native_descriptor_increment_size(ID3D12Device* self,
                                                    D3D12_DESCRIPTOR_HEAP_TYPE type) {
    (void)self;
    return type <= D3D12_DESCRIPTOR_HEAP_TYPE_DSV ? 32u : 0u;
}

static int native_command_type_valid(D3D12_COMMAND_LIST_TYPE type) {
    return type == D3D12_COMMAND_LIST_TYPE_DIRECT ||
           type == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
           type == D3D12_COMMAND_LIST_TYPE_COPY;
}

static HRESULT native_query_queue_interface(ID3D12CommandQueue* self,
                                            REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) ||
        IsEqualIID(iid, &IID_ID3D12CommandQueue)) {
        *out = self;
        ID3D12CommandQueue_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static HRESULT native_query_allocator_interface(ID3D12CommandAllocator* self,
                                                REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) ||
        IsEqualIID(iid, &IID_ID3D12CommandAllocator)) {
        *out = self;
        ID3D12CommandAllocator_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static HRESULT native_query_list_interface(ID3D12GraphicsCommandList* self,
                                           REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12CommandList) ||
        IsEqualIID(iid, &IID_ID3D12GraphicsCommandList)) {
        *out = self;
        ID3D12GraphicsCommandList_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static HRESULT native_query_fence_interface(ID3D12Fence* self, REFIID iid,
                                            void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) || IsEqualIID(iid, &IID_ID3D12Fence)) {
        *out = self;
        ID3D12Fence_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_queue_add_ref(ID3D12CommandQueue* self) {
    return (ULONG)InterlockedIncrement(&native_queue(self)->references);
}
static ULONG WINAPI native_allocator_add_ref(ID3D12CommandAllocator* self) {
    return (ULONG)InterlockedIncrement(&native_allocator(self)->references);
}
static ULONG WINAPI native_list_add_ref(ID3D12GraphicsCommandList* self) {
    return (ULONG)InterlockedIncrement(&native_list(self)->references);
}
static ULONG WINAPI native_fence_add_ref(ID3D12Fence* self) {
    return (ULONG)InterlockedIncrement(&native_fence(self)->references);
}

static ULONG WINAPI native_queue_release(ID3D12CommandQueue* self) {
    NativeD3d12Queue* queue = native_queue(self);
    LONG references = InterlockedDecrement(&queue->references);
    if (references == 0) {
        ID3D12Device_Release(&queue->device->iface);
        native_object_data_destroy(&queue->object);
        free(queue);
    }
    return (ULONG)references;
}
static ULONG WINAPI native_allocator_release(ID3D12CommandAllocator* self) {
    NativeD3d12Allocator* allocator = native_allocator(self);
    LONG references = InterlockedDecrement(&allocator->references);
    if (references == 0) {
        (void)rindx_d3d12_destroy_command_allocator(&allocator->core);
        ID3D12Device_Release(&allocator->device->iface);
        native_object_data_destroy(&allocator->object);
        free(allocator);
    }
    return (ULONG)references;
}
static ULONG WINAPI native_list_release(ID3D12GraphicsCommandList* self) {
    NativeD3d12List* list = native_list(self);
    LONG references = InterlockedDecrement(&list->references);
    if (references == 0) {
        if (list->color_target)
            ID3D12Resource_Release(&list->color_target->iface);
        if (list->depth_target)
            ID3D12Resource_Release(&list->depth_target->iface);
        if (list->vertex_buffer)
            ID3D12Resource_Release(&list->vertex_buffer->iface);
        if (list->index_buffer)
            ID3D12Resource_Release(&list->index_buffer->iface);
        if (list->pipeline)
            ID3D12PipelineState_Release(&list->pipeline->iface);
        (void)rindx_d3d12_destroy_command_list(&list->core);
        ID3D12CommandAllocator_Release(&list->allocator->iface);
        ID3D12Device_Release(&list->device->iface);
        native_object_data_destroy(&list->object);
        free(list);
    }
    return (ULONG)references;
}
static ULONG WINAPI native_fence_release(ID3D12Fence* self) {
    NativeD3d12Fence* fence = native_fence(self);
    LONG references = InterlockedDecrement(&fence->references);
    if (references == 0) {
        (void)ringpu_runtime_destroy_object(fence->device->core.runtime,
                                            fence->handle);
        ID3D12Device_Release(&fence->device->iface);
        native_object_data_destroy(&fence->object);
        free(fence);
    }
    return (ULONG)references;
}

#define DEFINE_NATIVE_OBJECT_METHODS(prefix, iface_type, object_expr) \
static HRESULT WINAPI prefix##_get_private_data( \
    iface_type* self, REFGUID guid, UINT* size, void* data) { \
    return native_get_private(&(object_expr)->object, guid, size, data); \
} \
static HRESULT WINAPI prefix##_set_private_data( \
    iface_type* self, REFGUID guid, UINT size, const void* data) { \
    return native_set_private(&(object_expr)->object, guid, size, data, NULL); \
} \
static HRESULT WINAPI prefix##_set_private_interface( \
    iface_type* self, REFGUID guid, const IUnknown* data) { \
    return native_set_private(&(object_expr)->object, guid, sizeof(IUnknown*), \
                              NULL, (IUnknown*)data); \
} \
static HRESULT WINAPI prefix##_set_name(iface_type* self, LPCWSTR name) { \
    return native_set_name(&(object_expr)->object, name); \
}

DEFINE_NATIVE_OBJECT_METHODS(native_queue, ID3D12CommandQueue,
                              native_queue(self))
DEFINE_NATIVE_OBJECT_METHODS(native_allocator, ID3D12CommandAllocator,
                              native_allocator(self))
DEFINE_NATIVE_OBJECT_METHODS(native_list, ID3D12GraphicsCommandList,
                              native_list(self))
DEFINE_NATIVE_OBJECT_METHODS(native_fence, ID3D12Fence, native_fence(self))

static HRESULT WINAPI native_queue_get_device(ID3D12CommandQueue* self,
                                              REFIID iid, void** out) {
    return native_get_device(native_queue(self)->device, iid, out);
}
static HRESULT WINAPI native_allocator_get_device(ID3D12CommandAllocator* self,
                                                  REFIID iid, void** out) {
    return native_get_device(native_allocator(self)->device, iid, out);
}
static HRESULT WINAPI native_list_get_device(ID3D12GraphicsCommandList* self,
                                             REFIID iid, void** out) {
    return native_get_device(native_list(self)->device, iid, out);
}
static HRESULT WINAPI native_fence_get_device(ID3D12Fence* self, REFIID iid,
                                              void** out) {
    return native_get_device(native_fence(self)->device, iid, out);
}

static D3D12_COMMAND_LIST_TYPE WINAPI native_queue_get_type(
    ID3D12CommandQueue* self) {
    return native_queue(self)->desc.Type;
}
static void WINAPI native_queue_execute(ID3D12CommandQueue* self, UINT count,
                                        ID3D12CommandList* const* lists) {
    NativeD3d12Queue* queue = native_queue(self);
    UINT index;
    queue->last_result = RIN_GPU_OK;
    if (count == 0u || !lists) {
        queue->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    for (index = 0u; index < count; ++index) {
        NativeD3d12List* list = lists[index]
            ? (NativeD3d12List*)(void*)lists[index] : NULL;
        uint64_t fence_value = 0u;
        if (!list || list->device != queue->device ||
            list->type != queue->desc.Type) {
            queue->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
            return;
        }
        queue->last_result = rindx_d3d12_execute_command_lists(
            &queue->device->core, &list->core, &fence_value);
        if (queue->last_result != RIN_GPU_OK) return;
    }
}

static HRESULT native_signal_fence(NativeD3d12Fence* fence, UINT64 value) {
    RinGpuCommandListDescV1 descriptor;
    RinGpuSubmitInfoV1 submit;
    RinGpuHandle command_list = 0u;
    int result;
    uint64_t current = 0u;
    if (!fence || value == 0u ||
        ringpu_runtime_get_fence_value(fence->device->core.runtime,
                                        fence->handle, &current) != RIN_GPU_OK ||
        value <= current)
        return E_INVALIDARG;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    result = ringpu_runtime_create_command_list(fence->device->core.runtime,
                                                &descriptor, &command_list);
    if (result != RIN_GPU_OK) return native_result(result);
    result = ringpu_runtime_command_list_close(fence->device->core.runtime,
                                               command_list);
    if (result == RIN_GPU_OK) {
        memset(&submit, 0, sizeof(submit));
        submit.abi_version = RIN_GPU_ABI_VERSION;
        submit.struct_size = sizeof(submit);
        submit.command_list = command_list;
        submit.signal_fence = fence->handle;
        submit.signal_value = value;
        result = ringpu_runtime_queue_submit(fence->device->core.runtime,
                                             fence->device->core.queue, &submit);
    }
    if (ringpu_runtime_destroy_object(fence->device->core.runtime,
                                      command_list) != RIN_GPU_OK &&
        result == RIN_GPU_OK)
        result = RIN_GPU_ERROR_STATE;
    return native_result(result);
}

static HRESULT WINAPI native_queue_signal(ID3D12CommandQueue* self,
                                          ID3D12Fence* fence, UINT64 value) {
    NativeD3d12Queue* queue = native_queue(self);
    NativeD3d12Fence* target = fence ? native_fence(fence) : NULL;
    if (!target || target->device != queue->device) return E_INVALIDARG;
    return native_signal_fence(target, value);
}
static HRESULT WINAPI native_queue_wait(ID3D12CommandQueue* self,
                                        ID3D12Fence* fence, UINT64 value) {
    NativeD3d12Queue* queue = native_queue(self);
    NativeD3d12Fence* target = fence ? native_fence(fence) : NULL;
    uint64_t current = 0u;
    if (!target || target->device != queue->device || value == 0u)
        return E_INVALIDARG;
    if (ringpu_runtime_get_fence_value(queue->device->core.runtime,
                                       target->handle, &current) != RIN_GPU_OK)
        return E_FAIL;
    return value <= current ? S_OK : E_FAIL;
}
static HRESULT WINAPI native_queue_get_timestamp_frequency(
    ID3D12CommandQueue* self, UINT64* frequency) {
    (void)self;
    if (!frequency) return E_POINTER;
    *frequency = 1000000000ull;
    return S_OK;
}
static HRESULT WINAPI native_queue_get_clock_calibration(
    ID3D12CommandQueue* self, UINT64* gpu, UINT64* cpu) {
    LARGE_INTEGER counter;
    (void)self;
    if (!gpu || !cpu || !QueryPerformanceCounter(&counter)) return E_POINTER;
    *gpu = (UINT64)counter.QuadPart;
    *cpu = (UINT64)counter.QuadPart;
    return S_OK;
}
static D3D12_COMMAND_QUEUE_DESC* WINAPI native_queue_get_desc(
    ID3D12CommandQueue* self, D3D12_COMMAND_QUEUE_DESC* out) {
    if (!out) return NULL;
    *out = native_queue(self)->desc;
    return out;
}

static HRESULT WINAPI native_allocator_reset(ID3D12CommandAllocator* self) {
    return native_result(rindx_d3d12_reset_command_allocator(
        &native_allocator(self)->core));
}

static D3D12_COMMAND_LIST_TYPE WINAPI native_list_get_type(
    ID3D12GraphicsCommandList* self) {
    return native_list(self)->type;
}
static HRESULT WINAPI native_list_close(ID3D12GraphicsCommandList* self) {
    NativeD3d12List* list = native_list(self);
    int result;
    if (list->core.render_pass_active != 0u) {
        result = rindx_d3d12_end_render_pass(&list->core);
        if (result != RIN_GPU_OK) return native_result(result);
    }
    if (list->last_result != RIN_GPU_OK)
        return native_result(list->last_result);
    return native_result(rindx_d3d12_close_command_list(&list->core));
}
static HRESULT WINAPI native_list_reset(ID3D12GraphicsCommandList* self,
                                        ID3D12CommandAllocator* allocator,
                                        ID3D12PipelineState* initial_state) {
    NativeD3d12List* list = native_list(self);
    if (!allocator || allocator != &list->allocator->iface) return E_INVALIDARG;
    if (initial_state && native_pipeline(initial_state)->device != list->device)
        return E_INVALIDARG;
    if (list->pipeline) {
        ID3D12PipelineState_Release(&list->pipeline->iface);
        list->pipeline = NULL;
    }
    if (initial_state) {
        list->pipeline = native_pipeline(initial_state);
        ID3D12PipelineState_AddRef(initial_state);
    }
    list->last_result = rindx_d3d12_reset_command_list(&list->core);
    return native_result(list->last_result);
}
static void WINAPI native_list_clear_state(ID3D12GraphicsCommandList* self,
                                           ID3D12PipelineState* pipeline) {
    NativeD3d12List* list = native_list(self);
    (void)pipeline;
    if (list->core.render_pass_active != 0u)
        list->last_result = rindx_d3d12_end_render_pass(&list->core);
    if (list->color_target) {
        ID3D12Resource_Release(&list->color_target->iface);
        list->color_target = NULL;
    }
    if (list->depth_target) {
        ID3D12Resource_Release(&list->depth_target->iface);
        list->depth_target = NULL;
    }
    if (list->vertex_buffer) {
        ID3D12Resource_Release(&list->vertex_buffer->iface);
        list->vertex_buffer = NULL;
    }
    if (list->index_buffer) {
        ID3D12Resource_Release(&list->index_buffer->iface);
        list->index_buffer = NULL;
    }
    if (list->pipeline) {
        ID3D12PipelineState_Release(&list->pipeline->iface);
        list->pipeline = NULL;
    }
}
static void WINAPI native_list_copy_buffer(
    ID3D12GraphicsCommandList* self, ID3D12Resource* dst, UINT64 dst_offset,
    ID3D12Resource* src, UINT64 src_offset, UINT64 size) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Resource* destination = dst
        ? (NativeD3d12Resource*)(void*)dst : NULL;
    NativeD3d12Resource* source = src
        ? (NativeD3d12Resource*)(void*)src : NULL;
    if (!destination || !source || destination->device != list->device ||
        source->device != list->device) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->last_result = rindx_d3d12_copy_buffer(
        &list->core, destination->handle, dst_offset, source->handle,
        src_offset, size);
}

static void native_list_set_target(NativeD3d12Resource** slot,
                                   NativeD3d12Resource* resource) {
    if (*slot == resource) return;
    if (*slot) ID3D12Resource_Release(&(*slot)->iface);
    *slot = resource;
    if (*slot) ID3D12Resource_AddRef(&(*slot)->iface);
}

static int native_list_transition_target(NativeD3d12List* list,
                                         NativeD3d12Resource* resource,
                                         D3D12_RESOURCE_STATES target) {
    uint32_t before;
    uint32_t after;
    if (!resource || resource->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    before = resource->rin_present_state
        ? RIN_GPU_IMAGE_STATE_PRESENT
        : native_resource_state(resource->state);
    after = native_resource_target_state(target);
    if (before == after) return RIN_GPU_OK;
    if (rindx_d3d12_transition_image(&list->core, resource->handle, before,
                                     after) != RIN_GPU_OK)
        return RIN_GPU_ERROR_STATE;
    resource->state = target;
    resource->rin_present_state = target == D3D12_RESOURCE_STATE_PRESENT;
    return RIN_GPU_OK;
}

static void WINAPI native_list_om_set_render_targets(
    ID3D12GraphicsCommandList* self, UINT count,
    const D3D12_CPU_DESCRIPTOR_HANDLE* targets, BOOL single,
    const D3D12_CPU_DESCRIPTOR_HANDLE* depth) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Descriptor* color_descriptor = NULL;
    NativeD3d12Descriptor* depth_descriptor = NULL;
    (void)single;
    if (count > 1u || (count != 0u && !targets)) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (count != 0u)
        color_descriptor = native_descriptor_resolve(targets[0], 1u);
    if (depth) depth_descriptor = native_descriptor_resolve(*depth, 2u);
    if (count != 0u && (!color_descriptor || !color_descriptor->resource)) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (depth && (!depth_descriptor || !depth_descriptor->resource)) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (list->core.render_pass_active != 0u)
        list->last_result = rindx_d3d12_end_render_pass(&list->core);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = RIN_GPU_OK;
    native_list_set_target(&list->color_target,
                           color_descriptor ? color_descriptor->resource : NULL);
    native_list_set_target(&list->depth_target,
                           depth_descriptor ? depth_descriptor->resource : NULL);
    if (list->color_target)
        list->last_result = native_list_transition_target(
            list, list->color_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (list->last_result == RIN_GPU_OK && list->depth_target)
        list->last_result = native_list_transition_target(
            list, list->depth_target, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

static void WINAPI native_list_set_topology(ID3D12GraphicsCommandList* self,
                                            D3D12_PRIMITIVE_TOPOLOGY topology) {
    NativeD3d12List* list = native_list(self);
    if (topology < D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
        topology > D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->topology = topology;
}

static void WINAPI native_list_set_pipeline_state(
    ID3D12GraphicsCommandList* self, ID3D12PipelineState* pipeline_state) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Pipeline* pipeline = pipeline_state
        ? native_pipeline(pipeline_state) : NULL;
    if (pipeline && pipeline->device != list->device) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (list->pipeline == pipeline) return;
    if (list->pipeline)
        ID3D12PipelineState_Release(&list->pipeline->iface);
    list->pipeline = pipeline;
    if (list->pipeline) ID3D12PipelineState_AddRef(&list->pipeline->iface);
    list->last_result = RIN_GPU_OK;
}

static void WINAPI native_list_draw(
    ID3D12GraphicsCommandList* self, UINT vertices, UINT instances,
    UINT first_vertex, UINT first_instance) {
    NativeD3d12List* list = native_list(self);
    RinGpuDrawV1 draw;
    if (!list->color_target || list->core.render_pass_active != 0u ||
        !list->pipeline || list->pipeline->compute) {
        if (!list->color_target) list->last_result = RIN_GPU_ERROR_STATE;
        else list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->last_result = rindx_d3d12_bind_descriptor_heap(
        &list->core, list->pipeline->bind_group);
    if (list->last_result != RIN_GPU_OK) return;
    if (rindx_d3d12_begin_render_pass(
            &list->core, list->color_target->handle,
            list->depth_target ? list->depth_target->handle : 0u, 0u,
            0.0f, 0.0f, 0.0f, 1.0f, 1.0f) != RIN_GPU_OK) {
        list->last_result = RIN_GPU_ERROR_STATE;
        return;
    }
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = list->pipeline->handle;
    draw.vertex_count = vertices;
    draw.instance_count = instances;
    draw.first_vertex = first_vertex;
    draw.first_instance = first_instance;
    draw.color_target = list->color_target->handle;
    list->last_result = rindx_d3d12_draw(&list->core, &draw);
}

static void WINAPI native_list_draw_indexed(
    ID3D12GraphicsCommandList* self, UINT index_count, UINT instance_count,
    UINT first_index, INT base_vertex, UINT first_instance) {
    NativeD3d12List* list = native_list(self);
    RinGpuDrawIndexedV2 draw;
    if (!list->color_target || list->core.render_pass_active != 0u ||
        !list->pipeline || list->pipeline->compute || !list->index_buffer ||
        !list->vertex_buffer || base_vertex != 0 || instance_count == 0u) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->last_result = rindx_d3d12_bind_descriptor_heap(
        &list->core, list->pipeline->bind_group);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = rindx_d3d12_begin_render_pass(
        &list->core, list->color_target->handle,
        list->depth_target ? list->depth_target->handle : 0u, 0u,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    if (list->last_result != RIN_GPU_OK) return;
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = list->pipeline->handle;
    draw.color_target = list->color_target->handle;
    draw.index_buffer = list->index_buffer->handle;
    draw.index_offset = list->index_offset;
    draw.index_format = list->index_format == DXGI_FORMAT_R16_UINT
        ? RIN_GPU_INDEX_UINT16 : RIN_GPU_INDEX_UINT32;
    draw.index_count = index_count;
    draw.vertex_count = index_count;
    draw.instance_count = instance_count;
    draw.first_index = first_index;
    draw.first_instance = first_instance;
    draw.binding_count = 1u;
    draw.vertex_buffers[0].binding = 0u;
    draw.vertex_buffers[0].buffer = list->vertex_buffer->handle;
    draw.vertex_buffers[0].offset = list->vertex_offset;
    list->last_result = rindx_d3d12_draw_indexed_instanced(&list->core, &draw);
}

static void WINAPI native_list_dispatch(ID3D12GraphicsCommandList* self,
                                        UINT x, UINT y, UINT z) {
    NativeD3d12List* list = native_list(self);
    RinGpuDispatchV1 dispatch;
    if (!list->pipeline || !list->pipeline->compute ||
        list->core.render_pass_active != 0u) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->last_result = rindx_d3d12_bind_descriptor_heap(
        &list->core, list->pipeline->bind_group);
    if (list->last_result != RIN_GPU_OK) return;
    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.abi_version = RIN_GPU_ABI_VERSION;
    dispatch.struct_size = sizeof(dispatch);
    dispatch.group_count_x = x;
    dispatch.group_count_y = y;
    dispatch.group_count_z = z;
    list->last_result = rindx_d3d12_dispatch(&list->core, &dispatch);
}

static void WINAPI native_list_resource_barrier(
    ID3D12GraphicsCommandList* self, UINT count,
    const D3D12_RESOURCE_BARRIER* barriers) {
    NativeD3d12List* list = native_list(self);
    UINT index;
    if (count != 0u && !barriers) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    list->last_result = RIN_GPU_OK;
    for (index = 0u; index < count; ++index) {
        NativeD3d12Resource* resource;
        if (barriers[index].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
            !barriers[index].Transition.pResource) {
            list->last_result = RIN_GPU_ERROR_UNSUPPORTED;
            return;
        }
        resource = (NativeD3d12Resource*)(void*)
            barriers[index].Transition.pResource;
        if (resource->device != list->device ||
            native_resource_state(barriers[index].Transition.StateBefore) !=
                native_resource_state(resource->state)) {
            list->last_result = RIN_GPU_ERROR_STATE;
            return;
        }
        list->last_result = rindx_d3d12_transition_image(
            &list->core, resource->handle,
            native_resource_state(barriers[index].Transition.StateBefore),
            native_resource_state(barriers[index].Transition.StateAfter));
        if (list->last_result != RIN_GPU_OK) return;
        resource->state = barriers[index].Transition.StateAfter;
    }
}

static void WINAPI native_list_clear_rtv(
    ID3D12GraphicsCommandList* self, D3D12_CPU_DESCRIPTOR_HANDLE handle,
    const FLOAT color[4], UINT rect_count, const D3D12_RECT* rects) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Descriptor* descriptor = native_descriptor_resolve(handle, 1u);
    (void)rect_count; (void)rects;
    if (!descriptor || !descriptor->resource || !color) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (list->core.render_pass_active != 0u)
        list->last_result = rindx_d3d12_end_render_pass(&list->core);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = native_list_transition_target(
        list, descriptor->resource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = rindx_d3d12_begin_render_pass(
        &list->core, descriptor->resource->handle, 0u, 1u, color[0], color[1],
        color[2], color[3], 1.0f);
}

static void WINAPI native_list_clear_dsv(
    ID3D12GraphicsCommandList* self, D3D12_CPU_DESCRIPTOR_HANDLE handle,
    D3D12_CLEAR_FLAGS flags, FLOAT depth, UINT8 stencil, UINT rect_count,
    const D3D12_RECT* rects) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Descriptor* descriptor = native_descriptor_resolve(handle, 2u);
    (void)rect_count; (void)rects;
    if (!descriptor || !descriptor->resource || flags == 0u) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    if (!list->color_target) {
        list->last_result = RIN_GPU_ERROR_UNSUPPORTED;
        return;
    }
    if (list->core.render_pass_active != 0u)
        list->last_result = rindx_d3d12_end_render_pass(&list->core);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = native_list_transition_target(
        list, descriptor->resource, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if (list->last_result != RIN_GPU_OK) return;
    list->last_result = rindx_d3d12_begin_render_pass(
        &list->core, list->color_target->handle, descriptor->resource->handle,
        0u, 0.0f, 0.0f, 0.0f, 1.0f, depth);
    (void)stencil;
}

static void WINAPI native_list_copy_resource(ID3D12GraphicsCommandList* self,
                                             ID3D12Resource* dst,
                                             ID3D12Resource* src) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Resource* destination = dst
        ? (NativeD3d12Resource*)(void*)dst : NULL;
    NativeD3d12Resource* source = src
        ? (NativeD3d12Resource*)(void*)src : NULL;
    RinGpuImageCopyRegionV1 region;
    if (!destination || !source || destination->device != list->device ||
        source->device != list->device ||
        destination->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        source->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        destination->desc.Width != source->desc.Width ||
        destination->desc.Height != source->desc.Height) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    memset(&region, 0, sizeof(region));
    region.abi_version = RIN_GPU_ABI_VERSION;
    region.struct_size = sizeof(region);
    region.width = (uint32_t)destination->desc.Width;
    region.height = destination->desc.Height;
    region.depth = 1u;
    list->last_result = rindx_d3d12_copy_texture2d(
        &list->core, destination->handle, source->handle, &region);
}

static void WINAPI native_list_resolve_subresource(
    ID3D12GraphicsCommandList* self, ID3D12Resource* dst, UINT dst_subresource,
    ID3D12Resource* src, UINT src_subresource, DXGI_FORMAT format) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Resource* destination = dst
        ? (NativeD3d12Resource*)(void*)dst : NULL;
    NativeD3d12Resource* source = src
        ? (NativeD3d12Resource*)(void*)src : NULL;
    RinGpuImageResolveV1 resolve;
    if (!destination || !source || dst_subresource != 0u ||
        src_subresource != 0u || native_format(format) == 0u ||
        destination->device != list->device || source->device != list->device) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    memset(&resolve, 0, sizeof(resolve));
    resolve.abi_version = RIN_GPU_ABI_VERSION;
    resolve.struct_size = sizeof(resolve);
    resolve.width = (uint32_t)destination->desc.Width;
    resolve.height = destination->desc.Height;
    list->last_result = rindx_d3d12_resolve_texture2d(
        &list->core, destination->handle, source->handle, &resolve);
}

static void WINAPI native_list_ia_set_vertex_buffers(
    ID3D12GraphicsCommandList* self, UINT start_slot, UINT count,
    const D3D12_VERTEX_BUFFER_VIEW* views) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Resource* resource;
    (void)start_slot;
    if (count != 1u || !views || views[0].BufferLocation == 0u) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    resource = (NativeD3d12Resource*)(uintptr_t)views[0].BufferLocation;
    if (!resource || resource->device != list->device) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    native_list_set_target(&list->vertex_buffer, resource);
    list->vertex_stride = views[0].StrideInBytes;
    list->vertex_offset = 0u;
}

static void WINAPI native_list_ia_set_index_buffer(
    ID3D12GraphicsCommandList* self, const D3D12_INDEX_BUFFER_VIEW* view) {
    NativeD3d12List* list = native_list(self);
    NativeD3d12Resource* resource;
    if (!view || view->BufferLocation == 0u ||
        (view->Format != DXGI_FORMAT_R16_UINT &&
         view->Format != DXGI_FORMAT_R32_UINT)) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    resource = (NativeD3d12Resource*)(uintptr_t)view->BufferLocation;
    if (!resource || resource->device != list->device) {
        list->last_result = RIN_GPU_ERROR_INVALID_ARGUMENT;
        return;
    }
    native_list_set_target(&list->index_buffer, resource);
    list->index_offset = 0u;
    list->index_format = view->Format;
}

static UINT64 WINAPI native_fence_get_completed_value(ID3D12Fence* self) {
    uint64_t value = 0u;
    if (ringpu_runtime_get_fence_value(native_fence(self)->device->core.runtime,
                                       native_fence(self)->handle,
                                       &value) != RIN_GPU_OK)
        return UINT64_MAX;
    return value;
}
static HRESULT WINAPI native_fence_set_event(ID3D12Fence* self, UINT64 value,
                                             HANDLE event) {
    if (!event || value == 0u) return E_INVALIDARG;
    if (native_fence_get_completed_value(self) < value) {
        HRESULT result = native_result(ringpu_runtime_wait_fence(
            native_fence(self)->device->core.runtime,
            native_fence(self)->handle, value, UINT64_MAX));
        if (FAILED(result)) return result;
    }
    return SetEvent(event) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
static HRESULT WINAPI native_fence_signal(ID3D12Fence* self, UINT64 value) {
    return native_signal_fence(native_fence(self), value);
}

static HRESULT WINAPI native_device_create_queue(
    ID3D12Device* self, const D3D12_COMMAND_QUEUE_DESC* desc, REFIID iid,
    void** out) {
    NativeD3d12Device* device = native_device(self);
    NativeD3d12Queue* queue;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->NodeMask != 1u || !native_command_type_valid(desc->Type) ||
        desc->Flags != D3D12_COMMAND_QUEUE_FLAG_NONE || desc->Priority < -0x80000000LL ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12CommandQueue)))
        return E_INVALIDARG;
    queue = (NativeD3d12Queue*)calloc(1u, sizeof(*queue));
    if (!queue) return E_OUTOFMEMORY;
    queue->iface.lpVtbl = (ID3D12CommandQueueVtbl*)(void*)&native_queue_vtable;
    queue->references = 1;
    queue->device = device;
    queue->desc = *desc;
    queue->last_result = RIN_GPU_OK;
    ID3D12Device_AddRef(self);
    *out = &queue->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_allocator(
    ID3D12Device* self, D3D12_COMMAND_LIST_TYPE type, REFIID iid, void** out) {
    NativeD3d12Allocator* allocator;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!native_command_type_valid(type) ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12CommandAllocator)))
        return E_INVALIDARG;
    allocator = (NativeD3d12Allocator*)calloc(1u, sizeof(*allocator));
    if (!allocator) return E_OUTOFMEMORY;
    result = rindx_d3d12_create_command_allocator(
        &native_device(self)->core, &allocator->core);
    if (result != RIN_GPU_OK) {
        free(allocator);
        return native_result(result);
    }
    allocator->iface.lpVtbl = (ID3D12CommandAllocatorVtbl*)(void*)&native_allocator_vtable;
    allocator->references = 1;
    allocator->device = native_device(self);
    ID3D12Device_AddRef(self);
    *out = &allocator->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_list(
    ID3D12Device* self, UINT node_mask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* allocator_interface,
    ID3D12PipelineState* initial_state, REFIID iid, void** out) {
    NativeD3d12Allocator* allocator = allocator_interface
        ? native_allocator(allocator_interface) : NULL;
    NativeD3d12List* list;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (node_mask != 1u || !native_command_type_valid(type) || !allocator ||
        allocator->device != native_device(self) || initial_state ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12CommandList) &&
         !IsEqualIID(iid, &IID_ID3D12GraphicsCommandList)))
        return E_INVALIDARG;
    list = (NativeD3d12List*)calloc(1u, sizeof(*list));
    if (!list) return E_OUTOFMEMORY;
    result = rindx_d3d12_create_command_list(
        &native_device(self)->core, &allocator->core, &list->core);
    if (result != RIN_GPU_OK) {
        free(list);
        return native_result(result);
    }
    list->iface.lpVtbl = (ID3D12GraphicsCommandListVtbl*)(void*)&native_list_vtable;
    list->references = 1;
    list->device = native_device(self);
    list->allocator = allocator;
    list->type = type;
    list->last_result = RIN_GPU_OK;
    ID3D12Device_AddRef(self);
    ID3D12CommandAllocator_AddRef(allocator_interface);
    *out = &list->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_fence(
    ID3D12Device* self, UINT64 initial_value, D3D12_FENCE_FLAGS flags,
    REFIID iid, void** out) {
    NativeD3d12Fence* fence;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (flags != D3D12_FENCE_FLAG_NONE ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D12Fence)))
        return E_INVALIDARG;
    fence = (NativeD3d12Fence*)calloc(1u, sizeof(*fence));
    if (!fence) return E_OUTOFMEMORY;
    result = ringpu_runtime_create_fence(native_device(self)->core.runtime,
                                         initial_value, &fence->handle);
    if (result != RIN_GPU_OK) {
        free(fence);
        return native_result(result);
    }
    fence->iface.lpVtbl = (ID3D12FenceVtbl*)(void*)&native_fence_vtable;
    fence->references = 1;
    fence->device = native_device(self);
    ID3D12Device_AddRef(self);
    *out = &fence->iface;
    return S_OK;
}

static HRESULT WINAPI native_pipeline_query_interface(
    ID3D12PipelineState* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D12Object) ||
        IsEqualIID(iid, &IID_ID3D12DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D12Pageable) ||
        IsEqualIID(iid, &IID_ID3D12PipelineState)) {
        *out = self;
        ID3D12PipelineState_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_pipeline_add_ref(ID3D12PipelineState* self) {
    return (ULONG)InterlockedIncrement(&native_pipeline(self)->references);
}

static ULONG WINAPI native_pipeline_release(ID3D12PipelineState* self) {
    NativeD3d12Pipeline* pipeline = native_pipeline(self);
    LONG references = InterlockedDecrement(&pipeline->references);
    if (references == 0) {
        if (pipeline->bind_group)
            (void)rindx_d3d12_destroy_object(&pipeline->device->core,
                                              pipeline->bind_group);
        if (pipeline->handle)
            (void)rindx_d3d12_destroy_object(&pipeline->device->core,
                                              pipeline->handle);
        ID3D12Device_Release(&pipeline->device->iface);
        native_object_data_destroy(&pipeline->object);
        free(pipeline);
    }
    return (ULONG)references;
}

DEFINE_NATIVE_OBJECT_METHODS(native_pipeline, ID3D12PipelineState,
                             native_pipeline(self))

static HRESULT WINAPI native_pipeline_get_device(ID3D12PipelineState* self,
                                                 REFIID iid, void** out) {
    return native_get_device(native_pipeline(self)->device, iid, out);
}

static HRESULT WINAPI native_blob_query_interface(
    ID3DBlob* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3DBlob)) {
        *out = self;
        ID3D10Blob_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_blob_add_ref(ID3DBlob* self) {
    return (ULONG)InterlockedIncrement(&native_blob(self)->references);
}

static ULONG WINAPI native_blob_release(ID3DBlob* self) {
    NativeD3d12Blob* blob = native_blob(self);
    LONG references = InterlockedDecrement(&blob->references);
    if (references == 0) {
        free(blob->bytes);
        free(blob);
    }
    return (ULONG)references;
}

static LPVOID WINAPI native_blob_get_buffer_pointer(ID3DBlob* self) {
    return native_blob(self)->bytes;
}

static SIZE_T WINAPI native_blob_get_buffer_size(ID3DBlob* self) {
    return native_blob(self)->size;
}

static HRESULT native_d3d12_pipeline_create_cache_blob(
    NativeD3d12Pipeline* pipeline, ID3DBlob** blob_out) {
    static const BYTE magic[] = {'R', 'I', 'N', 'D', 'X', 'P', 'S', 'O', 1u};
    NativeD3d12Blob* blob;
    uint64_t handle;
    uint32_t compute;
    if (!blob_out) return E_POINTER;
    *blob_out = NULL;
    blob = (NativeD3d12Blob*)calloc(1u, sizeof(*blob));
    if (!blob) return E_OUTOFMEMORY;
    blob->size = sizeof(magic) + sizeof(handle) + sizeof(compute);
    blob->bytes = (BYTE*)malloc(blob->size);
    if (!blob->bytes) {
        free(blob);
        return E_OUTOFMEMORY;
    }
    handle = pipeline->handle;
    compute = pipeline->compute ? 1u : 0u;
    memcpy(blob->bytes, magic, sizeof(magic));
    memcpy(blob->bytes + sizeof(magic), &handle, sizeof(handle));
    memcpy(blob->bytes + sizeof(magic) + sizeof(handle), &compute,
           sizeof(compute));
    blob->iface.lpVtbl = &native_blob_vtable;
    blob->references = 1;
    *blob_out = &blob->iface;
    return S_OK;
}

static HRESULT WINAPI native_pipeline_get_cached_blob(
    ID3D12PipelineState* self, ID3DBlob** blob_out) {
    return native_d3d12_pipeline_create_cache_blob(native_pipeline(self),
                                                   blob_out);
}

static int native_pipeline_input_layout(
    const D3D12_INPUT_LAYOUT_DESC* input,
    RinGpuVertexAttributeV2* attributes, UINT* attribute_count,
    RinGpuVertexBufferLayoutV1* bindings, UINT* binding_count) {
    UINT index;
    if (!input || !attribute_count || !binding_count ||
        (input->NumElements != 0u && !input->pInputElementDescs) ||
        input->NumElements > 16u)
        return 0;
    *attribute_count = 0u;
    *binding_count = 0u;
    for (index = 0u; index < input->NumElements; ++index) {
        const D3D12_INPUT_ELEMENT_DESC* element =
            &input->pInputElementDescs[index];
        uint32_t format = native_vertex_format(element->Format);
        UINT binding_index = 0u;
        UINT end;
        UINT binding;
        if (!element->SemanticName || format == 0u ||
            element->InputSlot >= RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS ||
            element->AlignedByteOffset == D3D12_APPEND_ALIGNED_ELEMENT ||
            element->InputSlotClass > D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA ||
            (element->InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA &&
             element->InstanceDataStepRate == 0u))
            return 0;
        end = element->AlignedByteOffset + 4u;
        for (binding = 0u; binding < *binding_count; ++binding) {
            if (bindings[binding].binding == element->InputSlot) {
                binding_index = binding;
                if (end > bindings[binding].stride)
                    bindings[binding].stride = end;
                break;
            }
        }
        if (binding == *binding_count) {
            if (*binding_count >= RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS)
                return 0;
            binding_index = (*binding_count)++;
            memset(&bindings[binding_index], 0, sizeof(bindings[binding_index]));
            bindings[binding_index].binding = element->InputSlot;
            bindings[binding_index].stride = end;
            bindings[binding_index].flags =
                element->InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                    ? element->InstanceDataStepRate : 0u;
        }
        memset(&attributes[*attribute_count], 0,
               sizeof(attributes[*attribute_count]));
        attributes[*attribute_count].abi_version = RIN_GPU_ABI_VERSION;
        attributes[*attribute_count].struct_size =
            sizeof(attributes[*attribute_count]);
        attributes[*attribute_count].location = element->SemanticIndex;
        attributes[*attribute_count].format = format;
        attributes[*attribute_count].offset = element->AlignedByteOffset;
        attributes[*attribute_count].binding =
            bindings[binding_index].binding;
        (*attribute_count)++;
    }
    return 1;
}

static HRESULT WINAPI native_device_create_graphics_pipeline_state(
    ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
    REFIID iid, void** out) {
    NativeD3d12Device* device = native_device(self);
    RinGpuGraphicsPipelineNativeDescV2 pipeline_desc;
    RinGpuVertexAttributeV2 attributes[16];
    RinGpuVertexBufferLayoutV1 bindings[RIN_GPU_MAX_VERTEX_BUFFER_BINDINGS];
    NativeD3d12Pipeline* pipeline;
    RinGpuHandle vertex_shader = 0u;
    RinGpuHandle fragment_shader = 0u;
    UINT attribute_count = 0u;
    UINT binding_count = 0u;
    uint32_t format;
    uint32_t topology;
    uint32_t depth_format = 0u;
    uint32_t depth_compare = 0u;
    uint32_t blend_source = 0u;
    uint32_t blend_destination = 0u;
    uint32_t blend_operation = 0u;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || !IsEqualIID(iid, &IID_IUnknown) &&
        !IsEqualIID(iid, &IID_ID3D12PipelineState))
        return E_INVALIDARG;
    format = native_format(desc->RTVFormats[0]);
    topology = native_primitive_topology(desc->PrimitiveTopologyType);
    if (desc->NodeMask != 1u || desc->Flags != D3D12_PIPELINE_STATE_FLAG_NONE ||
        desc->pRootSignature || desc->NumRenderTargets != 1u ||
        format == 0u || topology == 0u || desc->SampleDesc.Count != 1u ||
        !desc->VS.pShaderBytecode || desc->VS.BytecodeLength == 0u ||
        !desc->PS.pShaderBytecode || desc->PS.BytecodeLength == 0u ||
        !native_pipeline_input_layout(&desc->InputLayout, attributes,
                                      &attribute_count, bindings,
                                      &binding_count))
        return E_INVALIDARG;
    if (desc->DSVFormat != DXGI_FORMAT_UNKNOWN) {
        depth_format = native_format(desc->DSVFormat);
        depth_compare = native_compare(desc->DepthStencilState.DepthFunc);
        if (depth_format == 0u || depth_compare == 0u ||
            desc->DepthStencilState.StencilEnable)
            return E_INVALIDARG;
    }
    if (desc->BlendState.RenderTarget[0].BlendEnable) {
        blend_source = native_blend_factor(
            desc->BlendState.RenderTarget[0].SrcBlend);
        blend_destination = native_blend_factor(
            desc->BlendState.RenderTarget[0].DestBlend);
        blend_operation = native_blend_op(
            desc->BlendState.RenderTarget[0].BlendOp);
        if (!blend_source || !blend_destination || !blend_operation ||
            !native_blend_factor(desc->BlendState.RenderTarget[0].SrcBlendAlpha) ||
            !native_blend_factor(desc->BlendState.RenderTarget[0].DestBlendAlpha) ||
            !native_blend_op(desc->BlendState.RenderTarget[0].BlendOpAlpha))
            return E_INVALIDARG;
    }
    if (desc->RasterizerState.CullMode == D3D12_CULL_MODE_NONE)
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_NONE;
    else if (desc->RasterizerState.CullMode == D3D12_CULL_MODE_FRONT)
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_FRONT;
    else if (desc->RasterizerState.CullMode == D3D12_CULL_MODE_BACK)
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_BACK;
    else return E_INVALIDARG;
    pipeline_desc.base.front_face = desc->RasterizerState.FrontCounterClockwise
        ? RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE
        : RIN_GPU_FRONT_FACE_CLOCKWISE;
    result = rindx_d3d12_create_shader(&device->core, desc->VS.pShaderBytecode,
                                       desc->VS.BytecodeLength, &vertex_shader);
    if (result != RIN_GPU_OK) return native_result(result);
    result = rindx_d3d12_create_shader(&device->core, desc->PS.pShaderBytecode,
                                       desc->PS.BytecodeLength, &fragment_shader);
    if (result != RIN_GPU_OK) {
        (void)rindx_d3d12_destroy_object(&device->core, vertex_shader);
        return native_result(result);
    }
    memset(&pipeline_desc, 0, sizeof(pipeline_desc));
    pipeline_desc.base.abi_version = RIN_GPU_ABI_VERSION;
    pipeline_desc.base.struct_size = sizeof(pipeline_desc.base);
    pipeline_desc.base.vertex_shader = vertex_shader;
    pipeline_desc.base.fragment_shader = fragment_shader;
    pipeline_desc.base.color_format = format;
    pipeline_desc.base.primitive_topology = topology;
    pipeline_desc.base.depth_format = depth_format;
    pipeline_desc.base.depth_compare = depth_compare;
    pipeline_desc.base.depth_write_enabled =
        desc->DepthStencilState.DepthEnable ? 1u : 0u;
    pipeline_desc.base.blend_enabled =
        desc->BlendState.RenderTarget[0].BlendEnable ? 1u : 0u;
    pipeline_desc.base.source_color_factor = blend_source;
    pipeline_desc.base.destination_color_factor = blend_destination;
    pipeline_desc.base.color_operation = blend_operation;
    pipeline_desc.base.source_alpha_factor =
        pipeline_desc.base.blend_enabled
            ? native_blend_factor(desc->BlendState.RenderTarget[0].SrcBlendAlpha)
            : 0u;
    pipeline_desc.base.destination_alpha_factor =
        pipeline_desc.base.blend_enabled
            ? native_blend_factor(desc->BlendState.RenderTarget[0].DestBlendAlpha)
            : 0u;
    pipeline_desc.base.alpha_operation =
        pipeline_desc.base.blend_enabled
            ? native_blend_op(desc->BlendState.RenderTarget[0].BlendOpAlpha)
            : 0u;
    pipeline_desc.base.color_write_mask =
        desc->BlendState.RenderTarget[0].RenderTargetWriteMask &
        RIN_GPU_COLOR_WRITE_ALL;
    pipeline_desc.base.flags = 0u;
    pipeline_desc.base.reserved0 = 0u;
    if (desc->RasterizerState.CullMode == D3D12_CULL_MODE_NONE)
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_NONE;
    else if (desc->RasterizerState.CullMode == D3D12_CULL_MODE_FRONT)
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_FRONT;
    else
        pipeline_desc.base.cull_mode = RIN_GPU_CULL_BACK;
    pipeline_desc.base.front_face = desc->RasterizerState.FrontCounterClockwise
        ? RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE
        : RIN_GPU_FRONT_FACE_CLOCKWISE;
    pipeline_desc.blend_constant_red = 0.0f;
    pipeline_desc.blend_constant_green = 0.0f;
    pipeline_desc.blend_constant_blue = 0.0f;
    pipeline_desc.blend_constant_alpha = 0.0f;
    /* The owner writes the output handle through a distinct local so the
     * shader handles remain valid until the pipeline has acquired them. */
    {
        RinGpuHandle pipeline_handle = 0u;
        result = rindx_d3d12_create_graphics_pipeline(
            &device->core, &pipeline_desc, attributes, attribute_count,
            bindings, binding_count, NULL, 0u, &pipeline_handle);
        if (result != RIN_GPU_OK) {
            (void)rindx_d3d12_destroy_object(&device->core, fragment_shader);
            (void)rindx_d3d12_destroy_object(&device->core, vertex_shader);
            return native_result(result);
        }
        pipeline = (NativeD3d12Pipeline*)calloc(1u, sizeof(*pipeline));
        if (!pipeline) {
            (void)rindx_d3d12_destroy_object(&device->core, pipeline_handle);
            (void)rindx_d3d12_destroy_object(&device->core, fragment_shader);
            (void)rindx_d3d12_destroy_object(&device->core, vertex_shader);
            return E_OUTOFMEMORY;
        }
        result = rindx_d3d12_create_descriptor_heap(
            &device->core, pipeline_handle, NULL, 0u, &pipeline->bind_group);
        if (result != RIN_GPU_OK) {
            free(pipeline);
            (void)rindx_d3d12_destroy_object(&device->core, pipeline_handle);
            (void)rindx_d3d12_destroy_object(&device->core, fragment_shader);
            (void)rindx_d3d12_destroy_object(&device->core, vertex_shader);
            return native_result(result);
        }
        pipeline->iface.lpVtbl = (ID3D12PipelineStateVtbl*)(void*)&native_pipeline_vtable;
        pipeline->references = 1;
        pipeline->device = device;
        pipeline->handle = pipeline_handle;
        pipeline->compute = FALSE;
        ID3D12Device_AddRef(self);
        (void)rindx_d3d12_destroy_object(&device->core, fragment_shader);
        (void)rindx_d3d12_destroy_object(&device->core, vertex_shader);
        *out = &pipeline->iface;
    }
    return S_OK;
}

static HRESULT WINAPI native_device_create_compute_pipeline_state(
    ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* desc,
    REFIID iid, void** out) {
    NativeD3d12Device* device = native_device(self);
    NativeD3d12Pipeline* pipeline;
    RinGpuHandle shader = 0u;
    RinGpuHandle pipeline_handle = 0u;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || !IsEqualIID(iid, &IID_IUnknown) &&
        !IsEqualIID(iid, &IID_ID3D12PipelineState) || desc->NodeMask != 1u ||
        desc->Flags != D3D12_PIPELINE_STATE_FLAG_NONE || desc->pRootSignature ||
        !desc->CS.pShaderBytecode || desc->CS.BytecodeLength == 0u)
        return E_INVALIDARG;
    result = rindx_d3d12_create_shader(&device->core, desc->CS.pShaderBytecode,
                                       desc->CS.BytecodeLength, &shader);
    if (result != RIN_GPU_OK) return native_result(result);
    result = rindx_d3d12_create_compute_pipeline(&device->core, shader,
                                                 &pipeline_handle);
    if (result != RIN_GPU_OK) {
        (void)rindx_d3d12_destroy_object(&device->core, shader);
        return native_result(result);
    }
    pipeline = (NativeD3d12Pipeline*)calloc(1u, sizeof(*pipeline));
    if (!pipeline) {
        (void)rindx_d3d12_destroy_object(&device->core, pipeline_handle);
        (void)rindx_d3d12_destroy_object(&device->core, shader);
        return E_OUTOFMEMORY;
    }
    result = rindx_d3d12_create_compute_bind_group(
        &device->core, pipeline_handle, NULL, 0u, &pipeline->bind_group);
    if (result != RIN_GPU_OK) {
        free(pipeline);
        (void)rindx_d3d12_destroy_object(&device->core, pipeline_handle);
        (void)rindx_d3d12_destroy_object(&device->core, shader);
        return native_result(result);
    }
    pipeline->iface.lpVtbl = (ID3D12PipelineStateVtbl*)(void*)&native_pipeline_vtable;
    pipeline->references = 1;
    pipeline->device = device;
    pipeline->handle = pipeline_handle;
    pipeline->compute = TRUE;
    ID3D12Device_AddRef(self);
    (void)rindx_d3d12_destroy_object(&device->core, shader);
    *out = &pipeline->iface;
    return S_OK;
}

static NativeD3d12Swapchain* native_d3d12_swapchain(IDXGISwapChain* self) {
    return (NativeD3d12Swapchain*)(void*)self;
}

static HRESULT WINAPI native_d3d12_swapchain_query_interface(
    IDXGISwapChain* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDXGIObject) ||
        IsEqualIID(iid, &IID_IDXGIDeviceSubObject) ||
        IsEqualIID(iid, &IID_IDXGISwapChain)) {
        *out = self;
        IDXGISwapChain_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_d3d12_swapchain_add_ref(IDXGISwapChain* self) {
    return (ULONG)InterlockedIncrement(&native_d3d12_swapchain(self)->references);
}

static void native_d3d12_swapchain_destroy_buffers(NativeD3d12Swapchain* swapchain) {
    UINT index;
    for (index = 0u; index < swapchain->buffer_count; ++index) {
        if (swapchain->buffers[index]) {
            ID3D12Resource_Release(&swapchain->buffers[index]->iface);
            swapchain->buffers[index] = NULL;
        }
    }
    swapchain->buffer_count = 0u;
    swapchain->current_buffer = 0u;
}

static HRESULT native_d3d12_swapchain_create_buffers(
    NativeD3d12Swapchain* swapchain) {
    D3D12_RESOURCE_DESC resource_desc;
    UINT index;
    memset(&resource_desc, 0, sizeof(resource_desc));
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Width = swapchain->desc.BufferDesc.Width;
    resource_desc.Height = swapchain->desc.BufferDesc.Height;
    resource_desc.DepthOrArraySize = 1u;
    resource_desc.MipLevels = 1u;
    resource_desc.Format = swapchain->desc.BufferDesc.Format;
    resource_desc.SampleDesc = swapchain->desc.SampleDesc;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    for (index = 0u; index < swapchain->buffer_count; ++index) {
        HRESULT result = native_d3d12_create_swapchain_resource(
            swapchain->device, &resource_desc,
            (ID3D12Resource**)&swapchain->buffers[index]);
        if (FAILED(result)) {
            native_d3d12_swapchain_destroy_buffers(swapchain);
            return result;
        }
    }
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_release_object(
    IDXGISwapChain* self) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    native_d3d12_swapchain_destroy_buffers(swapchain);
    if (swapchain->queue)
        ID3D12CommandQueue_Release(&swapchain->queue->iface);
    if (swapchain->device)
        ID3D12Device_Release(&swapchain->device->iface);
    native_object_data_destroy(&swapchain->object);
    free(swapchain);
    return S_OK;
}

static ULONG WINAPI native_d3d12_swapchain_release(IDXGISwapChain* self) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    LONG references = InterlockedDecrement(&swapchain->references);
    if (references == 0)
        (void)native_d3d12_swapchain_release_object(self);
    return (ULONG)references;
}

DEFINE_NATIVE_OBJECT_METHODS(native_d3d12_swapchain, IDXGISwapChain,
                             native_d3d12_swapchain(self))

static HRESULT WINAPI native_d3d12_swapchain_get_device(
    IDXGISwapChain* self, REFIID iid, void** out) {
    return native_get_device(native_d3d12_swapchain(self)->device, iid, out);
}

static HRESULT WINAPI native_d3d12_swapchain_get_parent(
    IDXGISwapChain* self, REFIID iid, void** out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = NULL;
    (void)iid;
    return E_NOINTERFACE;
}

static HRESULT native_d3d12_swapchain_run_transition(
    NativeD3d12Swapchain* swapchain, NativeD3d12Resource* resource,
    D3D12_RESOURCE_STATES target) {
    ID3D12CommandAllocator* allocator = NULL;
    ID3D12GraphicsCommandList* list_interface = NULL;
    NativeD3d12List* list;
    uint64_t fence_value = 0u;
    HRESULT result;
    result = ID3D12Device_CreateCommandAllocator(
        &swapchain->device->iface, D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator, (void**)&allocator);
    if (FAILED(result)) return result;
    result = ID3D12Device_CreateCommandList(
        &swapchain->device->iface, 1u, D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator, NULL, &IID_ID3D12GraphicsCommandList,
        (void**)&list_interface);
    if (FAILED(result)) {
        ID3D12CommandAllocator_Release(allocator);
        return result;
    }
    list = native_list(list_interface);
    result = native_result(native_list_transition_target(list, resource, target));
    if (SUCCEEDED(result))
        result = ID3D12GraphicsCommandList_Close(list_interface);
    if (SUCCEEDED(result)) {
        ID3D12CommandList* command_list = (ID3D12CommandList*)list_interface;
        ID3D12CommandQueue_ExecuteCommandLists(
            &swapchain->queue->iface, 1u, &command_list);
        result = native_result(swapchain->queue->last_result);
    }
    if (SUCCEEDED(result)) {
        fence_value = swapchain->device->core.submission_value;
        result = native_result(rindx_d3d12_wait(
            &swapchain->device->core, fence_value, UINT64_MAX));
    }
    ID3D12GraphicsCommandList_Release(list_interface);
    ID3D12CommandAllocator_Release(allocator);
    return result;
}

static HRESULT native_d3d12_swapchain_present_pixels(
    NativeD3d12Swapchain* swapchain, NativeD3d12Resource* resource) {
    const UINT width = swapchain->desc.BufferDesc.Width;
    const UINT height = swapchain->desc.BufferDesc.Height;
    const UINT pitch = width * 4u;
    BYTE* pixels;
    RinGpuImageReadbackV1 readback;
    HDC dc;
    BITMAPINFO bitmap;
    UINT x;
    UINT y;
    HRESULT result;
    if (!swapchain->desc.OutputWindow || !IsWindow(swapchain->desc.OutputWindow))
        return E_INVALIDARG;
    pixels = (BYTE*)malloc((size_t)pitch * height);
    if (!pixels) return E_OUTOFMEMORY;
    memset(&readback, 0, sizeof(readback));
    readback.abi_version = RIN_GPU_ABI_VERSION;
    readback.struct_size = sizeof(readback);
    readback.width = width;
    readback.height = height;
    readback.depth = 1u;
    readback.destination_row_pitch_bytes = pitch;
    readback.destination_slice_pitch_bytes = (uint64_t)pitch * height;
    result = native_result(rindx_d3d12_readback_image(
        &swapchain->device->core, resource->handle, &readback,
        pixels, (uint64_t)pitch * height));
    if (FAILED(result)) {
        free(pixels);
        return result;
    }
    if (swapchain->desc.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        for (y = 0u; y < height; ++y) {
            for (x = 0u; x < width; ++x) {
                BYTE* pixel = pixels + (size_t)y * pitch + x * 4u;
                BYTE red = pixel[0];
                pixel[0] = pixel[2];
                pixel[2] = red;
            }
        }
    }
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
    bitmap.bmiHeader.biWidth = (LONG)width;
    bitmap.bmiHeader.biHeight = -(LONG)height;
    bitmap.bmiHeader.biPlanes = 1u;
    bitmap.bmiHeader.biBitCount = 32u;
    bitmap.bmiHeader.biCompression = BI_RGB;
    dc = GetDC(swapchain->desc.OutputWindow);
    if (!dc) {
        free(pixels);
        return HRESULT_FROM_WIN32(GetLastError());
    }
    result = StretchDIBits(dc, 0, 0, (int)width, (int)height, 0, 0,
                           (int)width, (int)height, pixels, &bitmap,
                           DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR
        ? E_FAIL : S_OK;
    ReleaseDC(swapchain->desc.OutputWindow, dc);
    free(pixels);
    return result;
}

static HRESULT WINAPI native_d3d12_swapchain_present(
    IDXGISwapChain* self, UINT sync_interval, UINT flags) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    NativeD3d12Resource* resource;
    ID3D12CommandAllocator* allocator = NULL;
    ID3D12GraphicsCommandList* list_interface = NULL;
    NativeD3d12List* list;
    ID3D12CommandList* command_list;
    uint64_t fence_value;
    HRESULT result;
    if (sync_interval > 4u || (flags & ~DXGI_PRESENT_TEST) != 0u)
        return E_INVALIDARG;
    if ((flags & DXGI_PRESENT_TEST) != 0u) return S_OK;
    resource = swapchain->buffers[swapchain->current_buffer];
    if (!resource) return E_FAIL;
    /* The transition helper needs a command-list owner.  Keep presentation
     * ordering explicit and record the barrier on the same list as Present. */
    result = ID3D12Device_CreateCommandAllocator(
        &swapchain->device->iface, D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator, (void**)&allocator);
    if (FAILED(result)) return result;
    result = ID3D12Device_CreateCommandList(
        &swapchain->device->iface, 1u, D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator, NULL, &IID_ID3D12GraphicsCommandList,
        (void**)&list_interface);
    if (FAILED(result)) {
        ID3D12CommandAllocator_Release(allocator);
        return result;
    }
    list = native_list(list_interface);
    if (resource->state != D3D12_RESOURCE_STATE_PRESENT)
        result = native_result(native_list_transition_target(
            list, resource, D3D12_RESOURCE_STATE_PRESENT));
    if (SUCCEEDED(result))
        result = native_result(rindx_d3d12_present(
            &list->core, resource->handle, RIN_GPU_PRIMARY_DISPLAY));
    if (SUCCEEDED(result)) result = ID3D12GraphicsCommandList_Close(list_interface);
    command_list = (ID3D12CommandList*)list_interface;
    if (SUCCEEDED(result)) {
        ID3D12CommandQueue_ExecuteCommandLists(
            &swapchain->queue->iface, 1u, &command_list);
        result = native_result(swapchain->queue->last_result);
    }
    if (SUCCEEDED(result)) {
        fence_value = swapchain->device->core.submission_value;
        result = native_result(rindx_d3d12_wait(
            &swapchain->device->core, fence_value, UINT64_MAX));
    }
    ID3D12GraphicsCommandList_Release(list_interface);
    ID3D12CommandAllocator_Release(allocator);
    if (FAILED(result)) return result;
    result = native_d3d12_swapchain_run_transition(
        swapchain, resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
    if (FAILED(result)) return result;
    result = native_d3d12_swapchain_present_pixels(swapchain, resource);
    if (FAILED(result)) return result;
    result = native_d3d12_swapchain_run_transition(
        swapchain, resource, D3D12_RESOURCE_STATE_PRESENT);
    if (FAILED(result)) return result;
    swapchain->current_buffer = (swapchain->current_buffer + 1u) %
                                swapchain->buffer_count;
    ++swapchain->present_count;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_buffer(
    IDXGISwapChain* self, UINT index, REFIID iid, void** out) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    if (!out) return E_POINTER;
    *out = NULL;
    if (index >= swapchain->buffer_count || !swapchain->buffers[index])
        return DXGI_ERROR_NOT_FOUND;
    return ID3D12Resource_QueryInterface(&swapchain->buffers[index]->iface,
                                         iid, out);
}

static HRESULT WINAPI native_d3d12_swapchain_set_fullscreen(
    IDXGISwapChain* self, BOOL fullscreen, IDXGIOutput* target) {
    (void)target;
    native_d3d12_swapchain(self)->desc.Windowed = fullscreen ? FALSE : TRUE;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_fullscreen(
    IDXGISwapChain* self, BOOL* fullscreen, IDXGIOutput** target) {
    if (!fullscreen) return E_POINTER;
    *fullscreen = native_d3d12_swapchain(self)->desc.Windowed ? FALSE : TRUE;
    if (target) *target = NULL;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_desc(
    IDXGISwapChain* self, DXGI_SWAP_CHAIN_DESC* out) {
    if (!out) return E_POINTER;
    *out = native_d3d12_swapchain(self)->desc;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_resize_buffers(
    IDXGISwapChain* self, UINT count, UINT width, UINT height,
    DXGI_FORMAT format, UINT flags) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    if (count == 0u) count = swapchain->buffer_count;
    if (width == 0u) width = swapchain->desc.BufferDesc.Width;
    if (height == 0u) height = swapchain->desc.BufferDesc.Height;
    if (format == DXGI_FORMAT_UNKNOWN) format = swapchain->desc.BufferDesc.Format;
    if (count < RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS ||
        count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS || width == 0u ||
        height == 0u || native_format(format) == 0u ||
        (flags & ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0u)
        return E_INVALIDARG;
    {
        UINT index;
        for (index = 0u; index < swapchain->buffer_count; ++index)
            if (swapchain->buffers[index] &&
                swapchain->buffers[index]->references > 1)
                return DXGI_ERROR_INVALID_CALL;
    }
    native_d3d12_swapchain_destroy_buffers(swapchain);
    swapchain->desc.BufferCount = count;
    swapchain->desc.BufferDesc.Width = width;
    swapchain->desc.BufferDesc.Height = height;
    swapchain->desc.BufferDesc.Format = format;
    swapchain->desc.Flags = flags;
    swapchain->buffer_count = count;
    swapchain->present_count = 0u;
    return native_d3d12_swapchain_create_buffers(swapchain);
}

static HRESULT WINAPI native_d3d12_swapchain_resize_target(
    IDXGISwapChain* self, const DXGI_MODE_DESC* target) {
    NativeD3d12Swapchain* swapchain = native_d3d12_swapchain(self);
    if (!target || target->Width == 0u || target->Height == 0u ||
        native_format(target->Format) == 0u)
        return E_INVALIDARG;
    swapchain->desc.BufferDesc = *target;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_output(
    IDXGISwapChain* self, IDXGIOutput** out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = NULL;
    return DXGI_ERROR_NOT_FOUND;
}

static HRESULT WINAPI native_d3d12_swapchain_get_frame_statistics(
    IDXGISwapChain* self, DXGI_FRAME_STATISTICS* out) {
    if (!out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    out->PresentCount = native_d3d12_swapchain(self)->present_count;
    out->SyncRefreshCount = out->PresentCount;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_last_present_count(
    IDXGISwapChain* self, UINT* out) {
    if (!out) return E_POINTER;
    *out = native_d3d12_swapchain(self)->present_count;
    return S_OK;
}

static HRESULT WINAPI native_d3d12_swapchain_get_containing_output(
    IDXGISwapChain* self, IDXGIOutput** out) {
    return native_d3d12_swapchain_get_output(self, out);
}

static HRESULT native_d3d12_create_swapchain(
    ID3D12CommandQueue* queue_interface, const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** out) {
    NativeD3d12Queue* queue = queue_interface
        ? native_queue(queue_interface) : NULL;
    NativeD3d12Swapchain* swapchain;
    UINT count;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!queue || !desc || !desc->OutputWindow || !IsWindow(desc->OutputWindow) ||
        desc->BufferDesc.Width == 0u || desc->BufferDesc.Height == 0u ||
        native_format(desc->BufferDesc.Format) == 0u ||
        desc->SampleDesc.Count != 1u || desc->SampleDesc.Quality != 0u ||
        (desc->SwapEffect != DXGI_SWAP_EFFECT_DISCARD &&
         desc->SwapEffect != DXGI_SWAP_EFFECT_SEQUENTIAL))
        return E_INVALIDARG;
    count = desc->BufferCount == 0u ? RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS
                                    : desc->BufferCount;
    if (count < RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS ||
        count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS)
        return E_INVALIDARG;
    swapchain = (NativeD3d12Swapchain*)calloc(1u, sizeof(*swapchain));
    if (!swapchain) return E_OUTOFMEMORY;
    swapchain->iface.lpVtbl = (IDXGISwapChainVtbl*)(void*)&native_d3d12_swapchain_vtable;
    swapchain->references = 1;
    swapchain->device = queue->device;
    swapchain->queue = queue;
    swapchain->desc = *desc;
    swapchain->desc.BufferCount = count;
    swapchain->buffer_count = count;
    ID3D12Device_AddRef(&swapchain->device->iface);
    ID3D12CommandQueue_AddRef(&swapchain->queue->iface);
    if (FAILED(native_d3d12_swapchain_create_buffers(swapchain))) {
        ID3D12CommandQueue_Release(&swapchain->queue->iface);
        ID3D12Device_Release(&swapchain->device->iface);
        free(swapchain);
        return E_FAIL;
    }
    *out = &swapchain->iface;
    return S_OK;
}

HRESULT rindx_native_d3d12_create_swapchain_for_queue(
    ID3D12CommandQueue* queue, const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** out) {
    return native_d3d12_create_swapchain(queue, desc, out);
}

static IDXGISwapChainVtbl native_d3d12_swapchain_vtable = {
    .QueryInterface = native_d3d12_swapchain_query_interface,
    .AddRef = native_d3d12_swapchain_add_ref,
    .Release = native_d3d12_swapchain_release,
    .GetPrivateData = native_d3d12_swapchain_get_private_data,
    .SetPrivateData = native_d3d12_swapchain_set_private_data,
    .SetPrivateDataInterface = native_d3d12_swapchain_set_private_interface,
    .GetParent = native_d3d12_swapchain_get_parent,
    .GetDevice = native_d3d12_swapchain_get_device,
    .Present = native_d3d12_swapchain_present,
    .GetBuffer = native_d3d12_swapchain_get_buffer,
    .SetFullscreenState = native_d3d12_swapchain_set_fullscreen,
    .GetFullscreenState = native_d3d12_swapchain_get_fullscreen,
    .GetDesc = native_d3d12_swapchain_get_desc,
    .ResizeBuffers = native_d3d12_swapchain_resize_buffers,
    .ResizeTarget = native_d3d12_swapchain_resize_target,
    .GetContainingOutput = native_d3d12_swapchain_get_containing_output,
    .GetFrameStatistics = native_d3d12_swapchain_get_frame_statistics,
    .GetLastPresentCount = native_d3d12_swapchain_get_last_present_count,
};

static ID3D12CommandQueueVtbl native_queue_vtable = {
    .QueryInterface = native_query_queue_interface,
    .AddRef = native_queue_add_ref,
    .Release = native_queue_release,
    .GetPrivateData = native_queue_get_private_data,
    .SetPrivateData = native_queue_set_private_data,
    .SetPrivateDataInterface = native_queue_set_private_interface,
    .SetName = native_queue_set_name,
    .GetDevice = native_queue_get_device,
    .ExecuteCommandLists = native_queue_execute,
    .Signal = native_queue_signal,
    .Wait = native_queue_wait,
    .GetTimestampFrequency = native_queue_get_timestamp_frequency,
    .GetClockCalibration = native_queue_get_clock_calibration,
    .GetDesc = native_queue_get_desc,
};

static ID3D12CommandAllocatorVtbl native_allocator_vtable = {
    .QueryInterface = native_query_allocator_interface,
    .AddRef = native_allocator_add_ref,
    .Release = native_allocator_release,
    .GetPrivateData = native_allocator_get_private_data,
    .SetPrivateData = native_allocator_set_private_data,
    .SetPrivateDataInterface = native_allocator_set_private_interface,
    .SetName = native_allocator_set_name,
    .GetDevice = native_allocator_get_device,
    .Reset = native_allocator_reset,
};

static ID3D12GraphicsCommandListVtbl native_list_vtable = {
    .QueryInterface = native_query_list_interface,
    .AddRef = native_list_add_ref,
    .Release = native_list_release,
    .GetPrivateData = native_list_get_private_data,
    .SetPrivateData = native_list_set_private_data,
    .SetPrivateDataInterface = native_list_set_private_interface,
    .SetName = native_list_set_name,
    .GetDevice = native_list_get_device,
    .GetType = native_list_get_type,
    .Close = native_list_close,
    .Reset = native_list_reset,
    .ClearState = native_list_clear_state,
    .SetPipelineState = native_list_set_pipeline_state,
    .DrawInstanced = native_list_draw,
    .DrawIndexedInstanced = native_list_draw_indexed,
    .Dispatch = native_list_dispatch,
    .CopyBufferRegion = native_list_copy_buffer,
    .CopyResource = native_list_copy_resource,
    .ResolveSubresource = native_list_resolve_subresource,
    .IASetPrimitiveTopology = native_list_set_topology,
    .ResourceBarrier = native_list_resource_barrier,
    .IASetIndexBuffer = native_list_ia_set_index_buffer,
    .IASetVertexBuffers = native_list_ia_set_vertex_buffers,
    .OMSetRenderTargets = native_list_om_set_render_targets,
    .ClearDepthStencilView = native_list_clear_dsv,
    .ClearRenderTargetView = native_list_clear_rtv,
};

static ID3D12FenceVtbl native_fence_vtable = {
    .QueryInterface = native_query_fence_interface,
    .AddRef = native_fence_add_ref,
    .Release = native_fence_release,
    .GetPrivateData = native_fence_get_private_data,
    .SetPrivateData = native_fence_set_private_data,
    .SetPrivateDataInterface = native_fence_set_private_interface,
    .SetName = native_fence_set_name,
    .GetDevice = native_fence_get_device,
    .GetCompletedValue = native_fence_get_completed_value,
    .SetEventOnCompletion = native_fence_set_event,
    .Signal = native_fence_signal,
};

static ID3D12ResourceVtbl native_resource_vtable = {
    .QueryInterface = native_resource_query,
    .AddRef = native_resource_add_ref,
    .Release = native_resource_release,
    .GetPrivateData = native_resource_get_private_data,
    .SetPrivateData = native_resource_set_private_data,
    .SetPrivateDataInterface = native_resource_set_private_interface,
    .SetName = native_resource_set_name,
    .GetDevice = native_resource_get_device,
    .Map = native_resource_map,
    .Unmap = native_resource_unmap,
    .GetDesc = native_resource_get_desc,
    .GetGPUVirtualAddress = native_resource_gpu_address,
    .WriteToSubresource = native_resource_write_subresource,
    .ReadFromSubresource = native_resource_read_subresource,
    .GetHeapProperties = native_resource_get_heap_properties,
};

static ID3D12DescriptorHeapVtbl native_descriptor_heap_vtable = {
    .QueryInterface = native_descriptor_heap_query,
    .AddRef = native_descriptor_heap_add_ref,
    .Release = native_descriptor_heap_release,
    .GetPrivateData = native_descriptor_heap_get_private_data,
    .SetPrivateData = native_descriptor_heap_set_private_data,
    .SetPrivateDataInterface = native_descriptor_heap_set_private_interface,
    .SetName = native_descriptor_heap_set_name,
    .GetDevice = native_descriptor_heap_get_device,
    .GetDesc = native_descriptor_heap_get_desc,
    .GetCPUDescriptorHandleForHeapStart = native_descriptor_heap_get_cpu_start,
    .GetGPUDescriptorHandleForHeapStart = native_descriptor_heap_get_gpu_start,
};

static ID3D12PipelineStateVtbl native_pipeline_vtable = {
    .QueryInterface = native_pipeline_query_interface,
    .AddRef = native_pipeline_add_ref,
    .Release = native_pipeline_release,
    .GetPrivateData = native_pipeline_get_private_data,
    .SetPrivateData = native_pipeline_set_private_data,
    .SetPrivateDataInterface = native_pipeline_set_private_interface,
    .SetName = native_pipeline_set_name,
    .GetDevice = native_pipeline_get_device,
    .GetCachedBlob = native_pipeline_get_cached_blob,
};

static ID3D10BlobVtbl native_blob_vtable = {
    .QueryInterface = native_blob_query_interface,
    .AddRef = native_blob_add_ref,
    .Release = native_blob_release,
    .GetBufferPointer = native_blob_get_buffer_pointer,
    .GetBufferSize = native_blob_get_buffer_size,
};

static ID3D12DeviceVtbl native_vtable = {
    .QueryInterface = native_query_interface,
    .AddRef = native_add_ref,
    .Release = native_release,
    .GetPrivateData = native_device_get_private_data,
    .SetPrivateData = native_device_set_private_data,
    .SetPrivateDataInterface = native_device_set_private_interface,
    .SetName = native_device_set_name,
    .GetNodeCount = native_get_node_count,
    .CreateCommandQueue = native_device_create_queue,
    .CreateCommandAllocator = native_device_create_allocator,
    .CreateCommandList = native_device_create_list,
    .CreateGraphicsPipelineState = native_device_create_graphics_pipeline_state,
    .CreateComputePipelineState = native_device_create_compute_pipeline_state,
    .CheckFeatureSupport = native_check_feature_support,
    .CreateDescriptorHeap = native_device_create_descriptor_heap,
    .GetDescriptorHandleIncrementSize = native_descriptor_increment_size,
    .CreateConstantBufferView = native_device_create_cbv,
    .CreateShaderResourceView = native_device_create_srv,
    .CreateUnorderedAccessView = native_device_create_uav,
    .CreateRenderTargetView = native_device_create_rtv,
    .CreateDepthStencilView = native_device_create_dsv,
    .CreateSampler = native_device_create_sampler,
    .CreateCommittedResource = native_device_create_committed_resource,
    .CreateFence = native_device_create_fence,
    .GetDeviceRemovedReason = native_removed_reason,
};

#define RINDX_NATIVE_EXPORT

RINDX_NATIVE_EXPORT HRESULT WINAPI D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL minimum_level,
                                 REFIID iid, void** device_out) {
    RinGpuRuntimeDescV1 surface;
    const uint32_t level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;
    NativeD3d12Device* device;
    int result;
    (void)adapter;
    if (!device_out) return E_POINTER;
    *device_out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) &&
        !IsEqualIID(iid, &IID_ID3D12Device)) return E_NOINTERFACE;
    if (minimum_level > D3D_FEATURE_LEVEL_12_0) return E_INVALIDARG;
    native_surface_init(&surface);
    device = (NativeD3d12Device*)calloc(1u, sizeof(*device));
    if (!device) return E_OUTOFMEMORY;
    result = rindx_d3d12_create_device(&surface, &level, 1u, &device->core);
    if (result != RIN_GPU_OK) { free(device); return native_result(result); }
    device->iface.lpVtbl = (ID3D12DeviceVtbl*)(void*)&native_vtable;
    device->references = 1;
    *device_out = &device->iface;
    return S_OK;
}

#endif /* _WIN32 */
