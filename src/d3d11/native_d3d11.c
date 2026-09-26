/* SPDX-License-Identifier: MIT */
/*
 * Windows ABI adapter for the RinDX D3D11 owner.
 *
 * This file is deliberately Windows-only.  The ABI objects use the SDK's
 * ID3D11* vtables, while every resource and command is lowered to the
 * validated RinDX/RinGPU software owner.  No native GPU call is made here.
 * Methods outside the currently representable RinGPU profile return the
 * documented HRESULT instead of manufacturing an object or completion.
 */
#if defined(_WIN32)

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>

#include <rindx/d3d11.h>

#include <stdlib.h>
#include <string.h>

typedef struct NativeD3d11Device NativeD3d11Device;
typedef struct NativeD3d11Context NativeD3d11Context;
typedef struct NativeD3d11View NativeD3d11View;
typedef struct NativeD3d11InputLayout NativeD3d11InputLayout;
typedef struct NativeD3d11CommandList NativeD3d11CommandList;
typedef struct NativeD3d11Swapchain NativeD3d11Swapchain;

typedef struct NativeD3d11PrivateData {
    GUID guid;
    BYTE* bytes;
    UINT size;
    IUnknown* interface_value;
    struct NativeD3d11PrivateData* next;
} NativeD3d11PrivateData;

typedef enum NativeD3d11ResourceKind {
    NATIVE_D3D11_BUFFER = 1,
    NATIVE_D3D11_TEXTURE1D = 2,
    NATIVE_D3D11_TEXTURE2D = 3,
    NATIVE_D3D11_TEXTURE3D = 4
} NativeD3d11ResourceKind;

typedef struct NativeD3d11Resource {
    ID3D11Buffer iface;
    LONG references;
    NativeD3d11Device* device;
    RinGpuHandle handle;
    NativeD3d11ResourceKind kind;
    D3D11_BUFFER_DESC buffer_desc;
    D3D11_TEXTURE1D_DESC texture1d_desc;
    D3D11_TEXTURE2D_DESC texture2d_desc;
    D3D11_TEXTURE3D_DESC texture3d_desc;
    UINT eviction_priority;
    uint32_t image_state;
    NativeD3d11PrivateData* private_data;
} NativeD3d11Resource;

typedef struct NativeD3d11Shader {
    ID3D11DeviceChild iface;
    LONG references;
    NativeD3d11Device* device;
    RinGpuHandle handle;
    UINT stage;
    NativeD3d11PrivateData* private_data;
} NativeD3d11Shader;

typedef struct NativeD3d11Sampler {
    ID3D11DeviceChild iface;
    LONG references;
    NativeD3d11Device* device;
    RinGpuHandle handle;
    NativeD3d11PrivateData* private_data;
} NativeD3d11Sampler;

typedef enum NativeD3d11ViewKind {
    NATIVE_D3D11_VIEW_SRV = 1,
    NATIVE_D3D11_VIEW_UAV = 2,
    NATIVE_D3D11_VIEW_RTV = 3,
    NATIVE_D3D11_VIEW_DSV = 4
} NativeD3d11ViewKind;

struct NativeD3d11View {
    ID3D11View iface;
    LONG references;
    NativeD3d11Device* device;
    NativeD3d11Resource* resource;
    NativeD3d11ViewKind kind;
    NativeD3d11PrivateData* private_data;
};

struct NativeD3d11InputLayout {
    ID3D11InputLayout iface;
    LONG references;
    NativeD3d11Device* device;
    UINT attribute_count;
    RinGpuVertexAttributeV2 attributes[16];
    RinGpuVertexBufferLayoutV1 layouts[16];
    UINT layout_count;
    NativeD3d11PrivateData* private_data;
};

struct NativeD3d11CommandList {
    ID3D11CommandList iface;
    LONG references;
    NativeD3d11Device* device;
    RinGpuHandle handle;
    UINT context_flags;
    NativeD3d11PrivateData* private_data;
};

struct NativeD3d11Swapchain {
    IDXGISwapChain iface;
    LONG references;
    NativeD3d11Device* device;
    HWND window;
    DXGI_SWAP_CHAIN_DESC desc;
    RinGpuDxgiSwapchainRuntime core;
    NativeD3d11Resource* buffers[RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS];
    uint32_t buffer_count;
    uint32_t current_buffer;
    UINT present_count;
    NativeD3d11PrivateData* private_data;
};

struct NativeD3d11Context {
    ID3D11DeviceContext iface;
    LONG references;
    NativeD3d11Device* device;
    RinDxD3d11Context core;
    RinDxD3d11MappedResource mapped;
    NativeD3d11Resource* current_target;
    NativeD3d11View* color_view;
    NativeD3d11View* depth_view;
    NativeD3d11Shader* vertex_shader;
    NativeD3d11Shader* pixel_shader;
    NativeD3d11Shader* compute_shader;
    NativeD3d11InputLayout* input_layout;
    NativeD3d11Resource* vertex_buffers[16];
    UINT vertex_strides[16];
    UINT vertex_offsets[16];
    NativeD3d11Resource* index_buffer;
    UINT index_offset;
    DXGI_FORMAT index_format;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    RinGpuHandle graphics_pipeline;
    RinGpuHandle compute_pipeline;
    RinGpuHandle bind_group;
};

struct NativeD3d11Device {
    ID3D11Device iface;
    LONG references;
    UINT creation_flags;
    NativeD3d11Context* immediate;
    RinDxD3d11Device core;
};

static NativeD3d11PrivateData* native_private_find(
    NativeD3d11PrivateData* head, REFGUID guid) {
    while (head) {
        if (IsEqualGUID(&head->guid, guid)) return head;
        head = head->next;
    }
    return NULL;
}

static void native_private_dispose(NativeD3d11PrivateData** head) {
    NativeD3d11PrivateData* item;
    NativeD3d11PrivateData* next;
    if (!head) return;
    item = *head;
    while (item) {
        next = item->next;
        if (item->interface_value)
            item->interface_value->lpVtbl->Release(item->interface_value);
        free(item->bytes);
        free(item);
        item = next;
    }
    *head = NULL;
}

static HRESULT native_private_get(NativeD3d11PrivateData* head,
                                  REFGUID guid, UINT* size, void* data) {
    NativeD3d11PrivateData* item;
    UINT required;
    if (!guid || !size) return E_POINTER;
    item = native_private_find(head, guid);
    if (!item) return DXGI_ERROR_NOT_FOUND;
    required = item->interface_value ? (UINT)sizeof(IUnknown*) : item->size;
    if (!data || *size < required) {
        *size = required;
        return data ? DXGI_ERROR_MORE_DATA : E_INVALIDARG;
    }
    if (item->interface_value) {
        *(IUnknown**)data = item->interface_value;
        item->interface_value->lpVtbl->AddRef(item->interface_value);
    } else if (item->size != 0u) {
        memcpy(data, item->bytes, item->size);
    }
    *size = required;
    return S_OK;
}

static HRESULT native_private_set(NativeD3d11PrivateData** head,
                                  REFGUID guid, UINT size, const void* data) {
    NativeD3d11PrivateData** cursor;
    NativeD3d11PrivateData* item;
    BYTE* copy = NULL;
    if (!head || !guid || (size != 0u && !data)) return E_INVALIDARG;
    cursor = head;
    while (*cursor && !IsEqualGUID(&(*cursor)->guid, guid))
        cursor = &(*cursor)->next;
    if (size == 0u) {
        if (*cursor) {
            item = *cursor;
            *cursor = item->next;
            if (item->interface_value)
                item->interface_value->lpVtbl->Release(item->interface_value);
            free(item->bytes);
            free(item);
        }
        return S_OK;
    }
    copy = (BYTE*)malloc(size);
    if (!copy) return E_OUTOFMEMORY;
    memcpy(copy, data, size);
    if (!*cursor) {
        item = (NativeD3d11PrivateData*)calloc(1u, sizeof(*item));
        if (!item) { free(copy); return E_OUTOFMEMORY; }
        item->guid = *guid;
        item->next = NULL;
        *cursor = item;
    } else {
        item = *cursor;
        if (item->interface_value) {
            item->interface_value->lpVtbl->Release(item->interface_value);
            item->interface_value = NULL;
        }
        free(item->bytes);
    }
    item->bytes = copy;
    item->size = size;
    return S_OK;
}

static HRESULT native_private_set_interface(NativeD3d11PrivateData** head,
                                             REFGUID guid,
                                             const IUnknown* object) {
    NativeD3d11PrivateData** cursor;
    NativeD3d11PrivateData* item;
    if (!head || !guid) return E_INVALIDARG;
    if (!object) return native_private_set(head, guid, 0u, NULL);
    cursor = head;
    while (*cursor && !IsEqualGUID(&(*cursor)->guid, guid))
        cursor = &(*cursor)->next;
    if (!*cursor) {
        item = (NativeD3d11PrivateData*)calloc(1u, sizeof(*item));
        if (!item) return E_OUTOFMEMORY;
        item->guid = *guid;
        *cursor = item;
    } else {
        item = *cursor;
        if (item->interface_value)
            item->interface_value->lpVtbl->Release(item->interface_value);
        free(item->bytes);
        item->bytes = NULL;
        item->size = 0u;
    }
    item->interface_value = (IUnknown*)(void*)object;
    item->interface_value->lpVtbl->AddRef(item->interface_value);
    return S_OK;
}

static HRESULT WINAPI native_device_query_interface(
    ID3D11Device* self, REFIID iid, void** object_out);
static ULONG WINAPI native_device_add_ref(ID3D11Device* self);
static ULONG WINAPI native_device_release(ID3D11Device* self);
static HRESULT WINAPI native_device_create_buffer(
    ID3D11Device* self, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Buffer** out);
static HRESULT WINAPI native_device_create_texture2d(
    ID3D11Device* self, const D3D11_TEXTURE2D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture2D** out);
static HRESULT WINAPI native_device_create_deferred_context(
    ID3D11Device* self, UINT flags, ID3D11DeviceContext** out);
static HRESULT WINAPI native_device_check_feature_support(
    ID3D11Device* self, D3D11_FEATURE feature, void* data, UINT size);
static D3D_FEATURE_LEVEL WINAPI native_device_get_feature_level(
    ID3D11Device* self);
static UINT WINAPI native_device_get_creation_flags(ID3D11Device* self);
static HRESULT WINAPI native_device_get_removed_reason(ID3D11Device* self);
static void WINAPI native_device_get_immediate_context(
    ID3D11Device* self, ID3D11DeviceContext** out);
static HRESULT WINAPI native_device_create_texture1d(
    ID3D11Device* self, const D3D11_TEXTURE1D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture1D** out);
static HRESULT WINAPI native_device_create_texture3d(
    ID3D11Device* self, const D3D11_TEXTURE3D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture3D** out);
static HRESULT WINAPI native_device_create_shader_resource_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
    ID3D11ShaderResourceView** out);
static HRESULT WINAPI native_device_create_unordered_access_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
    ID3D11UnorderedAccessView** out);
static HRESULT WINAPI native_device_create_render_target_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc,
    ID3D11RenderTargetView** out);
static HRESULT WINAPI native_device_create_depth_stencil_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
    ID3D11DepthStencilView** out);
static HRESULT WINAPI native_device_create_input_layout(
    ID3D11Device* self, const D3D11_INPUT_ELEMENT_DESC* elements,
    UINT count, const void* bytecode, SIZE_T bytecode_size,
    ID3D11InputLayout** out);
static HRESULT WINAPI native_device_create_vertex_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11VertexShader** out);
static HRESULT WINAPI native_device_create_pixel_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11PixelShader** out);
static HRESULT WINAPI native_device_create_compute_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11ComputeShader** out);
static HRESULT WINAPI native_device_create_sampler_state(
    ID3D11Device* self, const D3D11_SAMPLER_DESC* desc,
    ID3D11SamplerState** out);

static HRESULT WINAPI native_context_query_interface(
    ID3D11DeviceContext* self, REFIID iid, void** object_out);
static ULONG WINAPI native_context_add_ref(ID3D11DeviceContext* self);
static ULONG WINAPI native_context_release(ID3D11DeviceContext* self);
static void WINAPI native_context_get_device(
    ID3D11DeviceContext* self, ID3D11Device** out);
static void native_context_release_state(NativeD3d11Context* context);
static void WINAPI native_context_draw(
    ID3D11DeviceContext* self, UINT vertex_count, UINT first_vertex);
static void WINAPI native_context_draw_indexed(
    ID3D11DeviceContext* self, UINT index_count, UINT first_index,
    INT base_vertex);
static void WINAPI native_context_draw_instanced(
    ID3D11DeviceContext* self, UINT vertex_count, UINT instance_count,
    UINT first_vertex, UINT first_instance);
static void WINAPI native_context_draw_indexed_instanced(
    ID3D11DeviceContext* self, UINT index_count, UINT instance_count,
    UINT first_index, INT base_vertex, UINT first_instance);
static HRESULT WINAPI native_context_map(
    ID3D11DeviceContext* self, ID3D11Resource* resource, UINT subresource,
    D3D11_MAP map_type, UINT flags, D3D11_MAPPED_SUBRESOURCE* mapped);
static void WINAPI native_context_unmap(
    ID3D11DeviceContext* self, ID3D11Resource* resource, UINT subresource);
static void WINAPI native_context_copy_resource(
    ID3D11DeviceContext* self, ID3D11Resource* destination,
    ID3D11Resource* source);
static void WINAPI native_context_copy_subresource_region(
    ID3D11DeviceContext* self, ID3D11Resource* destination,
    UINT destination_subresource, UINT dst_x, UINT dst_y, UINT dst_z,
    ID3D11Resource* source, UINT source_subresource, const D3D11_BOX* box);
static void WINAPI native_context_update_subresource(
    ID3D11DeviceContext* self, ID3D11Resource* resource, UINT subresource,
    const D3D11_BOX* box, const void* data, UINT row_pitch, UINT depth_pitch);
static void WINAPI native_context_clear_rtv(
    ID3D11DeviceContext* self, ID3D11RenderTargetView* view,
    const FLOAT color[4]);
static void WINAPI native_context_clear_dsv(
    ID3D11DeviceContext* self, ID3D11DepthStencilView* view,
    UINT flags, FLOAT depth, UINT8 stencil);
static void WINAPI native_context_generate_mips(
    ID3D11DeviceContext* self, ID3D11ShaderResourceView* view);
static void WINAPI native_context_resolve(
    ID3D11DeviceContext* self, ID3D11Resource* destination,
    UINT destination_subresource, ID3D11Resource* source,
    UINT source_subresource, DXGI_FORMAT format);
static void WINAPI native_context_dispatch(
    ID3D11DeviceContext* self, UINT x, UINT y, UINT z);
static HRESULT WINAPI native_context_finish_command_list(
    ID3D11DeviceContext* self, BOOL restore, ID3D11CommandList** out);
static void WINAPI native_context_clear_state(ID3D11DeviceContext* self);
static void WINAPI native_context_flush(ID3D11DeviceContext* self);
static UINT WINAPI native_context_get_flags(ID3D11DeviceContext* self);
static void WINAPI native_context_om_set_render_targets(
    ID3D11DeviceContext* self, UINT count,
    ID3D11RenderTargetView* const* targets, ID3D11DepthStencilView* depth);
static void WINAPI native_context_ia_set_input_layout(
    ID3D11DeviceContext* self, ID3D11InputLayout* layout);
static void WINAPI native_context_ia_set_vertex_buffers(
    ID3D11DeviceContext* self, UINT start, UINT count,
    ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets);
static void WINAPI native_context_ia_set_index_buffer(
    ID3D11DeviceContext* self, ID3D11Buffer* buffer, DXGI_FORMAT format,
    UINT offset);
static void WINAPI native_context_vs_set_shader(
    ID3D11DeviceContext* self, ID3D11VertexShader* shader,
    ID3D11ClassInstance* const* instances, UINT count);
static void WINAPI native_context_ps_set_shader(
    ID3D11DeviceContext* self, ID3D11PixelShader* shader,
    ID3D11ClassInstance* const* instances, UINT count);
static void WINAPI native_context_cs_set_shader(
    ID3D11DeviceContext* self, ID3D11ComputeShader* shader,
    ID3D11ClassInstance* const* instances, UINT count);
static void WINAPI native_context_ia_set_primitive_topology(
    ID3D11DeviceContext* self, D3D11_PRIMITIVE_TOPOLOGY topology);
static void WINAPI native_context_execute_command_list(
    ID3D11DeviceContext* self, ID3D11CommandList* list, BOOL restore);
static HRESULT WINAPI native_view_query_interface(
    ID3D11View* self, REFIID iid, void** out);
static ULONG WINAPI native_view_add_ref(ID3D11View* self);
static ULONG WINAPI native_view_release(ID3D11View* self);
static void WINAPI native_view_get_device(ID3D11View* self, ID3D11Device** out);
static HRESULT WINAPI native_view_get_private_data(
    ID3D11View* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_view_set_private_data(
    ID3D11View* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_view_set_private_interface(
    ID3D11View* self, REFGUID guid, const IUnknown* object);
static void WINAPI native_view_get_resource(ID3D11View* self,
                                            ID3D11Resource** out);
static HRESULT WINAPI native_shader_query_interface(
    ID3D11DeviceChild* self, REFIID iid, void** out);
static ULONG WINAPI native_shader_add_ref(ID3D11DeviceChild* self);
static ULONG WINAPI native_shader_release(ID3D11DeviceChild* self);
static void WINAPI native_shader_get_device(ID3D11DeviceChild* self,
                                            ID3D11Device** out);
static HRESULT WINAPI native_shader_get_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_shader_set_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_shader_set_private_interface(
    ID3D11DeviceChild* self, REFGUID guid, const IUnknown* object);
static HRESULT WINAPI native_layout_query_interface(
    ID3D11InputLayout* self, REFIID iid, void** out);
static ULONG WINAPI native_layout_add_ref(ID3D11InputLayout* self);
static ULONG WINAPI native_layout_release(ID3D11InputLayout* self);
static void WINAPI native_layout_get_device(ID3D11InputLayout* self,
                                            ID3D11Device** out);
static HRESULT WINAPI native_layout_get_private_data(
    ID3D11InputLayout* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_layout_set_private_data(
    ID3D11InputLayout* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_layout_set_private_interface(
    ID3D11InputLayout* self, REFGUID guid, const IUnknown* object);
static HRESULT WINAPI native_sampler_query_interface(
    ID3D11DeviceChild* self, REFIID iid, void** out);
static ULONG WINAPI native_sampler_add_ref(ID3D11DeviceChild* self);
static ULONG WINAPI native_sampler_release(ID3D11DeviceChild* self);
static void WINAPI native_sampler_get_device(ID3D11DeviceChild* self,
                                             ID3D11Device** out);
static HRESULT WINAPI native_sampler_get_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_sampler_set_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_sampler_set_private_interface(
    ID3D11DeviceChild* self, REFGUID guid, const IUnknown* object);
static HRESULT WINAPI native_command_list_query_interface(
    ID3D11CommandList* self, REFIID iid, void** out);
static ULONG WINAPI native_command_list_add_ref(ID3D11CommandList* self);
static ULONG WINAPI native_command_list_release(ID3D11CommandList* self);
static void WINAPI native_command_list_get_device(ID3D11CommandList* self,
                                                   ID3D11Device** out);
static HRESULT WINAPI native_command_list_get_private_data(
    ID3D11CommandList* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_command_list_set_private_data(
    ID3D11CommandList* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_command_list_set_private_interface(
    ID3D11CommandList* self, REFGUID guid, const IUnknown* object);
static UINT WINAPI native_command_list_get_context_flags(ID3D11CommandList* self);
static HRESULT WINAPI native_swapchain_query_interface(
    IDXGISwapChain* self, REFIID iid, void** out);
static ULONG WINAPI native_swapchain_add_ref(IDXGISwapChain* self);
static ULONG WINAPI native_swapchain_release(IDXGISwapChain* self);
static HRESULT WINAPI native_swapchain_present(
    IDXGISwapChain* self, UINT sync_interval, UINT flags);
static HRESULT WINAPI native_swapchain_get_buffer(
    IDXGISwapChain* self, UINT index, REFIID iid, void** out);
static HRESULT WINAPI native_swapchain_set_fullscreen_state(
    IDXGISwapChain* self, BOOL fullscreen, IDXGIOutput* target);
static HRESULT WINAPI native_swapchain_get_fullscreen_state(
    IDXGISwapChain* self, BOOL* fullscreen, IDXGIOutput** target);
static HRESULT WINAPI native_swapchain_get_desc(
    IDXGISwapChain* self, DXGI_SWAP_CHAIN_DESC* desc);
static HRESULT WINAPI native_swapchain_resize_buffers(
    IDXGISwapChain* self, UINT count, UINT width, UINT height,
    DXGI_FORMAT format, UINT flags);
static HRESULT WINAPI native_swapchain_resize_target(
    IDXGISwapChain* self, const DXGI_MODE_DESC* mode);
static HRESULT WINAPI native_swapchain_get_device(
    IDXGISwapChain* self, REFIID iid, void** out);
static HRESULT WINAPI native_swapchain_set_private_data(
    IDXGISwapChain* self, REFGUID guid, UINT size, const void* data);
static HRESULT WINAPI native_swapchain_set_private_interface(
    IDXGISwapChain* self, REFGUID guid, const IUnknown* object);
static HRESULT WINAPI native_swapchain_get_private_data(
    IDXGISwapChain* self, REFGUID guid, UINT* size, void* data);
static HRESULT WINAPI native_swapchain_get_parent(
    IDXGISwapChain* self, REFIID iid, void** out);
static HRESULT WINAPI native_swapchain_get_containing_output(
    IDXGISwapChain* self, IDXGIOutput** out);
static HRESULT WINAPI native_swapchain_get_frame_statistics(
    IDXGISwapChain* self, DXGI_FRAME_STATISTICS* stats);
static HRESULT WINAPI native_swapchain_get_last_present_count(
    IDXGISwapChain* self, UINT* count);
static void WINAPI native_texture1d_get_desc(ID3D11Texture1D* self,
                                             D3D11_TEXTURE1D_DESC* out);
static void WINAPI native_texture3d_get_desc(ID3D11Texture3D* self,
                                             D3D11_TEXTURE3D_DESC* out);
static NativeD3d11InputLayout* native_layout_from_interface(
    ID3D11InputLayout* self);
static uint32_t native_dxgi_image_format(DXGI_FORMAT format);

/* The complete SDK vtable is populated below.  Unsupported optional native
 * entry points are explicit ABI rejections; listed resource/command paths
 * have concrete lowerings above and are never routed through this helper. */
static HRESULT WINAPI native_hresult_reject(void) { return E_NOTIMPL; }
static void WINAPI native_void_reject(void) {}
static UINT WINAPI native_uint_reject(void) { return 0u; }

#define HRESULT_REJECT ((void*)native_hresult_reject)
#define VOID_REJECT ((void*)native_void_reject)
#define UINT_REJECT ((void*)native_uint_reject)

#if 0
static const ID3D11DeviceVtbl native_device_vtable_legacy = {
    native_device_query_interface,
    native_device_add_ref,
    native_device_release,
    native_device_create_buffer,
    (void*)native_hresult_reject,
    native_device_create_texture2d,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    native_device_create_deferred_context,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_void_reject,
    (void*)native_hresult_reject,
    native_device_check_feature_support,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    native_device_get_feature_level,
    native_device_get_creation_flags,
    native_device_get_removed_reason,
    native_device_get_immediate_context,
    (void*)native_hresult_reject,
    UINT_REJECT,
};

static const ID3D11DeviceContextVtbl native_context_vtable_legacy = {
    native_context_query_interface,
    native_context_add_ref,
    native_context_release,
    native_context_get_device,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    (void*)native_hresult_reject,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    native_context_draw_indexed,
    native_context_draw,
    native_context_map,
    native_context_unmap,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    native_context_draw_indexed_instanced,
    native_context_draw_instanced,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    native_context_dispatch,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    native_context_copy_subresource_region,
    native_context_copy_resource,
    native_context_update_subresource,
    VOID_REJECT,
    native_context_clear_rtv,
    VOID_REJECT,
    VOID_REJECT,
    native_context_clear_dsv,
    native_context_generate_mips,
    VOID_REJECT,
    native_context_resolve,
    (void*)native_void_reject,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    VOID_REJECT,
    native_context_clear_state,
    native_context_flush,
    native_context_get_flags,
    native_context_finish_command_list,
};
#endif

/* Designated initializers are kept separately from the legacy positional
 * table above so the Windows SDK can change optional method ordering without
 * moving one of the concrete command lowerings to another slot. */
static const ID3D11DeviceVtbl native_device_vtable_bound = {
    .QueryInterface = native_device_query_interface,
    .AddRef = native_device_add_ref,
    .Release = native_device_release,
    .CreateBuffer = native_device_create_buffer,
    .CreateTexture1D = native_device_create_texture1d,
    .CreateTexture2D = native_device_create_texture2d,
    .CreateTexture3D = native_device_create_texture3d,
    .CreateShaderResourceView = native_device_create_shader_resource_view,
    .CreateUnorderedAccessView = native_device_create_unordered_access_view,
    .CreateRenderTargetView = native_device_create_render_target_view,
    .CreateDepthStencilView = native_device_create_depth_stencil_view,
    .CreateInputLayout = native_device_create_input_layout,
    .CreateVertexShader = native_device_create_vertex_shader,
    .CreatePixelShader = native_device_create_pixel_shader,
    .CreateComputeShader = native_device_create_compute_shader,
    .CreateSamplerState = native_device_create_sampler_state,
    .CreateDeferredContext = native_device_create_deferred_context,
    .CheckFeatureSupport = native_device_check_feature_support,
    .GetFeatureLevel = native_device_get_feature_level,
    .GetCreationFlags = native_device_get_creation_flags,
    .GetDeviceRemovedReason = native_device_get_removed_reason,
    .GetImmediateContext = native_device_get_immediate_context,
};

static const ID3D11DeviceContextVtbl native_context_vtable_bound = {
    .QueryInterface = native_context_query_interface,
    .AddRef = native_context_add_ref,
    .Release = native_context_release,
    .GetDevice = native_context_get_device,
    .IASetInputLayout = native_context_ia_set_input_layout,
    .IASetVertexBuffers = native_context_ia_set_vertex_buffers,
    .IASetIndexBuffer = native_context_ia_set_index_buffer,
    .VSSetShader = native_context_vs_set_shader,
    .PSSetShader = native_context_ps_set_shader,
    .CSSetShader = native_context_cs_set_shader,
    .IASetPrimitiveTopology = native_context_ia_set_primitive_topology,
    .DrawIndexed = native_context_draw_indexed,
    .Draw = native_context_draw,
    .Map = native_context_map,
    .Unmap = native_context_unmap,
    .DrawIndexedInstanced = native_context_draw_indexed_instanced,
    .DrawInstanced = native_context_draw_instanced,
    .Dispatch = native_context_dispatch,
    .ExecuteCommandList = native_context_execute_command_list,
    .CopySubresourceRegion = native_context_copy_subresource_region,
    .CopyResource = native_context_copy_resource,
    .UpdateSubresource = native_context_update_subresource,
    .OMSetRenderTargets = native_context_om_set_render_targets,
    .ClearRenderTargetView = native_context_clear_rtv,
    .ClearDepthStencilView = native_context_clear_dsv,
    .GenerateMips = native_context_generate_mips,
    .ResolveSubresource = native_context_resolve,
    .ClearState = native_context_clear_state,
    .Flush = native_context_flush,
    .GetContextFlags = native_context_get_flags,
    .FinishCommandList = native_context_finish_command_list,
};

static const ID3D11ViewVtbl native_view_vtable = {
    .QueryInterface = native_view_query_interface,
    .AddRef = native_view_add_ref,
    .Release = native_view_release,
    .GetDevice = native_view_get_device,
    .GetPrivateData = native_view_get_private_data,
    .SetPrivateData = native_view_set_private_data,
    .SetPrivateDataInterface = native_view_set_private_interface,
    .GetResource = native_view_get_resource,
};

static const ID3D11DeviceChildVtbl native_shader_vtable = {
    .QueryInterface = native_shader_query_interface,
    .AddRef = native_shader_add_ref,
    .Release = native_shader_release,
    .GetDevice = native_shader_get_device,
    .GetPrivateData = native_shader_get_private_data,
    .SetPrivateData = native_shader_set_private_data,
    .SetPrivateDataInterface = native_shader_set_private_interface,
};

static const ID3D11DeviceChildVtbl native_sampler_vtable = {
    .QueryInterface = native_sampler_query_interface,
    .AddRef = native_sampler_add_ref,
    .Release = native_sampler_release,
    .GetDevice = native_sampler_get_device,
    .GetPrivateData = native_sampler_get_private_data,
    .SetPrivateData = native_sampler_set_private_data,
    .SetPrivateDataInterface = native_sampler_set_private_interface,
};

static const ID3D11CommandListVtbl native_command_list_vtable = {
    .QueryInterface = native_command_list_query_interface,
    .AddRef = native_command_list_add_ref,
    .Release = native_command_list_release,
    .GetDevice = native_command_list_get_device,
    .GetPrivateData = native_command_list_get_private_data,
    .SetPrivateData = native_command_list_set_private_data,
    .SetPrivateDataInterface = native_command_list_set_private_interface,
    .GetContextFlags = native_command_list_get_context_flags,
};

static const IDXGISwapChainVtbl native_swapchain_vtable = {
    .QueryInterface = native_swapchain_query_interface,
    .AddRef = native_swapchain_add_ref,
    .Release = native_swapchain_release,
    .SetPrivateData = native_swapchain_set_private_data,
    .SetPrivateDataInterface = native_swapchain_set_private_interface,
    .GetPrivateData = native_swapchain_get_private_data,
    .GetParent = native_swapchain_get_parent,
    .GetDevice = native_swapchain_get_device,
    .Present = native_swapchain_present,
    .GetBuffer = native_swapchain_get_buffer,
    .SetFullscreenState = native_swapchain_set_fullscreen_state,
    .GetFullscreenState = native_swapchain_get_fullscreen_state,
    .GetDesc = native_swapchain_get_desc,
    .ResizeBuffers = native_swapchain_resize_buffers,
    .ResizeTarget = native_swapchain_resize_target,
    .GetContainingOutput = native_swapchain_get_containing_output,
    .GetFrameStatistics = native_swapchain_get_frame_statistics,
    .GetLastPresentCount = native_swapchain_get_last_present_count,
};

static const ID3D11DeviceChildVtbl native_layout_vtable = {
    .QueryInterface = (HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceChild*,
                                                     REFIID, void**))
        native_layout_query_interface,
    .AddRef = (ULONG (STDMETHODCALLTYPE*)(ID3D11DeviceChild*))
        native_layout_add_ref,
    .Release = (ULONG (STDMETHODCALLTYPE*)(ID3D11DeviceChild*))
        native_layout_release,
    .GetDevice = (void (STDMETHODCALLTYPE*)(ID3D11DeviceChild*, ID3D11Device**))
        native_layout_get_device,
    .GetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceChild*,
                                                     REFGUID, UINT*, void*))
        native_layout_get_private_data,
    .SetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceChild*,
                                                     REFGUID, UINT, const void*))
        native_layout_set_private_data,
    .SetPrivateDataInterface =
        (HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceChild*, REFGUID,
                                      const IUnknown*))
            native_layout_set_private_interface,
};

static int native_surface_present(void* context,
                                  const RinGpuSoftwarePresentedImageV1* image) {
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
    surface->handle_secret = UINT64_C(0x52494e4458333131);
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
    memcpy(surface->adapter.name, "RinDX Windows software", 22u);
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

static NativeD3d11Resource* native_resource_from_buffer(ID3D11Buffer* value) {
    return (NativeD3d11Resource*)(void*)value;
}

static NativeD3d11Resource* native_resource_from_resource(ID3D11Resource* value) {
    return (NativeD3d11Resource*)(void*)value;
}

static NativeD3d11Context* native_context_from_interface(
    ID3D11DeviceContext* value) {
    return (NativeD3d11Context*)(void*)value;
}

static NativeD3d11Device* native_device_from_interface(ID3D11Device* value) {
    return (NativeD3d11Device*)(void*)value;
}

static HRESULT core_result(int result) {
    if (result == RIN_GPU_OK) return S_OK;
    if (result == RIN_GPU_ERROR_INVALID_ARGUMENT ||
        result == RIN_GPU_ERROR_BOUNDS) return E_INVALIDARG;
    if (result == RIN_GPU_ERROR_DEVICE_LOST) return DXGI_ERROR_DEVICE_REMOVED;
    if (result == RIN_GPU_ERROR_UNSUPPORTED) return E_NOTIMPL;
    return E_FAIL;
}

static HRESULT WINAPI native_resource_query_interface(
    ID3D11Buffer* self, REFIID iid, void** out) {
    NativeD3d11Resource* resource = native_resource_from_buffer(self);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ID3D11Resource) ||
        (resource->kind == NATIVE_D3D11_BUFFER &&
         IsEqualIID(iid, &IID_ID3D11Buffer)) ||
        (resource->kind == NATIVE_D3D11_TEXTURE1D &&
         IsEqualIID(iid, &IID_ID3D11Texture1D)) ||
        (resource->kind == NATIVE_D3D11_TEXTURE2D &&
         IsEqualIID(iid, &IID_ID3D11Texture2D)) ||
        (resource->kind == NATIVE_D3D11_TEXTURE3D &&
         IsEqualIID(iid, &IID_ID3D11Texture3D))) {
        *out = self;
        native_resource_from_buffer(self)->references =
            InterlockedIncrement(&native_resource_from_buffer(self)->references);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_resource_add_ref(ID3D11Buffer* self) {
    return (ULONG)InterlockedIncrement(
        &native_resource_from_buffer(self)->references);
}

static ULONG WINAPI native_resource_release(ID3D11Buffer* self) {
    NativeD3d11Resource* resource = native_resource_from_buffer(self);
    LONG references = InterlockedDecrement(&resource->references);
    if (references == 0) {
        (void)rindx_d3d11_destroy_object(&resource->device->core,
                                         resource->handle);
        native_private_dispose(&resource->private_data);
        ID3D11Device_Release(&resource->device->iface);
        free(resource);
    }
    return (ULONG)references;
}

static void WINAPI native_resource_get_device(ID3D11Buffer* self,
                                              ID3D11Device** out) {
    NativeD3d11Resource* resource = native_resource_from_buffer(self);
    if (!out) return;
    *out = &resource->device->iface;
    ID3D11Device_AddRef(*out);
}

static void native_context_release_state(NativeD3d11Context* context) {
    UINT index;
    if (context->graphics_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                          context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
    if (context->compute_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                          context->compute_pipeline);
        context->compute_pipeline = 0u;
    }
    if (context->bind_group != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                          context->bind_group);
        context->bind_group = 0u;
    }
    if (context->color_view) {
        ID3D11View_Release(&context->color_view->iface);
        context->color_view = NULL;
    }
    if (context->depth_view) {
        ID3D11View_Release(&context->depth_view->iface);
        context->depth_view = NULL;
    }
    if (context->vertex_shader) {
        ID3D11DeviceChild_Release(&context->vertex_shader->iface);
        context->vertex_shader = NULL;
    }
    if (context->pixel_shader) {
        ID3D11DeviceChild_Release(&context->pixel_shader->iface);
        context->pixel_shader = NULL;
    }
    if (context->compute_shader) {
        ID3D11DeviceChild_Release(&context->compute_shader->iface);
        context->compute_shader = NULL;
    }
    if (context->input_layout) {
        ID3D11InputLayout_Release(&context->input_layout->iface);
        context->input_layout = NULL;
    }
    for (index = 0u; index < 16u; ++index) {
        if (context->vertex_buffers[index]) {
            ID3D11Buffer_Release(&context->vertex_buffers[index]->iface);
            context->vertex_buffers[index] = NULL;
        }
    }
    if (context->index_buffer) {
        ID3D11Buffer_Release(&context->index_buffer->iface);
        context->index_buffer = NULL;
    }
}

static uint32_t native_topology(D3D11_PRIMITIVE_TOPOLOGY topology) {
    switch (topology) {
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST: return RIN_GPU_PRIMITIVE_POINT_LIST;
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST: return RIN_GPU_PRIMITIVE_LINE_LIST;
    case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP: return RIN_GPU_PRIMITIVE_LINE_STRIP;
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        return RIN_GPU_PRIMITIVE_TRIANGLE_LIST;
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        return RIN_GPU_PRIMITIVE_TRIANGLE_STRIP;
    default: return 0u;
    }
}

static int native_context_prepare_graphics(NativeD3d11Context* context) {
    RinGpuGraphicsPipelineNativeDescV2 desc;
    RinGpuVertexAttributeV2 attributes[16];
    RinGpuVertexBufferLayoutV1 layouts[16];
    UINT attribute_count = 0u;
    UINT layout_count = 0u;
    uint32_t color_format;
    uint32_t depth_format = 0u;
    uint32_t topology;
    int result;
    if (!context->color_view || !context->vertex_shader ||
        !context->pixel_shader) return RIN_GPU_ERROR_STATE;
    if (context->graphics_pipeline != 0u) return RIN_GPU_OK;
    color_format = native_dxgi_image_format(
        context->color_view->resource->texture2d_desc.Format);
    topology = native_topology(context->topology);
    if (color_format == 0u || topology == 0u) return RIN_GPU_ERROR_UNSUPPORTED;
    if (context->depth_view) {
        depth_format = native_dxgi_image_format(
            context->depth_view->resource->texture2d_desc.Format);
    }
    memset(&desc, 0, sizeof(desc));
    desc.base.abi_version = RIN_GPU_ABI_VERSION;
    desc.base.struct_size = sizeof(desc);
    desc.base.vertex_shader = context->vertex_shader->handle;
    desc.base.fragment_shader = context->pixel_shader->handle;
    desc.base.color_format = color_format;
    desc.base.primitive_topology = topology;
    desc.base.position_output_location = 0u;
    desc.base.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
    desc.base.blend_enabled = 1u;
    desc.base.source_color_factor = RIN_GPU_BLEND_ONE;
    desc.base.destination_color_factor = RIN_GPU_BLEND_ZERO;
    desc.base.color_operation = RIN_GPU_BLEND_ADD;
    desc.base.source_alpha_factor = RIN_GPU_BLEND_ONE;
    desc.base.destination_alpha_factor = RIN_GPU_BLEND_ZERO;
    desc.base.alpha_operation = RIN_GPU_BLEND_ADD;
    desc.base.cull_mode = RIN_GPU_CULL_NONE;
    desc.base.front_face = RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.base.depth_format = depth_format;
    desc.base.depth_compare = depth_format ? RIN_GPU_COMPARE_LESS : 0u;
    desc.base.depth_write_enabled = depth_format ? 1u : 0u;
    if (context->input_layout) {
        attribute_count = context->input_layout->attribute_count;
        layout_count = context->input_layout->layout_count;
        memcpy(attributes, context->input_layout->attributes,
               sizeof(attributes));
        memcpy(layouts, context->input_layout->layouts, sizeof(layouts));
    }
    result = rindx_d3d11_create_graphics_pipeline(
        &context->device->core, &desc, attributes, attribute_count, layouts,
        layout_count, NULL, 0u, &context->graphics_pipeline);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d11_create_graphics_bind_group(
        &context->device->core, context->graphics_pipeline, NULL, 0u,
        &context->bind_group);
    if (result != RIN_GPU_OK) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
    return result;
}

static int native_context_begin_pass(NativeD3d11Context* context) {
    RinGpuHandle color = context->color_view
                             ? context->color_view->resource->handle
                             : 0u;
    RinGpuHandle depth = context->depth_view
                             ? context->depth_view->resource->handle
                             : 0u;
    if (context->core.render_pass_active != 0u) return RIN_GPU_OK;
    return rindx_d3d11_begin_render_pass(&context->core, color, depth, 0u,
                                         0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void WINAPI native_context_om_set_render_targets(
    ID3D11DeviceContext* self, UINT count,
    ID3D11RenderTargetView* const* targets, ID3D11DepthStencilView* depth) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11View* color = NULL;
    NativeD3d11View* depth_view = depth ? (NativeD3d11View*)(void*)depth : NULL;
    if (count > 1u || (count != 0u && !targets)) return;
    if (count != 0u && targets[0]) color = (NativeD3d11View*)(void*)targets[0];
    if (context->core.render_pass_active != 0u)
        (void)rindx_d3d11_end_render_pass(&context->core);
    if (context->color_view) ID3D11View_Release(&context->color_view->iface);
    if (context->depth_view) ID3D11View_Release(&context->depth_view->iface);
    context->color_view = color;
    context->depth_view = depth_view;
    if (context->color_view) ID3D11View_AddRef(&context->color_view->iface);
    if (context->depth_view) ID3D11View_AddRef(&context->depth_view->iface);
    if (context->color_view &&
        context->color_view->resource->image_state !=
            RIN_GPU_IMAGE_STATE_COLOR_TARGET) {
        int transition_result = rindx_d3d11_transition_image(
                &context->core, context->color_view->resource->handle,
                context->color_view->resource->image_state,
                RIN_GPU_IMAGE_STATE_COLOR_TARGET);
        if (transition_result != RIN_GPU_OK) {
            return;
        }
        context->color_view->resource->image_state =
            RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    }
    if (context->depth_view &&
        context->depth_view->resource->image_state !=
            RIN_GPU_IMAGE_STATE_DEPTH_TARGET) {
        if (rindx_d3d11_transition_image(
                &context->core, context->depth_view->resource->handle,
                context->depth_view->resource->image_state,
                RIN_GPU_IMAGE_STATE_DEPTH_TARGET) != RIN_GPU_OK)
            return;
        context->depth_view->resource->image_state =
            RIN_GPU_IMAGE_STATE_DEPTH_TARGET;
    }
    if (context->graphics_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
}

static void WINAPI native_context_ia_set_input_layout(
    ID3D11DeviceContext* self, ID3D11InputLayout* layout) {
    NativeD3d11Context* context = native_context_from_interface(self);
    if (context->input_layout) ID3D11InputLayout_Release(&context->input_layout->iface);
    context->input_layout = layout ? native_layout_from_interface(layout) : NULL;
    if (context->input_layout) ID3D11InputLayout_AddRef(&context->input_layout->iface);
    if (context->graphics_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
}

static void WINAPI native_context_ia_set_vertex_buffers(
    ID3D11DeviceContext* self, UINT start, UINT count,
    ID3D11Buffer* const* buffers, const UINT* strides, const UINT* offsets) {
    NativeD3d11Context* context = native_context_from_interface(self);
    UINT index;
    if (start >= 16u || count > 16u - start || (count != 0u &&
        (!buffers || !strides || !offsets))) return;
    for (index = 0u; index < count; ++index) {
        UINT slot = start + index;
        if (context->vertex_buffers[slot])
            ID3D11Buffer_Release(&context->vertex_buffers[slot]->iface);
        context->vertex_buffers[slot] = buffers[index]
            ? native_resource_from_buffer(buffers[index]) : NULL;
        if (context->vertex_buffers[slot])
            ID3D11Buffer_AddRef(&context->vertex_buffers[slot]->iface);
        context->vertex_strides[slot] = strides[index];
        context->vertex_offsets[slot] = offsets[index];
    }
}

static void WINAPI native_context_ia_set_index_buffer(
    ID3D11DeviceContext* self, ID3D11Buffer* buffer, DXGI_FORMAT format,
    UINT offset) {
    NativeD3d11Context* context = native_context_from_interface(self);
    if (context->index_buffer) ID3D11Buffer_Release(&context->index_buffer->iface);
    context->index_buffer = buffer ? native_resource_from_buffer(buffer) : NULL;
    if (context->index_buffer) ID3D11Buffer_AddRef(&context->index_buffer->iface);
    context->index_format = format;
    context->index_offset = offset;
}

static void native_context_set_shader(NativeD3d11Shader** destination,
                                      NativeD3d11Shader* shader,
                                      ID3D11DeviceChild* iface) {
    if (*destination) ID3D11DeviceChild_Release(&(*destination)->iface);
    *destination = shader;
    if (*destination) ID3D11DeviceChild_AddRef(iface);
}

static void WINAPI native_context_vs_set_shader(
    ID3D11DeviceContext* self, ID3D11VertexShader* shader,
    ID3D11ClassInstance* const* instances, UINT count) {
    NativeD3d11Context* context = native_context_from_interface(self);
    (void)instances; (void)count;
    native_context_set_shader(&context->vertex_shader,
                              shader ? (NativeD3d11Shader*)(void*)shader : NULL,
                              shader ? (ID3D11DeviceChild*)(void*)shader : NULL);
    if (context->graphics_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
}
static void WINAPI native_context_ps_set_shader(
    ID3D11DeviceContext* self, ID3D11PixelShader* shader,
    ID3D11ClassInstance* const* instances, UINT count) {
    NativeD3d11Context* context = native_context_from_interface(self);
    (void)instances; (void)count;
    native_context_set_shader(&context->pixel_shader,
                              shader ? (NativeD3d11Shader*)(void*)shader : NULL,
                              shader ? (ID3D11DeviceChild*)(void*)shader : NULL);
    if (context->graphics_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->graphics_pipeline);
        context->graphics_pipeline = 0u;
    }
}
static void WINAPI native_context_cs_set_shader(
    ID3D11DeviceContext* self, ID3D11ComputeShader* shader,
    ID3D11ClassInstance* const* instances, UINT count) {
    NativeD3d11Context* context = native_context_from_interface(self);
    (void)instances; (void)count;
    native_context_set_shader(&context->compute_shader,
                              shader ? (NativeD3d11Shader*)(void*)shader : NULL,
                              shader ? (ID3D11DeviceChild*)(void*)shader : NULL);
    if (context->compute_pipeline != 0u) {
        (void)rindx_d3d11_destroy_object(&context->device->core,
                                         context->compute_pipeline);
        context->compute_pipeline = 0u;
    }
}
static void WINAPI native_context_ia_set_primitive_topology(
    ID3D11DeviceContext* self, D3D11_PRIMITIVE_TOPOLOGY topology) {
    native_context_from_interface(self)->topology = topology;
}

static HRESULT WINAPI native_buffer_private_data(
    ID3D11Buffer* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(native_resource_from_buffer(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_buffer_set_private_data(
    ID3D11Buffer* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(&native_resource_from_buffer(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_buffer_set_private_interface(
    ID3D11Buffer* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_resource_from_buffer(self)->private_data, guid, object);
}
static HRESULT WINAPI native_texture_private_data(
    ID3D11Texture2D* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(
        native_resource_from_buffer((ID3D11Buffer*)(void*)self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_texture_set_private_data(
    ID3D11Texture2D* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(
        &native_resource_from_buffer((ID3D11Buffer*)(void*)self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_texture_set_private_interface(
    ID3D11Texture2D* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_resource_from_buffer((ID3D11Buffer*)(void*)self)->private_data,
        guid, object);
}
static void WINAPI native_resource_get_type(ID3D11Buffer* self,
                                            D3D11_RESOURCE_DIMENSION* out) {
    NativeD3d11Resource* resource = native_resource_from_buffer(self);
    if (out) {
        if (resource->kind == NATIVE_D3D11_BUFFER)
            *out = D3D11_RESOURCE_DIMENSION_BUFFER;
        else if (resource->kind == NATIVE_D3D11_TEXTURE1D)
            *out = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
        else if (resource->kind == NATIVE_D3D11_TEXTURE3D)
            *out = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
        else
            *out = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    }
}
static HRESULT WINAPI native_resource_set_eviction(ID3D11Buffer* self, UINT value) {
    native_resource_from_buffer(self)->eviction_priority = value;
    return S_OK;
}
static UINT WINAPI native_resource_get_eviction(ID3D11Buffer* self) {
    return native_resource_from_buffer(self)->eviction_priority;
}
static void WINAPI native_buffer_get_desc(ID3D11Buffer* self,
                                          D3D11_BUFFER_DESC* out) {
    if (out) *out = native_resource_from_buffer(self)->buffer_desc;
}
static void WINAPI native_texture2d_get_desc(ID3D11Texture2D* self,
                                             D3D11_TEXTURE2D_DESC* out) {
    if (out) *out = native_resource_from_buffer((ID3D11Buffer*)self)
                        ->texture2d_desc;
}
static HRESULT WINAPI native_texture_query_interface(
    ID3D11Texture2D* self, REFIID iid, void** out) {
    return native_resource_query_interface((ID3D11Buffer*)(void*)self, iid, out);
}
static ULONG WINAPI native_texture_add_ref(ID3D11Texture2D* self) {
    return native_resource_add_ref((ID3D11Buffer*)(void*)self);
}
static ULONG WINAPI native_texture_release(ID3D11Texture2D* self) {
    return native_resource_release((ID3D11Buffer*)(void*)self);
}
static void WINAPI native_texture_get_device(ID3D11Texture2D* self,
                                              ID3D11Device** out) {
    native_resource_get_device((ID3D11Buffer*)(void*)self, out);
}
static void WINAPI native_texture_get_type(ID3D11Texture2D* self,
                                            D3D11_RESOURCE_DIMENSION* out) {
    native_resource_get_type((ID3D11Buffer*)(void*)self, out);
}
static HRESULT WINAPI native_texture_set_eviction(ID3D11Texture2D* self,
                                                  UINT value) {
    return native_resource_set_eviction((ID3D11Buffer*)(void*)self, value);
}
static UINT WINAPI native_texture_get_eviction(ID3D11Texture2D* self) {
    return native_resource_get_eviction((ID3D11Buffer*)(void*)self);
}

static void WINAPI native_texture1d_get_desc(ID3D11Texture1D* self,
                                             D3D11_TEXTURE1D_DESC* out) {
    if (out) *out = native_resource_from_buffer((ID3D11Buffer*)(void*)self)
                         ->texture1d_desc;
}

static void WINAPI native_texture3d_get_desc(ID3D11Texture3D* self,
                                             D3D11_TEXTURE3D_DESC* out) {
    if (out) *out = native_resource_from_buffer((ID3D11Buffer*)(void*)self)
                         ->texture3d_desc;
}

static NativeD3d11View* native_view_from_interface(ID3D11View* self) {
    return (NativeD3d11View*)(void*)self;
}

static NativeD3d11Shader* native_shader_from_child(ID3D11DeviceChild* self) {
    return (NativeD3d11Shader*)(void*)self;
}

static NativeD3d11InputLayout* native_layout_from_interface(
    ID3D11InputLayout* self) {
    return (NativeD3d11InputLayout*)(void*)self;
}

static uint32_t native_dxgi_image_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8_UNORM: return RIN_GPU_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return RIN_GPU_FORMAT_RGBA8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return RIN_GPU_FORMAT_BGRA8_UNORM;
    case DXGI_FORMAT_D32_FLOAT: return RIN_GPU_FORMAT_D32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    default: return 0u;
    }
}

static uint32_t native_dxgi_vertex_format(DXGI_FORMAT format) {
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

static HRESULT native_create_view(NativeD3d11Device* device,
                                  ID3D11Resource* resource,
                                  NativeD3d11ViewKind kind,
                                  ID3D11View** out) {
    NativeD3d11Resource* native_resource;
    NativeD3d11View* view;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!device || !resource) return E_INVALIDARG;
    native_resource = native_resource_from_resource(resource);
    if (!native_resource || native_resource->device != device) return E_INVALIDARG;
    if (native_resource->kind == NATIVE_D3D11_BUFFER &&
        kind != NATIVE_D3D11_VIEW_UAV) return E_INVALIDARG;
    view = (NativeD3d11View*)calloc(1u, sizeof(*view));
    if (!view) return E_OUTOFMEMORY;
    view->iface.lpVtbl = (ID3D11ViewVtbl*)(void*)&native_view_vtable;
    view->references = 1;
    view->device = device;
    view->resource = native_resource;
    view->kind = kind;
    ID3D11Device_AddRef(&device->iface);
    ID3D11Resource_AddRef(resource);
    *out = &view->iface;
    return S_OK;
}

static HRESULT native_create_shader(NativeD3d11Device* device, UINT stage,
                                    const void* bytecode, SIZE_T bytecode_size,
                                    ID3D11DeviceChild** out) {
    NativeD3d11Shader* shader;
    RinGpuHandle handle = 0u;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!device || !bytecode || bytecode_size == 0u) return E_INVALIDARG;
    result = rindx_d3d11_create_shader(&device->core, bytecode,
                                       (uint64_t)bytecode_size, &handle);
    if (result != RIN_GPU_OK) return core_result(result);
    shader = (NativeD3d11Shader*)calloc(1u, sizeof(*shader));
    if (!shader) {
        (void)rindx_d3d11_destroy_object(&device->core, handle);
        return E_OUTOFMEMORY;
    }
    shader->iface.lpVtbl = (ID3D11DeviceChildVtbl*)(void*)&native_shader_vtable;
    shader->references = 1;
    shader->device = device;
    shader->handle = handle;
    shader->stage = stage;
    ID3D11Device_AddRef(&device->iface);
    *out = &shader->iface;
    return S_OK;
}

static HRESULT native_create_texture_desc(
    NativeD3d11Device* device, const RinGpuImageDescV1* core_desc,
    const void* initial_data, uint64_t source_size,
    NativeD3d11Resource* resource, uint32_t kind) {
    RinGpuImageUploadV1 upload;
    int result;
    if (!device || !core_desc || !resource) return E_INVALIDARG;
    if (kind == NATIVE_D3D11_TEXTURE1D)
        result = rindx_d3d11_create_texture1d(&device->core, core_desc,
                                              &resource->handle);
    else if (kind == NATIVE_D3D11_TEXTURE2D)
        result = rindx_d3d11_create_texture2d(&device->core, core_desc,
                                              &resource->handle);
    else
        result = rindx_d3d11_create_texture3d(&device->core, core_desc,
                                              &resource->handle);
    if (result != RIN_GPU_OK) return core_result(result);
    if (!initial_data) return S_OK;
    memset(&upload, 0, sizeof(upload));
    upload.abi_version = RIN_GPU_ABI_VERSION;
    upload.struct_size = sizeof(upload);
    upload.width = core_desc->width;
    upload.height = core_desc->height;
    upload.depth = core_desc->depth;
    upload.source_row_pitch_bytes = core_desc->width * 4u;
    upload.source_slice_pitch_bytes = upload.source_row_pitch_bytes *
                                      core_desc->height;
    if (kind == NATIVE_D3D11_TEXTURE3D)
        upload.source_slice_pitch_bytes *= core_desc->depth;
    result = rindx_d3d11_upload_image(&device->core, resource->handle,
                                      &upload, initial_data, source_size);
    if (result != RIN_GPU_OK) return core_result(result);
    return S_OK;
}

static HRESULT WINAPI native_view_query_interface(
    ID3D11View* self, REFIID iid, void** out) {
    NativeD3d11View* view = native_view_from_interface(self);
    int matched = IsEqualIID(iid, &IID_IUnknown) ||
                  IsEqualIID(iid, &IID_ID3D11DeviceChild) ||
                  IsEqualIID(iid, &IID_ID3D11View);
    if (view->kind == NATIVE_D3D11_VIEW_SRV)
        matched = matched || IsEqualIID(iid, &IID_ID3D11ShaderResourceView);
    if (view->kind == NATIVE_D3D11_VIEW_UAV)
        matched = matched || IsEqualIID(iid, &IID_ID3D11UnorderedAccessView);
    if (view->kind == NATIVE_D3D11_VIEW_RTV)
        matched = matched || IsEqualIID(iid, &IID_ID3D11RenderTargetView);
    if (view->kind == NATIVE_D3D11_VIEW_DSV)
        matched = matched || IsEqualIID(iid, &IID_ID3D11DepthStencilView);
    if (!out) return E_POINTER;
    *out = matched ? self : NULL;
    if (matched) ID3D11View_AddRef(self);
    return matched ? S_OK : E_NOINTERFACE;
}

static ULONG WINAPI native_view_add_ref(ID3D11View* self) {
    return (ULONG)InterlockedIncrement(&native_view_from_interface(self)->references);
}

static ULONG WINAPI native_view_release(ID3D11View* self) {
    NativeD3d11View* view = native_view_from_interface(self);
    LONG references = InterlockedDecrement(&view->references);
    if (references == 0) {
        native_private_dispose(&view->private_data);
        ID3D11Resource_Release((ID3D11Resource*)(void*)view->resource);
        ID3D11Device_Release(&view->device->iface);
        free(view);
    }
    return (ULONG)references;
}

static void WINAPI native_view_get_device(ID3D11View* self,
                                          ID3D11Device** out) {
    NativeD3d11View* view = native_view_from_interface(self);
    if (!out) return;
    *out = &view->device->iface;
    ID3D11Device_AddRef(*out);
}

static HRESULT WINAPI native_view_get_private_data(
    ID3D11View* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(native_view_from_interface(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_view_set_private_data(
    ID3D11View* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(&native_view_from_interface(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_view_set_private_interface(
    ID3D11View* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_view_from_interface(self)->private_data, guid, object);
}
static void WINAPI native_view_get_resource(ID3D11View* self,
                                            ID3D11Resource** out) {
    NativeD3d11View* view = native_view_from_interface(self);
    if (!out) return;
    *out = (ID3D11Resource*)(void*)view->resource;
    ID3D11Resource_AddRef(*out);
}

static HRESULT WINAPI native_shader_query_interface(
    ID3D11DeviceChild* self, REFIID iid, void** out) {
    NativeD3d11Shader* shader = native_shader_from_child(self);
    int matched = IsEqualIID(iid, &IID_IUnknown) ||
                  IsEqualIID(iid, &IID_ID3D11DeviceChild);
    if (shader->stage == RIN_SHADER_STAGE_VERTEX)
        matched = matched || IsEqualIID(iid, &IID_ID3D11VertexShader);
    else if (shader->stage == RIN_SHADER_STAGE_FRAGMENT)
        matched = matched || IsEqualIID(iid, &IID_ID3D11PixelShader);
    else if (shader->stage == RIN_SHADER_STAGE_COMPUTE)
        matched = matched || IsEqualIID(iid, &IID_ID3D11ComputeShader);
    if (!out) return E_POINTER;
    *out = matched ? self : NULL;
    if (matched) ID3D11DeviceChild_AddRef(self);
    return matched ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI native_shader_add_ref(ID3D11DeviceChild* self) {
    return (ULONG)InterlockedIncrement(
        &native_shader_from_child(self)->references);
}
static ULONG WINAPI native_shader_release(ID3D11DeviceChild* self) {
    NativeD3d11Shader* shader = native_shader_from_child(self);
    LONG references = InterlockedDecrement(&shader->references);
    if (references == 0) {
        (void)rindx_d3d11_destroy_object(&shader->device->core, shader->handle);
        native_private_dispose(&shader->private_data);
        ID3D11Device_Release(&shader->device->iface);
        free(shader);
    }
    return (ULONG)references;
}
static void WINAPI native_shader_get_device(ID3D11DeviceChild* self,
                                            ID3D11Device** out) {
    NativeD3d11Shader* shader = native_shader_from_child(self);
    if (!out) return;
    *out = &shader->device->iface;
    ID3D11Device_AddRef(*out);
}
static HRESULT WINAPI native_shader_get_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(native_shader_from_child(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_shader_set_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(&native_shader_from_child(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_shader_set_private_interface(
    ID3D11DeviceChild* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_shader_from_child(self)->private_data, guid, object);
}

static NativeD3d11Sampler* native_sampler_from_child(ID3D11DeviceChild* self) {
    return (NativeD3d11Sampler*)(void*)self;
}
static HRESULT WINAPI native_sampler_query_interface(
    ID3D11DeviceChild* self, REFIID iid, void** out) {
    int matched = IsEqualIID(iid, &IID_IUnknown) ||
                  IsEqualIID(iid, &IID_ID3D11DeviceChild) ||
                  IsEqualIID(iid, &IID_ID3D11SamplerState);
    if (!out) return E_POINTER;
    *out = matched ? self : NULL;
    if (matched) ID3D11DeviceChild_AddRef(self);
    return matched ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI native_sampler_add_ref(ID3D11DeviceChild* self) {
    return (ULONG)InterlockedIncrement(&native_sampler_from_child(self)->references);
}
static ULONG WINAPI native_sampler_release(ID3D11DeviceChild* self) {
    NativeD3d11Sampler* sampler = native_sampler_from_child(self);
    LONG references = InterlockedDecrement(&sampler->references);
    if (references == 0) {
        (void)rindx_d3d11_destroy_object(&sampler->device->core,
                                          sampler->handle);
        native_private_dispose(&sampler->private_data);
        ID3D11Device_Release(&sampler->device->iface);
        free(sampler);
    }
    return (ULONG)references;
}
static void WINAPI native_sampler_get_device(ID3D11DeviceChild* self,
                                             ID3D11Device** out) {
    NativeD3d11Sampler* sampler = native_sampler_from_child(self);
    if (!out) return;
    *out = &sampler->device->iface;
    ID3D11Device_AddRef(*out);
}
static HRESULT WINAPI native_sampler_get_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(native_sampler_from_child(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_sampler_set_private_data(
    ID3D11DeviceChild* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(&native_sampler_from_child(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_sampler_set_private_interface(
    ID3D11DeviceChild* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_sampler_from_child(self)->private_data, guid, object);
}

static NativeD3d11CommandList* native_command_list_from_interface(
    ID3D11CommandList* self) {
    return (NativeD3d11CommandList*)(void*)self;
}
static HRESULT WINAPI native_command_list_query_interface(
    ID3D11CommandList* self, REFIID iid, void** out) {
    int matched = IsEqualIID(iid, &IID_IUnknown) ||
                  IsEqualIID(iid, &IID_ID3D11DeviceChild) ||
                  IsEqualIID(iid, &IID_ID3D11CommandList);
    if (!out) return E_POINTER;
    *out = matched ? self : NULL;
    if (matched) ID3D11CommandList_AddRef(self);
    return matched ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI native_command_list_add_ref(ID3D11CommandList* self) {
    return (ULONG)InterlockedIncrement(
        &native_command_list_from_interface(self)->references);
}
static ULONG WINAPI native_command_list_release(ID3D11CommandList* self) {
    NativeD3d11CommandList* list = native_command_list_from_interface(self);
    LONG references = InterlockedDecrement(&list->references);
    if (references == 0) {
        (void)rindx_d3d11_destroy_object(&list->device->core, list->handle);
        native_private_dispose(&list->private_data);
        ID3D11Device_Release(&list->device->iface);
        free(list);
    }
    return (ULONG)references;
}
static void WINAPI native_command_list_get_device(ID3D11CommandList* self,
                                                   ID3D11Device** out) {
    NativeD3d11CommandList* list = native_command_list_from_interface(self);
    if (!out) return;
    *out = &list->device->iface;
    ID3D11Device_AddRef(*out);
}
static HRESULT WINAPI native_command_list_get_private_data(
    ID3D11CommandList* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(
        native_command_list_from_interface(self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_command_list_set_private_data(
    ID3D11CommandList* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(
        &native_command_list_from_interface(self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_command_list_set_private_interface(
    ID3D11CommandList* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_command_list_from_interface(self)->private_data,
        guid, object);
}
static UINT WINAPI native_command_list_get_context_flags(ID3D11CommandList* self) {
    return native_command_list_from_interface(self)->context_flags;
}

static HRESULT WINAPI native_layout_query_interface(
    ID3D11InputLayout* self, REFIID iid, void** out) {
    int matched = IsEqualIID(iid, &IID_IUnknown) ||
                  IsEqualIID(iid, &IID_ID3D11DeviceChild) ||
                  IsEqualIID(iid, &IID_ID3D11InputLayout);
    if (!out) return E_POINTER;
    *out = matched ? self : NULL;
    if (matched) ID3D11InputLayout_AddRef(self);
    return matched ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI native_layout_add_ref(ID3D11InputLayout* self) {
    return (ULONG)InterlockedIncrement(
        &native_layout_from_interface(self)->references);
}
static ULONG WINAPI native_layout_release(ID3D11InputLayout* self) {
    NativeD3d11InputLayout* layout = native_layout_from_interface(self);
    LONG references = InterlockedDecrement(&layout->references);
    if (references == 0) {
        native_private_dispose(&layout->private_data);
        ID3D11Device_Release(&layout->device->iface);
        free(layout);
    }
    return (ULONG)references;
}
static void WINAPI native_layout_get_device(ID3D11InputLayout* self,
                                            ID3D11Device** out) {
    NativeD3d11InputLayout* layout = native_layout_from_interface(self);
    if (!out) return;
    *out = &layout->device->iface;
    ID3D11Device_AddRef(*out);
}
static HRESULT WINAPI native_layout_get_private_data(
    ID3D11InputLayout* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(native_layout_from_interface(self)->private_data,
                              guid, size, data);
}
static HRESULT WINAPI native_layout_set_private_data(
    ID3D11InputLayout* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(
        &native_layout_from_interface(self)->private_data, guid, size, data);
}
static HRESULT WINAPI native_layout_set_private_interface(
    ID3D11InputLayout* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_layout_from_interface(self)->private_data, guid, object);
}

static const ID3D11BufferVtbl native_buffer_vtable = {
    native_resource_query_interface,
    native_resource_add_ref,
    native_resource_release,
    native_resource_get_device,
    native_buffer_private_data,
    native_buffer_set_private_data,
    native_buffer_set_private_interface,
    native_resource_get_type,
    native_resource_set_eviction,
    native_resource_get_eviction,
    native_buffer_get_desc,
};

static const ID3D11Texture2DVtbl native_texture2d_vtable = {
    .QueryInterface = native_texture_query_interface,
    .AddRef = native_texture_add_ref,
    .Release = native_texture_release,
    .GetDevice = native_texture_get_device,
    .GetPrivateData = native_texture_private_data,
    .SetPrivateData = native_texture_set_private_data,
    .SetPrivateDataInterface = native_texture_set_private_interface,
    .GetType = native_texture_get_type,
    .SetEvictionPriority = native_texture_set_eviction,
    .GetEvictionPriority = native_texture_get_eviction,
    native_texture2d_get_desc,
};

static const ID3D11Texture1DVtbl native_texture1d_vtable = {
    .QueryInterface = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture1D*, REFIID,
                                                      void**))
        native_texture_query_interface,
    .AddRef = (ULONG (STDMETHODCALLTYPE*)(ID3D11Texture1D*))
        native_texture_add_ref,
    .Release = (ULONG (STDMETHODCALLTYPE*)(ID3D11Texture1D*))
        native_texture_release,
    .GetDevice = (void (STDMETHODCALLTYPE*)(ID3D11Texture1D*, ID3D11Device**))
        native_texture_get_device,
    .GetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture1D*, REFGUID,
                                                     UINT*, void*))
        native_texture_private_data,
    .SetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture1D*, REFGUID,
                                                     UINT, const void*))
        native_texture_set_private_data,
    .SetPrivateDataInterface =
        (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture1D*, REFGUID,
                                      const IUnknown*))native_texture_set_private_interface,
    .GetType = (void (STDMETHODCALLTYPE*)(ID3D11Texture1D*,
                                          D3D11_RESOURCE_DIMENSION*))
        native_texture_get_type,
    .SetEvictionPriority = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture1D*, UINT))
        native_texture_set_eviction,
    .GetEvictionPriority = (UINT (STDMETHODCALLTYPE*)(ID3D11Texture1D*))
        native_texture_get_eviction,
    .GetDesc = native_texture1d_get_desc,
};

static const ID3D11Texture3DVtbl native_texture3d_vtable = {
    .QueryInterface = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture3D*, REFIID,
                                                      void**))
        native_texture_query_interface,
    .AddRef = (ULONG (STDMETHODCALLTYPE*)(ID3D11Texture3D*))
        native_texture_add_ref,
    .Release = (ULONG (STDMETHODCALLTYPE*)(ID3D11Texture3D*))
        native_texture_release,
    .GetDevice = (void (STDMETHODCALLTYPE*)(ID3D11Texture3D*, ID3D11Device**))
        native_texture_get_device,
    .GetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture3D*, REFGUID,
                                                     UINT*, void*))
        native_texture_private_data,
    .SetPrivateData = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture3D*, REFGUID,
                                                     UINT, const void*))
        native_texture_set_private_data,
    .SetPrivateDataInterface =
        (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture3D*, REFGUID,
                                      const IUnknown*))native_texture_set_private_interface,
    .GetType = (void (STDMETHODCALLTYPE*)(ID3D11Texture3D*,
                                          D3D11_RESOURCE_DIMENSION*))
        native_texture_get_type,
    .SetEvictionPriority = (HRESULT (STDMETHODCALLTYPE*)(ID3D11Texture3D*, UINT))
        native_texture_set_eviction,
    .GetEvictionPriority = (UINT (STDMETHODCALLTYPE*)(ID3D11Texture3D*))
        native_texture_get_eviction,
    .GetDesc = native_texture3d_get_desc,
};

static HRESULT WINAPI native_device_query_interface(
    ID3D11Device* self, REFIID iid, void** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ID3D11Device)) {
        *out = self;
        ID3D11Device_AddRef(self);
        return S_OK;
    }
    if (IsEqualIID(iid, &IID_ID3D11DeviceContext)) {
        if (!device->immediate) return E_FAIL;
        *out = &device->immediate->iface;
        ID3D11DeviceContext_AddRef(&device->immediate->iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI native_device_add_ref(ID3D11Device* self) {
    return (ULONG)InterlockedIncrement(
        &native_device_from_interface(self)->references);
}

static ULONG WINAPI native_device_release(ID3D11Device* self) {
    NativeD3d11Device* device = native_device_from_interface(self);
    LONG references = InterlockedDecrement(&device->references);
    if (references == 0) {
        if (device->immediate) {
            ID3D11DeviceContext_Release(&device->immediate->iface);
            device->immediate = NULL;
        }
        (void)rindx_d3d11_destroy_device(&device->core);
        free(device);
    }
    return (ULONG)references;
}

static HRESULT WINAPI native_device_create_buffer(
    ID3D11Device* self, const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Buffer** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    RinGpuBufferDescV1 core_desc;
    NativeD3d11Resource* resource;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->ByteWidth == 0u || desc->StructureByteStride > desc->ByteWidth)
        return E_INVALIDARG;
    memset(&core_desc, 0, sizeof(core_desc));
    core_desc.abi_version = RIN_GPU_ABI_VERSION;
    core_desc.struct_size = sizeof(core_desc);
    core_desc.size_bytes = desc->ByteWidth;
    core_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE | RIN_GPU_BUFFER_COPY_DESTINATION;
    if ((desc->BindFlags & (D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER)) != 0u)
        core_desc.usage |= RIN_GPU_BUFFER_VERTEX;
    if ((desc->BindFlags & D3D11_BIND_INDEX_BUFFER) != 0u)
        core_desc.usage |= RIN_GPU_BUFFER_INDEX;
    if ((desc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0u)
        core_desc.usage |= RIN_GPU_BUFFER_STORAGE;
    if ((desc->MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) != 0u)
        core_desc.usage |= RIN_GPU_BUFFER_INDIRECT;
    if (desc->Usage == D3D11_USAGE_DYNAMIC || desc->Usage == D3D11_USAGE_STAGING)
        core_desc.flags |= RIN_GPU_BUFFER_CPU_VISIBLE;
    resource = (NativeD3d11Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    result = rindx_d3d11_create_buffer(&device->core, &core_desc,
                                       &resource->handle);
    if (result != RIN_GPU_OK) { free(resource); return core_result(result); }
    resource->iface.lpVtbl = (ID3D11BufferVtbl*)(void*)&native_buffer_vtable;
    resource->references = 1;
    resource->device = device;
    resource->kind = NATIVE_D3D11_BUFFER;
    resource->buffer_desc = *desc;
    ID3D11Device_AddRef(self);
    if (initial_data) {
        result = rindx_d3d11_upload_buffer(&device->core, resource->handle, 0u,
                                           initial_data->pSysMem, desc->ByteWidth);
        if (result != RIN_GPU_OK) {
            ID3D11Buffer_Release(&resource->iface);
            return core_result(result);
        }
    }
    *out = &resource->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_texture2d(
    ID3D11Device* self, const D3D11_TEXTURE2D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture2D** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    RinGpuImageDescV1 core_desc;
    NativeD3d11Resource* resource;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->Width == 0u || desc->Height == 0u || desc->ArraySize != 1u ||
        desc->MipLevels > 1u || desc->SampleDesc.Count != 1u)
        return E_INVALIDARG;
    memset(&core_desc, 0, sizeof(core_desc));
    core_desc.abi_version = RIN_GPU_ABI_VERSION;
    core_desc.struct_size = sizeof(core_desc);
    core_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
    core_desc.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    core_desc.width = desc->Width;
    core_desc.height = desc->Height;
    core_desc.depth = 1u;
    core_desc.array_layers = 1u;
    core_desc.mip_levels = 1u;
    core_desc.sample_count = 1u;
    core_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE | RIN_GPU_IMAGE_COPY_DESTINATION;
    if ((desc->BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0u)
        core_desc.usage |= RIN_GPU_IMAGE_SAMPLED;
    if ((desc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0u)
        core_desc.usage |= RIN_GPU_IMAGE_STORAGE;
    if ((desc->BindFlags & D3D11_BIND_RENDER_TARGET) != 0u)
        core_desc.usage |= RIN_GPU_IMAGE_COLOR_TARGET;
    if ((desc->BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0u)
        core_desc.usage |= RIN_GPU_IMAGE_DEPTH_STENCIL;
    if (desc->Usage == D3D11_USAGE_STAGING)
        core_desc.flags |= RIN_GPU_IMAGE_CPU_VISIBLE | RIN_GPU_IMAGE_CPU_READABLE;
    resource = (NativeD3d11Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    result = rindx_d3d11_create_texture2d(&device->core, &core_desc,
                                          &resource->handle);
    if (result != RIN_GPU_OK) { free(resource); return core_result(result); }
    resource->iface.lpVtbl = (ID3D11BufferVtbl*)(void*)&native_texture2d_vtable;
    resource->references = 1;
    resource->device = device;
    resource->kind = NATIVE_D3D11_TEXTURE2D;
    resource->texture2d_desc = *desc;
    ID3D11Device_AddRef(self);
    if (initial_data && initial_data->pSysMem) {
        RinGpuImageUploadV1 upload;
        memset(&upload, 0, sizeof(upload));
        upload.abi_version = RIN_GPU_ABI_VERSION;
        upload.struct_size = sizeof(upload);
        upload.width = desc->Width;
        upload.height = desc->Height;
        upload.depth = 1u;
        upload.source_row_pitch_bytes = initial_data->SysMemPitch;
        upload.source_slice_pitch_bytes = initial_data->SysMemSlicePitch;
        if (upload.source_slice_pitch_bytes == 0u)
            upload.source_slice_pitch_bytes = upload.source_row_pitch_bytes * desc->Height;
        result = rindx_d3d11_upload_image(&device->core, resource->handle,
                                          &upload, initial_data->pSysMem,
                                          upload.source_slice_pitch_bytes);
        if (result != RIN_GPU_OK) {
            ID3D11Buffer_Release(&resource->iface);
            return core_result(result);
        }
    }
    *out = (ID3D11Texture2D*)(void*)resource;
    return S_OK;
}

static void native_fill_image_desc(RinGpuImageDescV1* core_desc,
                                    uint32_t dimension, uint32_t format,
                                    uint32_t width, uint32_t height,
                                    uint32_t depth, UINT bind_flags,
                                    D3D11_USAGE usage) {
    memset(core_desc, 0, sizeof(*core_desc));
    core_desc->abi_version = RIN_GPU_ABI_VERSION;
    core_desc->struct_size = sizeof(*core_desc);
    core_desc->dimension = dimension;
    core_desc->format = format;
    core_desc->width = width;
    core_desc->height = height;
    core_desc->depth = depth;
    core_desc->array_layers = 1u;
    core_desc->mip_levels = 1u;
    core_desc->sample_count = 1u;
    core_desc->usage = RIN_GPU_IMAGE_COPY_SOURCE |
                       RIN_GPU_IMAGE_COPY_DESTINATION;
    if ((bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0u)
        core_desc->usage |= RIN_GPU_IMAGE_SAMPLED;
    if ((bind_flags & D3D11_BIND_UNORDERED_ACCESS) != 0u)
        core_desc->usage |= RIN_GPU_IMAGE_STORAGE;
    if ((bind_flags & D3D11_BIND_RENDER_TARGET) != 0u)
        core_desc->usage |= RIN_GPU_IMAGE_COLOR_TARGET;
    if ((bind_flags & D3D11_BIND_DEPTH_STENCIL) != 0u)
        core_desc->usage |= RIN_GPU_IMAGE_DEPTH_STENCIL;
    if (usage == D3D11_USAGE_STAGING)
        core_desc->flags |= RIN_GPU_IMAGE_CPU_VISIBLE | RIN_GPU_IMAGE_CPU_READABLE;
}

static HRESULT WINAPI native_device_create_texture1d(
    ID3D11Device* self, const D3D11_TEXTURE1D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture1D** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    NativeD3d11Resource* resource;
    RinGpuImageDescV1 core_desc;
    uint32_t format;
    uint64_t source_size;
    HRESULT result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->Width == 0u || desc->ArraySize != 1u ||
        desc->MipLevels > 1u || (format = native_dxgi_image_format(desc->Format)) == 0u)
        return E_INVALIDARG;
    native_fill_image_desc(&core_desc, RIN_GPU_IMAGE_DIMENSION_1D, format,
                           desc->Width, 1u, 1u, desc->BindFlags, desc->Usage);
    resource = (NativeD3d11Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    source_size = initial_data ? (initial_data->SysMemSlicePitch != 0u
                                      ? initial_data->SysMemSlicePitch
                                      : initial_data->SysMemPitch)
                               : 0u;
    result = native_create_texture_desc(device, &core_desc,
                                        initial_data ? initial_data->pSysMem : NULL,
                                        source_size, resource,
                                        NATIVE_D3D11_TEXTURE1D);
    if (FAILED(result)) { free(resource); return result; }
    resource->iface.lpVtbl = (ID3D11BufferVtbl*)(void*)&native_texture1d_vtable;
    resource->references = 1;
    resource->device = device;
    resource->kind = NATIVE_D3D11_TEXTURE1D;
    resource->texture1d_desc = *desc;
    ID3D11Device_AddRef(self);
    *out = (ID3D11Texture1D*)(void*)resource;
    return S_OK;
}

static HRESULT WINAPI native_device_create_texture3d(
    ID3D11Device* self, const D3D11_TEXTURE3D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data, ID3D11Texture3D** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    NativeD3d11Resource* resource;
    RinGpuImageDescV1 core_desc;
    uint32_t format;
    uint64_t source_size;
    HRESULT result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->Width == 0u || desc->Height == 0u || desc->Depth == 0u ||
        desc->MipLevels > 1u || (format = native_dxgi_image_format(desc->Format)) == 0u)
        return E_INVALIDARG;
    native_fill_image_desc(&core_desc, RIN_GPU_IMAGE_DIMENSION_3D, format,
                           desc->Width, desc->Height, desc->Depth,
                           desc->BindFlags, desc->Usage);
    resource = (NativeD3d11Resource*)calloc(1u, sizeof(*resource));
    if (!resource) return E_OUTOFMEMORY;
    source_size = initial_data ? (initial_data->SysMemSlicePitch != 0u
                                      ? initial_data->SysMemSlicePitch * desc->Depth
                                      : initial_data->SysMemPitch * desc->Height * desc->Depth)
                               : 0u;
    result = native_create_texture_desc(device, &core_desc,
                                        initial_data ? initial_data->pSysMem : NULL,
                                        source_size, resource,
                                        NATIVE_D3D11_TEXTURE3D);
    if (FAILED(result)) { free(resource); return result; }
    resource->iface.lpVtbl = (ID3D11BufferVtbl*)(void*)&native_texture3d_vtable;
    resource->references = 1;
    resource->device = device;
    resource->kind = NATIVE_D3D11_TEXTURE3D;
    resource->texture3d_desc = *desc;
    ID3D11Device_AddRef(self);
    *out = (ID3D11Texture3D*)(void*)resource;
    return S_OK;
}

static HRESULT WINAPI native_device_create_shader_resource_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
    ID3D11ShaderResourceView** out) {
    NativeD3d11View* view;
    if (desc && desc->Format != DXGI_FORMAT_UNKNOWN &&
        native_dxgi_image_format(desc->Format) == 0u) return E_INVALIDARG;
    if (FAILED(native_create_view(native_device_from_interface(self), resource,
                                  NATIVE_D3D11_VIEW_SRV,
                                  (ID3D11View**)&view))) return E_INVALIDARG;
    *out = (ID3D11ShaderResourceView*)(void*)view;
    return S_OK;
}

static HRESULT WINAPI native_device_create_unordered_access_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
    ID3D11UnorderedAccessView** out) {
    NativeD3d11View* view;
    if (!out) return E_POINTER;
    *out = NULL;
    if (desc && desc->Format != DXGI_FORMAT_UNKNOWN &&
        native_dxgi_image_format(desc->Format) == 0u) return E_INVALIDARG;
    if (FAILED(native_create_view(native_device_from_interface(self), resource,
                                  NATIVE_D3D11_VIEW_UAV,
                                  (ID3D11View**)&view))) return E_INVALIDARG;
    *out = (ID3D11UnorderedAccessView*)(void*)view;
    return S_OK;
}

static HRESULT WINAPI native_device_create_render_target_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc,
    ID3D11RenderTargetView** out) {
    NativeD3d11View* view;
    if (!out) return E_POINTER;
    *out = NULL;
    if (desc && desc->Format != DXGI_FORMAT_UNKNOWN &&
        native_dxgi_image_format(desc->Format) == 0u) return E_INVALIDARG;
    if (FAILED(native_create_view(native_device_from_interface(self), resource,
                                  NATIVE_D3D11_VIEW_RTV,
                                  (ID3D11View**)&view))) return E_INVALIDARG;
    *out = (ID3D11RenderTargetView*)(void*)view;
    return S_OK;
}

static HRESULT WINAPI native_device_create_depth_stencil_view(
    ID3D11Device* self, ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
    ID3D11DepthStencilView** out) {
    NativeD3d11View* view;
    if (!out) return E_POINTER;
    *out = NULL;
    if (desc && desc->Format != DXGI_FORMAT_UNKNOWN &&
        native_dxgi_image_format(desc->Format) == 0u) return E_INVALIDARG;
    if (FAILED(native_create_view(native_device_from_interface(self), resource,
                                  NATIVE_D3D11_VIEW_DSV,
                                  (ID3D11View**)&view))) return E_INVALIDARG;
    *out = (ID3D11DepthStencilView*)(void*)view;
    return S_OK;
}

static HRESULT WINAPI native_device_create_input_layout(
    ID3D11Device* self, const D3D11_INPUT_ELEMENT_DESC* elements,
    UINT count, const void* bytecode, SIZE_T bytecode_size,
    ID3D11InputLayout** out) {
    NativeD3d11InputLayout* layout;
    UINT index;
    (void)bytecode;
    (void)bytecode_size;
    if (!out) return E_POINTER;
    *out = NULL;
    if ((count != 0u && !elements) || count > 16u) return E_INVALIDARG;
    layout = (NativeD3d11InputLayout*)calloc(1u, sizeof(*layout));
    if (!layout) return E_OUTOFMEMORY;
    layout->iface.lpVtbl = (ID3D11InputLayoutVtbl*)(void*)&native_layout_vtable;
    layout->references = 1;
    layout->device = native_device_from_interface(self);
    layout->attribute_count = count;
    layout->layout_count = 0u;
    for (index = 0u; index < count; ++index) {
        uint32_t format = native_dxgi_vertex_format(elements[index].Format);
        UINT binding = elements[index].InputSlot;
        if (format == 0u || binding >= 16u || elements[index].InputSlotClass !=
            D3D11_INPUT_PER_VERTEX_DATA) {
            free(layout);
            return E_INVALIDARG;
        }
        layout->attributes[index].abi_version = RIN_GPU_ABI_VERSION;
        layout->attributes[index].struct_size = sizeof(layout->attributes[index]);
        layout->attributes[index].location = elements[index].SemanticIndex;
        layout->attributes[index].format = format;
        layout->attributes[index].offset = elements[index].AlignedByteOffset ==
                                                   D3D11_APPEND_ALIGNED_ELEMENT
                                               ? 0u
                                               : elements[index].AlignedByteOffset;
        layout->attributes[index].binding = binding;
        if (binding >= layout->layout_count) layout->layout_count = binding + 1u;
        layout->layouts[binding].binding = binding;
        layout->layouts[binding].stride = 0u;
    }
    for (index = 0u; index < layout->layout_count; ++index)
        layout->layouts[index].stride = 4u;
    ID3D11Device_AddRef(self);
    *out = &layout->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_create_vertex_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11VertexShader** out) {
    ID3D11DeviceChild* child;
    (void)linkage;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(native_create_shader(native_device_from_interface(self),
                                    RIN_SHADER_STAGE_VERTEX, bytecode,
                                    bytecode_size, &child))) return E_INVALIDARG;
    *out = (ID3D11VertexShader*)(void*)child;
    return S_OK;
}
static HRESULT WINAPI native_device_create_pixel_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11PixelShader** out) {
    ID3D11DeviceChild* child;
    (void)linkage;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(native_create_shader(native_device_from_interface(self),
                                    RIN_SHADER_STAGE_FRAGMENT, bytecode,
                                    bytecode_size, &child))) return E_INVALIDARG;
    *out = (ID3D11PixelShader*)(void*)child;
    return S_OK;
}
static HRESULT WINAPI native_device_create_compute_shader(
    ID3D11Device* self, const void* bytecode, SIZE_T bytecode_size,
    ID3D11ClassLinkage* linkage, ID3D11ComputeShader** out) {
    ID3D11DeviceChild* child;
    (void)linkage;
    if (!out) return E_POINTER;
    *out = NULL;
    if (FAILED(native_create_shader(native_device_from_interface(self),
                                    RIN_SHADER_STAGE_COMPUTE, bytecode,
                                    bytecode_size, &child))) return E_INVALIDARG;
    *out = (ID3D11ComputeShader*)(void*)child;
    return S_OK;
}

static HRESULT WINAPI native_device_create_sampler_state(
    ID3D11Device* self, const D3D11_SAMPLER_DESC* desc,
    ID3D11SamplerState** out) {
    RinGpuSamplerDescV1 sampler;
    RinGpuHandle handle = 0u;
    NativeD3d11Sampler* native_sampler;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!desc || desc->Filter != D3D11_FILTER_MIN_MAG_MIP_POINT &&
        desc->Filter != D3D11_FILTER_MIN_MAG_MIP_LINEAR)
        return E_INVALIDARG;
    memset(&sampler, 0, sizeof(sampler));
    sampler.abi_version = RIN_GPU_ABI_VERSION;
    sampler.struct_size = sizeof(sampler);
    sampler.min_filter = desc->Filter == D3D11_FILTER_MIN_MAG_MIP_LINEAR
                             ? RIN_GPU_SAMPLER_FILTER_LINEAR
                             : RIN_GPU_SAMPLER_FILTER_NEAREST;
    sampler.mag_filter = sampler.min_filter;
    sampler.mip_filter = RIN_GPU_SAMPLER_MIP_FILTER_NONE;
    sampler.address_u = desc->AddressU == D3D11_TEXTURE_ADDRESS_WRAP
                            ? RIN_GPU_SAMPLER_ADDRESS_REPEAT
                            : RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler.address_v = desc->AddressV == D3D11_TEXTURE_ADDRESS_WRAP
                            ? RIN_GPU_SAMPLER_ADDRESS_REPEAT
                            : RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler.address_w = desc->AddressW == D3D11_TEXTURE_ADDRESS_WRAP
                            ? RIN_GPU_SAMPLER_ADDRESS_REPEAT
                            : RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler.max_anisotropy = 1u;
    result = rindx_d3d11_create_sampler(&native_device_from_interface(self)->core,
                                        &sampler, &handle);
    if (result != RIN_GPU_OK) return core_result(result);
    native_sampler = (NativeD3d11Sampler*)calloc(1u, sizeof(*native_sampler));
    if (!native_sampler) {
        (void)rindx_d3d11_destroy_object(&native_device_from_interface(self)->core,
                                         handle);
        return E_OUTOFMEMORY;
    }
    native_sampler->iface.lpVtbl = (ID3D11DeviceChildVtbl*)(void*)&native_sampler_vtable;
    native_sampler->references = 1;
    native_sampler->device = native_device_from_interface(self);
    native_sampler->handle = handle;
    ID3D11Device_AddRef(self);
    *out = (ID3D11SamplerState*)(void*)native_sampler;
    return S_OK;
}

static HRESULT WINAPI native_device_create_deferred_context(
    ID3D11Device* self, UINT flags, ID3D11DeviceContext** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    NativeD3d11Context* context;
    int result;
    if (!out) return E_POINTER;
    *out = NULL;
    if (flags != 0u) return E_INVALIDARG;
    context = (NativeD3d11Context*)calloc(1u, sizeof(*context));
    if (!context) return E_OUTOFMEMORY;
    result = rindx_d3d11_create_deferred_context(&device->core, &context->core);
    if (result != RIN_GPU_OK) { free(context); return core_result(result); }
    context->iface.lpVtbl = (ID3D11DeviceContextVtbl*)(void*)&native_context_vtable_bound;
    context->references = 1;
    context->device = device;
    context->topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ID3D11Device_AddRef(self);
    *out = &context->iface;
    return S_OK;
}

static HRESULT WINAPI native_device_check_feature_support(
    ID3D11Device* self, D3D11_FEATURE feature, void* data, UINT size) {
    (void)self;
    if (!data) return E_POINTER;
    if (feature == D3D11_FEATURE_D3D11_OPTIONS && size >= sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS)) {
        memset(data, 0, sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS));
        return S_OK;
    }
    if (feature == D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS &&
        size >= sizeof(D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS)) {
        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS* options =
            (D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS*)data;
        options->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x =
            FALSE;
        return S_OK;
    }
    return E_INVALIDARG;
}

static D3D_FEATURE_LEVEL WINAPI native_device_get_feature_level(ID3D11Device* self) {
    (void)self;
    return D3D_FEATURE_LEVEL_11_0;
}
static UINT WINAPI native_device_get_creation_flags(ID3D11Device* self) {
    return native_device_from_interface(self)->creation_flags;
}
static HRESULT WINAPI native_device_get_removed_reason(ID3D11Device* self) {
    return rindx_d3d11_get_device_removed_reason_hresult(
        &native_device_from_interface(self)->core);
}
static void WINAPI native_device_get_immediate_context(
    ID3D11Device* self, ID3D11DeviceContext** out) {
    NativeD3d11Device* device = native_device_from_interface(self);
    if (!out) return;
    *out = device->immediate ? &device->immediate->iface : NULL;
    if (*out) ID3D11DeviceContext_AddRef(*out);
}

static HRESULT WINAPI native_context_query_interface(
    ID3D11DeviceContext* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ID3D11DeviceChild) ||
        IsEqualIID(iid, &IID_ID3D11DeviceContext)) {
        *out = self;
        ID3D11DeviceContext_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI native_context_add_ref(ID3D11DeviceContext* self) {
    return (ULONG)InterlockedIncrement(
        &native_context_from_interface(self)->references);
}
static ULONG WINAPI native_context_release(ID3D11DeviceContext* self) {
    NativeD3d11Context* context = native_context_from_interface(self);
    LONG references = InterlockedDecrement(&context->references);
    if (references == 0) {
        native_context_release_state(context);
        (void)rindx_d3d11_destroy_context(&context->core);
        ID3D11Device_Release(&context->device->iface);
        free(context);
    }
    return (ULONG)references;
}
static void WINAPI native_context_get_device(
    ID3D11DeviceContext* self, ID3D11Device** out) {
    NativeD3d11Context* context = native_context_from_interface(self);
    if (!out) return;
    *out = &context->device->iface;
    ID3D11Device_AddRef(*out);
}
static NativeD3d11Resource* native_resource_from_view(void* view) {
    NativeD3d11View* native_view = (NativeD3d11View*)view;
    return native_view ? native_view->resource : NULL;
}
static void WINAPI native_context_draw(ID3D11DeviceContext* self,
                                       UINT vertex_count, UINT first_vertex) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuDrawV1 draw;
    if (native_context_prepare_graphics(context) != RIN_GPU_OK ||
        native_context_begin_pass(context) != RIN_GPU_OK) return;
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.vertex_count = vertex_count;
    draw.instance_count = 1u;
    draw.first_vertex = first_vertex;
    draw.first_instance = 0u;
    draw.pipeline = context->graphics_pipeline;
    draw.color_target = context->color_view->resource->handle;
    (void)rindx_d3d11_draw(&context->core, &draw);
}
static void WINAPI native_context_draw_indexed(ID3D11DeviceContext* self,
                                               UINT index_count,
                                               UINT first_index,
                                               INT base_vertex) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuDrawIndexedV2 draw;
    if (native_context_prepare_graphics(context) != RIN_GPU_OK ||
        native_context_begin_pass(context) != RIN_GPU_OK ||
        !context->index_buffer) return;
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.index_count = index_count;
    draw.instance_count = 1u;
    draw.first_index = first_index;
    draw.vertex_count = base_vertex < 0 ? 0u : (uint32_t)base_vertex;
    draw.pipeline = context->graphics_pipeline;
    draw.color_target = context->color_view->resource->handle;
    draw.index_buffer = context->index_buffer->handle;
    draw.index_offset = context->index_offset;
    draw.index_format = context->index_format == DXGI_FORMAT_R16_UINT ? 16u : 32u;
    (void)rindx_d3d11_draw_indexed_instanced(&context->core, &draw);
}
static void WINAPI native_context_draw_instanced(ID3D11DeviceContext* self,
                                                 UINT vertex_count,
                                                 UINT instance_count,
                                                 UINT first_vertex,
                                                 UINT first_instance) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuDrawV1 draw;
    if (native_context_prepare_graphics(context) != RIN_GPU_OK ||
        native_context_begin_pass(context) != RIN_GPU_OK) return;
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.vertex_count = vertex_count;
    draw.instance_count = instance_count;
    draw.first_vertex = first_vertex;
    draw.first_instance = first_instance;
    draw.pipeline = context->graphics_pipeline;
    draw.color_target = context->color_view->resource->handle;
    (void)rindx_d3d11_draw(&context->core, &draw);
}
static void WINAPI native_context_draw_indexed_instanced(
    ID3D11DeviceContext* self, UINT index_count, UINT instance_count,
    UINT first_index, INT base_vertex, UINT first_instance) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuDrawIndexedV2 draw;
    if (native_context_prepare_graphics(context) != RIN_GPU_OK ||
        native_context_begin_pass(context) != RIN_GPU_OK ||
        !context->index_buffer) return;
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.index_count = index_count;
    draw.instance_count = instance_count;
    draw.first_index = first_index;
    draw.vertex_count = base_vertex < 0 ? 0u : (uint32_t)base_vertex;
    draw.first_instance = first_instance;
    draw.pipeline = context->graphics_pipeline;
    draw.color_target = context->color_view->resource->handle;
    draw.index_buffer = context->index_buffer->handle;
    draw.index_offset = context->index_offset;
    draw.index_format = context->index_format == DXGI_FORMAT_R16_UINT ? 16u : 32u;
    (void)rindx_d3d11_draw_indexed_instanced(&context->core, &draw);
}
static HRESULT WINAPI native_context_map(
    ID3D11DeviceContext* self, ID3D11Resource* resource, UINT subresource,
    D3D11_MAP map_type, UINT flags, D3D11_MAPPED_SUBRESOURCE* mapped) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* native_resource = native_resource_from_resource(resource);
    RinDxD3d11MappedResource core_mapped;
    int result;
    if (subresource != 0u || !native_resource || !mapped) return E_INVALIDARG;
    memset(&core_mapped, 0, sizeof(core_mapped));
    core_mapped.struct_size = sizeof(core_mapped);
    core_mapped.version = RIN_DX_D3D11_VERSION;
    result = rindx_d3d11_map_buffer(&context->core, native_resource->handle,
                                    (uint32_t)map_type, flags, &core_mapped);
    if (result != RIN_GPU_OK) return core_result(result);
    context->mapped = core_mapped;
    mapped->pData = core_mapped.data;
    mapped->RowPitch = (UINT)core_mapped.row_pitch_bytes;
    mapped->DepthPitch = (UINT)core_mapped.depth_pitch_bytes;
    return S_OK;
}
static void WINAPI native_context_unmap(ID3D11DeviceContext* self,
                                        ID3D11Resource* resource,
                                        UINT subresource) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* native_resource = native_resource_from_resource(resource);
    if (subresource == 0u && native_resource)
        (void)rindx_d3d11_unmap_buffer(&context->core, native_resource->handle,
                                       &context->mapped);
}
static void WINAPI native_context_copy_resource(ID3D11DeviceContext* self,
                                                ID3D11Resource* destination,
                                                ID3D11Resource* source) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* dst = native_resource_from_resource(destination);
    NativeD3d11Resource* src = native_resource_from_resource(source);
    if (dst && src) (void)rindx_d3d11_copy_resource(&context->core, dst->handle,
                                                    src->handle);
}
static void WINAPI native_context_copy_subresource_region(
    ID3D11DeviceContext* self, ID3D11Resource* destination,
    UINT destination_subresource, UINT dst_x, UINT dst_y, UINT dst_z,
    ID3D11Resource* source, UINT source_subresource, const D3D11_BOX* box) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* dst = native_resource_from_resource(destination);
    NativeD3d11Resource* src = native_resource_from_resource(source);
    RinGpuImageCopyRegionV1 region;
    if (!dst || !src || destination_subresource != 0u || source_subresource != 0u)
        return;
    memset(&region, 0, sizeof(region));
    region.abi_version = RIN_GPU_ABI_VERSION;
    region.struct_size = sizeof(region);
    region.destination_mip_level = 0u;
    region.source_mip_level = 0u;
    region.destination_x = dst_x;
    region.destination_y = dst_y;
    region.destination_z = dst_z;
    if (box) {
        if (box->right < box->left || box->bottom < box->top || box->back < box->front)
            return;
        region.source_x = box->left;
        region.source_y = box->top;
        region.source_z = box->front;
        region.width = box->right - box->left;
        region.height = box->bottom - box->top;
        region.depth = box->back - box->front;
    }
    (void)rindx_d3d11_copy_subresource_region(&context->core, dst->handle,
                                               src->handle, &region);
}
static void WINAPI native_context_update_subresource(
    ID3D11DeviceContext* self, ID3D11Resource* resource, UINT subresource,
    const D3D11_BOX* box, const void* data, UINT row_pitch, UINT depth_pitch) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* target = native_resource_from_resource(resource);
    if (!target || !data || subresource != 0u) return;
    if (target->kind == NATIVE_D3D11_BUFFER) {
        UINT64 size = target->buffer_desc.ByteWidth;
        UINT64 offset = box ? box->left : 0u;
        if (box) size = box->right >= box->left ? box->right - box->left : 0u;
        (void)rindx_d3d11_update_subresource_buffer(&context->device->core,
                                                    target->handle, offset,
                                                    data, size);
    } else {
        RinGpuImageUploadV1 upload;
        UINT width = target->texture2d_desc.Width;
        UINT height = target->texture2d_desc.Height;
        memset(&upload, 0, sizeof(upload));
        upload.abi_version = RIN_GPU_ABI_VERSION;
        upload.struct_size = sizeof(upload);
        upload.width = box ? box->right - box->left : width;
        upload.height = box ? box->bottom - box->top : height;
        upload.depth = 1u;
        upload.x = box ? box->left : 0u;
        upload.y = box ? box->top : 0u;
        upload.source_row_pitch_bytes = row_pitch;
        upload.source_slice_pitch_bytes = depth_pitch ? depth_pitch : row_pitch * upload.height;
        (void)rindx_d3d11_update_subresource_texture2d(
            &context->device->core, target->handle, &upload, data,
            upload.source_slice_pitch_bytes);
    }
}
static void WINAPI native_context_clear_rtv(
    ID3D11DeviceContext* self, ID3D11RenderTargetView* view,
    const FLOAT color[4]) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* target = native_resource_from_view(view);
    if (!target || !color) return;
    if (context->core.render_pass_active != 0u &&
        rindx_d3d11_end_render_pass(&context->core) != RIN_GPU_OK)
        return;
    if (target->image_state != RIN_GPU_IMAGE_STATE_COLOR_TARGET) {
        if (rindx_d3d11_transition_image(
                &context->core, target->handle, target->image_state,
                RIN_GPU_IMAGE_STATE_COLOR_TARGET) != RIN_GPU_OK)
            return;
        target->image_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    }
    (void)rindx_d3d11_begin_render_pass(
        &context->core, target->handle, 0u, 1u, color[0], color[1], color[2],
        color[3], 1.0f);
}
static void WINAPI native_context_clear_dsv(
    ID3D11DeviceContext* self, ID3D11DepthStencilView* view, UINT flags,
    FLOAT depth, UINT8 stencil) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* target = native_resource_from_view(view);
    if (target)
        (void)rindx_d3d11_clear_depth_stencil_view(&context->core,
                                                   target->handle, flags,
                                                   depth, stencil);
}
static void WINAPI native_context_generate_mips(
    ID3D11DeviceContext* self, ID3D11ShaderResourceView* view) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* target = native_resource_from_view(view);
    if (target) (void)rindx_d3d11_generate_mips(&context->core, target->handle);
}
static void WINAPI native_context_resolve(
    ID3D11DeviceContext* self, ID3D11Resource* destination,
    UINT destination_subresource, ID3D11Resource* source,
    UINT source_subresource, DXGI_FORMAT format) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11Resource* dst = native_resource_from_resource(destination);
    NativeD3d11Resource* src = native_resource_from_resource(source);
    RinGpuImageResolveV1 resolve;
    if (!dst || !src || destination_subresource != 0u || source_subresource != 0u)
        return;
    memset(&resolve, 0, sizeof(resolve));
    resolve.abi_version = RIN_GPU_ABI_VERSION;
    resolve.struct_size = sizeof(resolve);
    resolve.destination_mip_level = 0u;
    resolve.source_mip_level = 0u;
    resolve.width = dst->texture2d_desc.Width;
    resolve.height = dst->texture2d_desc.Height;
    resolve.source_x = 0u;
    resolve.source_y = 0u;
    resolve.destination_x = 0u;
    resolve.destination_y = 0u;
    (void)format;
    (void)rindx_d3d11_resolve_subresource(&context->core, dst->handle, src->handle,
                                          &resolve);
}
static void WINAPI native_context_dispatch(ID3D11DeviceContext* self,
                                           UINT x, UINT y, UINT z) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuDispatchV1 dispatch;
    int result;
    if (!context->compute_shader) return;
    if (context->compute_pipeline == 0u) {
        result = rindx_d3d11_create_compute_pipeline(
            &context->device->core, context->compute_shader->handle,
            &context->compute_pipeline);
        if (result != RIN_GPU_OK) return;
        result = rindx_d3d11_create_compute_bind_group(
            &context->device->core, context->compute_pipeline, NULL, 0u,
            &context->bind_group);
        if (result != RIN_GPU_OK) {
            (void)rindx_d3d11_destroy_object(&context->device->core,
                                             context->compute_pipeline);
            context->compute_pipeline = 0u;
            return;
        }
    }
    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.abi_version = RIN_GPU_ABI_VERSION;
    dispatch.struct_size = sizeof(dispatch);
    dispatch.group_count_x = x;
    dispatch.group_count_y = y;
    dispatch.group_count_z = z;
    dispatch.pipeline = context->compute_pipeline;
    dispatch.bind_group = context->bind_group;
    (void)rindx_d3d11_dispatch(&context->core, &dispatch);
}
static HRESULT WINAPI native_context_finish_command_list(
    ID3D11DeviceContext* self, BOOL restore, ID3D11CommandList** out) {
    NativeD3d11Context* context = native_context_from_interface(self);
    RinGpuHandle command_list = 0u;
    NativeD3d11CommandList* native_list;
    int result;
    (void)restore;
    if (!out) return E_POINTER;
    *out = NULL;
    result = rindx_d3d11_finish_command_list(&context->core, &command_list);
    if (result != RIN_GPU_OK) return core_result(result);
    native_list = (NativeD3d11CommandList*)calloc(1u, sizeof(*native_list));
    if (!native_list) {
        (void)rindx_d3d11_destroy_object(&context->device->core, command_list);
        return E_OUTOFMEMORY;
    }
    native_list->iface.lpVtbl = (ID3D11CommandListVtbl*)(void*)&native_command_list_vtable;
    native_list->references = 1;
    native_list->device = context->device;
    native_list->handle = command_list;
    native_list->context_flags = restore ? 1u : 0u;
    ID3D11Device_AddRef(&context->device->iface);
    *out = &native_list->iface;
    return S_OK;
}
static void WINAPI native_context_execute_command_list(
    ID3D11DeviceContext* self, ID3D11CommandList* list, BOOL restore) {
    NativeD3d11Context* context = native_context_from_interface(self);
    NativeD3d11CommandList* native_list = list
        ? native_command_list_from_interface(list) : NULL;
    uint64_t fence = 0u;
    if (!native_list || native_list->device != context->device || restore)
        return;
    (void)rindx_d3d11_execute_command_list(&context->core,
                                           native_list->handle, 0u, &fence);
}
static void WINAPI native_context_clear_state(ID3D11DeviceContext* self) {
    NativeD3d11Context* context = native_context_from_interface(self);
    (void)rindx_d3d11_end_render_pass(&context->core);
    native_context_release_state(context);
    context->current_target = NULL;
}
static void WINAPI native_context_flush(ID3D11DeviceContext* self) {
    NativeD3d11Context* context = native_context_from_interface(self);
    uint64_t fence = 0u;
    (void)rindx_d3d11_close_and_submit(&context->core, &fence);
    (void)rindx_d3d11_wait(&context->device->core, fence, RIN_GPU_TIMEOUT_INFINITE);
}
static UINT WINAPI native_context_get_flags(ID3D11DeviceContext* self) {
    (void)self;
    return 0u;
}

static NativeD3d11Swapchain* native_swapchain_from_interface(
    IDXGISwapChain* self) {
    return (NativeD3d11Swapchain*)(void*)self;
}

static int native_swapchain_window_retain(void* context, void* native_window) {
    (void)context;
    return IsWindow((HWND)native_window) ? 0 : -1;
}
static void native_swapchain_window_release(void* context, void* native_window) {
    (void)context; (void)native_window;
}
static int native_swapchain_window_validate(void* context, void* native_window,
                                            uint32_t display_id) {
    (void)context;
    return IsWindow((HWND)native_window) && display_id == RIN_GPU_PRIMARY_DISPLAY
               ? 0
               : -1;
}

static int native_swapchain_backend_submit(
    void* context, const RinGpuPresentationSubmitV1* submit,
    uint64_t fence_value) {
    NativeD3d11Swapchain* swapchain = (NativeD3d11Swapchain*)context;
    RinGpuDxgiSwapchainBufferV1 buffer;
    RinGpuImageReadbackV1 readback;
    DXGI_FRAME_STATISTICS unused_stats;
    BITMAPINFO bitmap_info;
    void* pixels;
    HDC dc;
    RECT client;
    size_t size;
    uint32_t buffer_index = UINT32_MAX;
    int result;
    (void)unused_stats;
    (void)fence_value;
    if (!swapchain || !submit || submit->image_token == 0u) {
        return -1;
    }
    memset(&buffer, 0, sizeof(buffer));
    buffer.struct_size = sizeof(buffer);
    buffer.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    result = rin_gpu_dxgi_swapchain_get_buffer(&swapchain->core,
                                               (uint32_t)0u, &buffer);
    if (result != RIN_GPU_DXGI_SWAPCHAIN_OK) {
        return -1;
    }
    for (uint32_t index = 0u; index < swapchain->buffer_count; ++index) {
        RinGpuDxgiSwapchainBufferV1 candidate;
        memset(&candidate, 0, sizeof(candidate));
        candidate.struct_size = sizeof(candidate);
        candidate.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
        if (rin_gpu_dxgi_swapchain_get_buffer(&swapchain->core, index,
                                              &candidate) ==
                RIN_GPU_DXGI_SWAPCHAIN_OK &&
            candidate.image_token == submit->image_token) {
            buffer = candidate;
            buffer_index = index;
            break;
        }
    }
    if (buffer_index == UINT32_MAX || buffer.image_token != submit->image_token ||
        buffer.resource_handle == 0u) {
        return -1;
    }
    {
        NativeD3d11Context* immediate = swapchain->device->immediate;
        if (!immediate) return -1;
        if (immediate->core.render_pass_active != 0u &&
            rindx_d3d11_end_render_pass(&immediate->core) != RIN_GPU_OK)
            return -1;
        {
            int transition_result = rindx_d3d11_transition_image(
                &immediate->core, buffer.resource_handle,
                swapchain->buffers[buffer_index]->image_state,
                RIN_GPU_IMAGE_STATE_COPY_SOURCE);
            if (transition_result != RIN_GPU_OK) {
                return -1;
            }
        }
        {
            uint64_t fence = 0u;
            if (rindx_d3d11_close_and_submit(&immediate->core, &fence) !=
                    RIN_GPU_OK ||
                rindx_d3d11_wait(&swapchain->device->core, fence,
                                 RIN_GPU_TIMEOUT_INFINITE) != RIN_GPU_OK)
                return -1;
        }
        swapchain->buffers[buffer_index]->image_state =
            RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    }
    size = (size_t)buffer.width * (size_t)buffer.height * 4u;
    pixels = calloc(1u, size);
    if (!pixels) return -1;
    memset(&readback, 0, sizeof(readback));
    readback.abi_version = RIN_GPU_ABI_VERSION;
    readback.struct_size = sizeof(readback);
    readback.width = buffer.width;
    readback.height = buffer.height;
    readback.depth = 1u;
    readback.destination_row_pitch_bytes = (uint64_t)buffer.width * 4u;
    readback.destination_slice_pitch_bytes = (uint64_t)size;
    result = rindx_d3d11_readback_image(&swapchain->device->core,
                                        buffer.resource_handle, &readback,
                                        pixels, (uint64_t)size);
    if (result == RIN_GPU_OK && GetClientRect(swapchain->window, &client)) {
        memset(&bitmap_info, 0, sizeof(bitmap_info));
        bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
        bitmap_info.bmiHeader.biWidth = (LONG)buffer.width;
        bitmap_info.bmiHeader.biHeight = -(LONG)buffer.height;
        bitmap_info.bmiHeader.biPlanes = 1u;
        bitmap_info.bmiHeader.biBitCount = 32u;
        bitmap_info.bmiHeader.biCompression = BI_RGB;
        dc = GetDC(swapchain->window);
        if (dc) {
            if (StretchDIBits(dc, 0, 0, client.right - client.left,
                              client.bottom - client.top, 0, 0,
                              (int)buffer.width, (int)buffer.height, pixels,
                              &bitmap_info, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR)
                result = RIN_GPU_ERROR_BACKEND;
            ReleaseDC(swapchain->window, dc);
        } else {
            result = RIN_GPU_ERROR_BACKEND;
        }
    } else if (result == RIN_GPU_OK) {
        result = RIN_GPU_ERROR_BACKEND;
    }
    free(pixels);
    {
        NativeD3d11Context* immediate = swapchain->device->immediate;
        uint64_t fence = 0u;
        if (!immediate || rindx_d3d11_transition_image(
                &immediate->core, buffer.resource_handle,
                RIN_GPU_IMAGE_STATE_COPY_SOURCE,
                RIN_GPU_IMAGE_STATE_PRESENT) != RIN_GPU_OK ||
            rindx_d3d11_close_and_submit(&immediate->core, &fence) !=
                RIN_GPU_OK ||
            rindx_d3d11_wait(&swapchain->device->core, fence,
                             RIN_GPU_TIMEOUT_INFINITE) != RIN_GPU_OK)
            result = RIN_GPU_ERROR_BACKEND;
        else
            swapchain->buffers[buffer_index]->image_state =
                RIN_GPU_IMAGE_STATE_PRESENT;
    }
    return result == RIN_GPU_OK ? 0 : -1;
}

static int native_swapchain_backend_cancel(
    void* context, const RinGpuPresentationCompletionV1* pending) {
    (void)context; (void)pending;
    return 0;
}

static HRESULT native_swapchain_create_buffers(NativeD3d11Swapchain* swapchain) {
    uint32_t index;
    uint32_t format = native_dxgi_image_format(swapchain->desc.BufferDesc.Format);
    if (format == 0u) return E_INVALIDARG;
    for (index = 0u; index < swapchain->buffer_count; ++index) {
        RinGpuImageDescV1 image_desc;
        RinGpuDxgiSwapchainBufferV1 buffer;
        NativeD3d11Resource* resource;
        int result;
        memset(&image_desc, 0, sizeof(image_desc));
        image_desc.abi_version = RIN_GPU_ABI_VERSION;
        image_desc.struct_size = sizeof(image_desc);
        image_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
        image_desc.format = format;
        image_desc.width = swapchain->desc.BufferDesc.Width;
        image_desc.height = swapchain->desc.BufferDesc.Height;
        image_desc.depth = 1u;
        image_desc.array_layers = 1u;
        image_desc.mip_levels = 1u;
        image_desc.sample_count = 1u;
        image_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                           RIN_GPU_IMAGE_COPY_DESTINATION |
                           RIN_GPU_IMAGE_COLOR_TARGET |
                           RIN_GPU_IMAGE_PRESENT;
        image_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
        resource = (NativeD3d11Resource*)calloc(1u, sizeof(*resource));
        if (!resource) return E_OUTOFMEMORY;
        result = rindx_d3d11_create_texture2d(&swapchain->device->core,
                                              &image_desc, &resource->handle);
        if (result != RIN_GPU_OK) { free(resource); return core_result(result); }
        resource->iface.lpVtbl = (ID3D11BufferVtbl*)(void*)&native_texture2d_vtable;
        resource->references = 1;
        resource->device = swapchain->device;
        resource->kind = NATIVE_D3D11_TEXTURE2D;
        resource->image_state = RIN_GPU_IMAGE_STATE_UNDEFINED;
        memset(&resource->texture2d_desc, 0, sizeof(resource->texture2d_desc));
        resource->texture2d_desc.Width = swapchain->desc.BufferDesc.Width;
        resource->texture2d_desc.Height = swapchain->desc.BufferDesc.Height;
        resource->texture2d_desc.MipLevels = 1u;
        resource->texture2d_desc.ArraySize = 1u;
        resource->texture2d_desc.Format = swapchain->desc.BufferDesc.Format;
        resource->texture2d_desc.SampleDesc.Count = 1u;
        resource->texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
        resource->texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Device_AddRef(&swapchain->device->iface);
        swapchain->buffers[index] = resource;
        memset(&buffer, 0, sizeof(buffer));
        buffer.struct_size = sizeof(buffer);
        buffer.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
        result = rin_gpu_dxgi_swapchain_get_buffer(&swapchain->core, index,
                                                   &buffer);
        if (result != RIN_GPU_DXGI_SWAPCHAIN_OK ||
            rin_gpu_dxgi_swapchain_bind_buffer(&swapchain->core,
                                               buffer.image_token,
                                               resource->handle) !=
                RIN_GPU_DXGI_SWAPCHAIN_OK)
            return E_FAIL;
    }
    return S_OK;
}

static void native_swapchain_destroy_buffers(NativeD3d11Swapchain* swapchain) {
    uint32_t index;
    for (index = 0u; index < swapchain->buffer_count; ++index) {
        RinGpuDxgiSwapchainBufferV1 buffer;
        if (swapchain->buffers[index]) {
            memset(&buffer, 0, sizeof(buffer));
            buffer.struct_size = sizeof(buffer);
            buffer.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
            if (rin_gpu_dxgi_swapchain_get_buffer(&swapchain->core, index,
                                                  &buffer) ==
                RIN_GPU_DXGI_SWAPCHAIN_OK)
                (void)rin_gpu_dxgi_swapchain_unbind_buffer(
                    &swapchain->core, buffer.image_token,
                    swapchain->buffers[index]->handle);
            ID3D11Buffer_Release(&swapchain->buffers[index]->iface);
            swapchain->buffers[index] = NULL;
        }
    }
}

static HRESULT WINAPI native_swapchain_query_interface(
    IDXGISwapChain* self, REFIID iid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_IDXGIObject) ||
        IsEqualIID(iid, &IID_IDXGIDeviceSubObject) ||
        IsEqualIID(iid, &IID_IDXGISwapChain)) {
        *out = self;
        IDXGISwapChain_AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI native_swapchain_add_ref(IDXGISwapChain* self) {
    return (ULONG)InterlockedIncrement(
        &native_swapchain_from_interface(self)->references);
}
static ULONG WINAPI native_swapchain_release(IDXGISwapChain* self) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    LONG references = InterlockedDecrement(&swapchain->references);
    if (references == 0) {
        native_swapchain_destroy_buffers(swapchain);
        native_private_dispose(&swapchain->private_data);
        (void)rin_gpu_dxgi_swapchain_runtime_shutdown(&swapchain->core);
        ID3D11Device_Release(&swapchain->device->iface);
        free(swapchain);
    }
    return (ULONG)references;
}
static HRESULT WINAPI native_swapchain_set_private_data(
    IDXGISwapChain* self, REFGUID guid, UINT size, const void* data) {
    return native_private_set(
        &native_swapchain_from_interface(self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_swapchain_set_private_interface(
    IDXGISwapChain* self, REFGUID guid, const IUnknown* object) {
    return native_private_set_interface(
        &native_swapchain_from_interface(self)->private_data, guid, object);
}
static HRESULT WINAPI native_swapchain_get_private_data(
    IDXGISwapChain* self, REFGUID guid, UINT* size, void* data) {
    return native_private_get(
        native_swapchain_from_interface(self)->private_data,
        guid, size, data);
}
static HRESULT WINAPI native_swapchain_get_parent(
    IDXGISwapChain* self, REFIID iid, void** out) {
    (void)self; (void)iid;
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOINTERFACE;
}
static HRESULT WINAPI native_swapchain_get_device(
    IDXGISwapChain* self, REFIID iid, void** out) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    if (!out) return E_POINTER;
    *out = NULL;
    return ID3D11Device_QueryInterface(&swapchain->device->iface, iid, out);
}
static HRESULT WINAPI native_swapchain_present(
    IDXGISwapChain* self, UINT sync_interval, UINT flags) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    RinGpuPresentationAcquireV1 acquire;
    RinGpuPresentationSubmitV1 submit;
    RinGpuPresentationCompletionV1 completion;
    uint64_t presentation_fence = 0u;
    int result;
    if (sync_interval > 4u || (flags & ~(DXGI_PRESENT_DO_NOT_WAIT |
                                          DXGI_PRESENT_TEST)) != 0u)
        return E_INVALIDARG;
    memset(&acquire, 0, sizeof(acquire));
    acquire.struct_size = sizeof(acquire);
    acquire.version = RIN_GPU_PRESENTATION_VERSION;
    result = rin_gpu_dxgi_swapchain_acquire(&swapchain->core, &acquire);
    if (result == RIN_GPU_DXGI_SWAPCHAIN_WAS_STILL_DRAWING)
        return (flags & DXGI_PRESENT_DO_NOT_WAIT) ? DXGI_ERROR_WAS_STILL_DRAWING
                                                  : DXGI_ERROR_WAS_STILL_DRAWING;
    if (result != RIN_GPU_DXGI_SWAPCHAIN_OK &&
        result != RIN_GPU_DXGI_SWAPCHAIN_SUBOPTIMAL)
        return result == RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST
                   ? DXGI_ERROR_DEVICE_REMOVED
                   : DXGI_ERROR_INVALID_CALL;
    memset(&submit, 0, sizeof(submit));
    submit.struct_size = sizeof(submit);
    submit.version = RIN_GPU_PRESENTATION_VERSION;
    submit.display_id = acquire.display_id;
    submit.mode = acquire.mode;
    submit.image_token = acquire.image_token;
    submit.output_generation = acquire.output_generation;
    submit.device_generation = acquire.device_generation;
    submit.frame_id = acquire.frame_id;
    submit.flags = RIN_GPU_PRESENTATION_SUBMIT_FULL_DAMAGE;
    submit.damage_count = 0u;
    result = rin_gpu_dxgi_swapchain_present(&swapchain->core, &submit,
                                            &presentation_fence);
    if (result == RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST)
        return DXGI_ERROR_DEVICE_REMOVED;
    if (result != RIN_GPU_DXGI_SWAPCHAIN_OK) return DXGI_ERROR_INVALID_CALL;
    memset(&completion, 0, sizeof(completion));
    completion.struct_size = sizeof(completion);
    completion.version = RIN_GPU_PRESENTATION_VERSION;
    completion.display_id = submit.display_id;
    completion.status = RIN_GPU_PRESENTATION_COMPLETION_SUCCESS;
    completion.image_token = submit.image_token;
    completion.output_generation = submit.output_generation;
    completion.device_generation = submit.device_generation;
    completion.frame_id = submit.frame_id;
    completion.fence_value = presentation_fence;
    result = rin_gpu_dxgi_swapchain_complete(&swapchain->core, &completion);
    if (result != RIN_GPU_DXGI_SWAPCHAIN_OK)
        return result == RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST
                   ? DXGI_ERROR_DEVICE_REMOVED
                   : DXGI_ERROR_INVALID_CALL;
    swapchain->present_count++;
    return S_OK;
}
static HRESULT WINAPI native_swapchain_get_buffer(
    IDXGISwapChain* self, UINT index, REFIID iid, void** out) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    if (!out) return E_POINTER;
    *out = NULL;
    if (index >= swapchain->buffer_count ||
        (!IsEqualIID(iid, &IID_IUnknown) &&
         !IsEqualIID(iid, &IID_ID3D11Resource) &&
         !IsEqualIID(iid, &IID_ID3D11Texture2D))) return E_INVALIDARG;
    *out = &swapchain->buffers[index]->iface;
    ID3D11Buffer_AddRef(&swapchain->buffers[index]->iface);
    return S_OK;
}
static HRESULT WINAPI native_swapchain_set_fullscreen_state(
    IDXGISwapChain* self, BOOL fullscreen, IDXGIOutput* target) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    RinGpuPresentationOutputV1 output;
    (void)target;
    memset(&output, 0, sizeof(output));
    output.struct_size = sizeof(output);
    output.version = RIN_GPU_PRESENTATION_VERSION;
    output.display_id = RIN_GPU_PRIMARY_DISPLAY;
    output.flags = RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    output.width = swapchain->desc.BufferDesc.Width;
    output.height = swapchain->desc.BufferDesc.Height;
    output.refresh_millihertz = 60000u;
    output.format = native_dxgi_image_format(swapchain->desc.BufferDesc.Format);
    output.output_generation = 1u;
    output.device_generation = 1u;
    return core_result(rin_gpu_dxgi_swapchain_set_fullscreen(
        &swapchain->core, fullscreen ? 1u : 0u, &output));
}
static HRESULT WINAPI native_swapchain_get_fullscreen_state(
    IDXGISwapChain* self, BOOL* fullscreen, IDXGIOutput** target) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    if (!fullscreen) return E_POINTER;
    *fullscreen = swapchain->desc.Windowed ? FALSE : TRUE;
    if (target) *target = NULL;
    return S_OK;
}
static HRESULT WINAPI native_swapchain_get_desc(
    IDXGISwapChain* self, DXGI_SWAP_CHAIN_DESC* desc) {
    if (!desc) return E_POINTER;
    *desc = native_swapchain_from_interface(self)->desc;
    return S_OK;
}
static HRESULT WINAPI native_swapchain_resize_buffers(
    IDXGISwapChain* self, UINT count, UINT width, UINT height,
    DXGI_FORMAT format, UINT flags) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    RinGpuPresentationOutputV1 output;
    uint32_t old_count = swapchain->buffer_count;
    HRESULT result;
    if (count == 0u) count = swapchain->desc.BufferCount;
    if (count < RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS ||
        count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS || flags != 0u)
        return E_INVALIDARG;
    if (width == 0u || height == 0u) {
        RECT client;
        if (!GetClientRect(swapchain->window, &client)) return E_INVALIDARG;
        width = (UINT)(client.right - client.left);
        height = (UINT)(client.bottom - client.top);
    }
    if (format == DXGI_FORMAT_UNKNOWN) format = swapchain->desc.BufferDesc.Format;
    if (native_dxgi_image_format(format) == 0u) return E_INVALIDARG;
    native_swapchain_destroy_buffers(swapchain);
    memset(&output, 0, sizeof(output));
    output.struct_size = sizeof(output);
    output.version = RIN_GPU_PRESENTATION_VERSION;
    output.display_id = RIN_GPU_PRIMARY_DISPLAY;
    output.flags = RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    output.width = width;
    output.height = height;
    output.refresh_millihertz = 60000u;
    output.format = native_dxgi_image_format(format);
    output.output_generation = 2u;
    output.device_generation = 1u;
    result = core_result(rin_gpu_dxgi_swapchain_resize_buffers(
        &swapchain->core, &output, count));
    if (FAILED(result)) return result;
    swapchain->desc.BufferCount = count;
    swapchain->desc.BufferDesc.Width = width;
    swapchain->desc.BufferDesc.Height = height;
    swapchain->desc.BufferDesc.Format = format;
    swapchain->buffer_count = count;
    result = native_swapchain_create_buffers(swapchain);
    if (FAILED(result)) {
        swapchain->buffer_count = old_count;
        return result;
    }
    return S_OK;
}
static HRESULT WINAPI native_swapchain_resize_target(
    IDXGISwapChain* self, const DXGI_MODE_DESC* mode) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    if (!mode || mode->Width == 0u || mode->Height == 0u ||
        native_dxgi_image_format(mode->Format) == 0u)
        return E_INVALIDARG;
    swapchain->desc.BufferDesc = *mode;
    return S_OK;
}
static HRESULT WINAPI native_swapchain_get_containing_output(
    IDXGISwapChain* self, IDXGIOutput** out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = NULL;
    return DXGI_ERROR_NOT_FOUND;
}
static HRESULT WINAPI native_swapchain_get_frame_statistics(
    IDXGISwapChain* self, DXGI_FRAME_STATISTICS* stats) {
    NativeD3d11Swapchain* swapchain = native_swapchain_from_interface(self);
    if (!stats) return E_POINTER;
    memset(stats, 0, sizeof(*stats));
    stats->PresentCount = swapchain->present_count;
    stats->SyncRefreshCount = swapchain->present_count;
    return S_OK;
}
static HRESULT WINAPI native_swapchain_get_last_present_count(
    IDXGISwapChain* self, UINT* count) {
    if (!count) return E_POINTER;
    *count = native_swapchain_from_interface(self)->present_count;
    return S_OK;
}

/* Internal DXGI bridge used by the native factory.  The factory owns only
 * the COM shell; the swap-chain storage and presentation state remain owned
 * by this D3D11 implementation and retain the same lifetime rules as
 * D3D11CreateDeviceAndSwapChain. */
HRESULT rindx_native_d3d11_create_swapchain_for_device(
    ID3D11Device* device_interface, const DXGI_SWAP_CHAIN_DESC* swap_chain_desc,
    IDXGISwapChain** swap_chain_out) {
    NativeD3d11Device* device = device_interface
        ? native_device_from_interface(device_interface) : NULL;
    NativeD3d11Swapchain* swapchain;
    RinGpuDxgiSwapchainDescV1 core_desc;
    RinGpuDxgiWindowOwnerV1 window_owner;
    RinGpuPresentationBackendV1 backend;
    RinGpuPresentationOutputV1 output;
    HRESULT result;
    UINT buffer_count;
    uint32_t format;
    if (!swap_chain_out) return E_POINTER;
    *swap_chain_out = NULL;
    if (!device || !swap_chain_desc || !swap_chain_desc->OutputWindow ||
        !IsWindow(swap_chain_desc->OutputWindow)) return E_INVALIDARG;
    format = native_dxgi_image_format(swap_chain_desc->BufferDesc.Format);
    if (format == 0u || swap_chain_desc->BufferDesc.Width == 0u ||
        swap_chain_desc->BufferDesc.Height == 0u ||
        (swap_chain_desc->SwapEffect != DXGI_SWAP_EFFECT_DISCARD &&
         swap_chain_desc->SwapEffect != DXGI_SWAP_EFFECT_SEQUENTIAL))
        return E_INVALIDARG;
    buffer_count = swap_chain_desc->BufferCount == 0u
        ? RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS : swap_chain_desc->BufferCount;
    if (buffer_count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS) return E_INVALIDARG;
    swapchain = (NativeD3d11Swapchain*)calloc(1u, sizeof(*swapchain));
    if (!swapchain) return E_OUTOFMEMORY;
    swapchain->iface.lpVtbl = (IDXGISwapChainVtbl*)(void*)&native_swapchain_vtable;
    swapchain->references = 1;
    swapchain->device = device;
    swapchain->window = swap_chain_desc->OutputWindow;
    swapchain->desc = *swap_chain_desc;
    swapchain->desc.BufferCount = buffer_count;
    swapchain->buffer_count = buffer_count;
    memset(&output, 0, sizeof(output));
    output.struct_size = sizeof(output);
    output.version = RIN_GPU_PRESENTATION_VERSION;
    output.display_id = RIN_GPU_PRIMARY_DISPLAY;
    output.flags = (swap_chain_desc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
        ? RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE
        : RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    output.width = swap_chain_desc->BufferDesc.Width;
    output.height = swap_chain_desc->BufferDesc.Height;
    output.refresh_millihertz = swap_chain_desc->BufferDesc.RefreshRate.Denominator
        ? (UINT)(1000u * swap_chain_desc->BufferDesc.RefreshRate.Numerator /
                 swap_chain_desc->BufferDesc.RefreshRate.Denominator)
        : 60000u;
    output.format = format;
    output.output_generation = 1u;
    output.device_generation = 1u;
    memset(&core_desc, 0, sizeof(core_desc));
    core_desc.struct_size = sizeof(core_desc);
    core_desc.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    core_desc.buffer_count = buffer_count;
    core_desc.flags = (swap_chain_desc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
        ? RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING : 0u;
    core_desc.output = output;
    memset(&window_owner, 0, sizeof(window_owner));
    window_owner.struct_size = sizeof(window_owner);
    window_owner.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    window_owner.native_window = swap_chain_desc->OutputWindow;
    window_owner.retain = native_swapchain_window_retain;
    window_owner.release = native_swapchain_window_release;
    window_owner.validate = native_swapchain_window_validate;
    memset(&backend, 0, sizeof(backend));
    backend.struct_size = sizeof(backend);
    backend.version = RIN_GPU_PRESENTATION_VERSION;
    backend.context = swapchain;
    backend.submit = native_swapchain_backend_submit;
    backend.cancel = native_swapchain_backend_cancel;
    result = core_result(rin_gpu_dxgi_swapchain_runtime_init(
        &swapchain->core, &core_desc, &window_owner, &backend));
    if (FAILED(result)) { free(swapchain); return result; }
    result = native_swapchain_create_buffers(swapchain);
    if (FAILED(result)) {
        native_swapchain_destroy_buffers(swapchain);
        (void)rin_gpu_dxgi_swapchain_runtime_shutdown(&swapchain->core);
        free(swapchain);
        return result;
    }
    ID3D11Device_AddRef(device_interface);
    *swap_chain_out = &swapchain->iface;
    return S_OK;
}

#define RINDX_NATIVE_EXPORT

RINDX_NATIVE_EXPORT HRESULT WINAPI D3D11CreateDevice(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type, HMODULE software,
    UINT flags, const D3D_FEATURE_LEVEL* feature_levels, UINT feature_count,
    UINT sdk_version, ID3D11Device** device_out,
    D3D_FEATURE_LEVEL* feature_level_out, ID3D11DeviceContext** context_out) {
    RinGpuRuntimeDescV1 surface;
    const uint32_t level = RIN_DX_D3D11_FEATURE_LEVEL_11_0;
    NativeD3d11Device* device;
    int result;
    (void)adapter;
    (void)software;
    if (device_out) *device_out = NULL;
    if (context_out) *context_out = NULL;
    if (sdk_version != D3D11_SDK_VERSION || driver_type == D3D_DRIVER_TYPE_SOFTWARE ||
        !device_out) return E_INVALIDARG;
    if (feature_count != 0u && (!feature_levels ||
        feature_levels[0] != D3D_FEATURE_LEVEL_11_0)) return E_INVALIDARG;
    native_surface_init(&surface);
    device = (NativeD3d11Device*)calloc(1u, sizeof(*device));
    if (!device) return E_OUTOFMEMORY;
    result = rindx_d3d11_create_device(&surface, &level, 1u, &device->core);
    if (result != RIN_GPU_OK) { free(device); return core_result(result); }
    device->iface.lpVtbl = (ID3D11DeviceVtbl*)(void*)&native_device_vtable_bound;
    device->references = 1;
    device->creation_flags = flags;
    device->immediate = (NativeD3d11Context*)calloc(1u, sizeof(*device->immediate));
    if (!device->immediate) {
        (void)rindx_d3d11_destroy_device(&device->core);
        free(device);
        return E_OUTOFMEMORY;
    }
    /* Recreate the context into the allocated object; the probe above only
     * validated that the software owner can create an immediate context. */
    memset(&device->immediate->core, 0, sizeof(device->immediate->core));
    result = rindx_d3d11_create_context(&device->core, &device->immediate->core);
    if (result != RIN_GPU_OK) {
        free(device->immediate);
        (void)rindx_d3d11_destroy_device(&device->core);
        free(device);
        return core_result(result);
    }
    device->immediate->iface.lpVtbl =
        (ID3D11DeviceContextVtbl*)(void*)&native_context_vtable_bound;
    device->immediate->references = 1;
    device->immediate->device = device;
    device->immediate->topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    if (feature_level_out) *feature_level_out = D3D_FEATURE_LEVEL_11_0;
    if (context_out) {
        *context_out = &device->immediate->iface;
        ID3D11DeviceContext_AddRef(*context_out);
    }
    *device_out = &device->iface;
    return S_OK;
}

RINDX_NATIVE_EXPORT HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type, HMODULE software,
    UINT flags, const D3D_FEATURE_LEVEL* feature_levels, UINT feature_count,
    UINT sdk_version, const DXGI_SWAP_CHAIN_DESC* swap_chain_desc,
    IDXGISwapChain** swap_chain_out, ID3D11Device** device_out,
    D3D_FEATURE_LEVEL* feature_level_out,
    ID3D11DeviceContext** context_out) {
    ID3D11Device* device = NULL;
    ID3D11DeviceContext* context = NULL;
    NativeD3d11Swapchain* swapchain;
    RinGpuDxgiSwapchainDescV1 core_desc;
    RinGpuDxgiWindowOwnerV1 window_owner;
    RinGpuPresentationBackendV1 backend;
    RinGpuPresentationOutputV1 output;
    HRESULT result;
    UINT buffer_count;
    uint32_t format;
    if (swap_chain_out) *swap_chain_out = NULL;
    if (device_out) *device_out = NULL;
    if (context_out) *context_out = NULL;
    if (!swap_chain_desc || !swap_chain_out || !device_out ||
        !swap_chain_desc->OutputWindow ||
        !IsWindow(swap_chain_desc->OutputWindow)) return E_INVALIDARG;
    format = native_dxgi_image_format(swap_chain_desc->BufferDesc.Format);
    if (format == 0u || swap_chain_desc->BufferDesc.Width == 0u ||
        swap_chain_desc->BufferDesc.Height == 0u ||
        (swap_chain_desc->SwapEffect != DXGI_SWAP_EFFECT_DISCARD &&
         swap_chain_desc->SwapEffect != DXGI_SWAP_EFFECT_SEQUENTIAL))
        return E_INVALIDARG;
    result = D3D11CreateDevice(adapter, driver_type, software, flags,
                               feature_levels, feature_count, sdk_version,
                               &device, feature_level_out, &context);
    if (FAILED(result)) return result;
    swapchain = (NativeD3d11Swapchain*)calloc(1u, sizeof(*swapchain));
    if (!swapchain) {
        ID3D11DeviceContext_Release(context);
        ID3D11Device_Release(device);
        return E_OUTOFMEMORY;
    }
    swapchain->iface.lpVtbl = (IDXGISwapChainVtbl*)(void*)&native_swapchain_vtable;
    swapchain->references = 1;
    swapchain->device = native_device_from_interface(device);
    swapchain->window = swap_chain_desc->OutputWindow;
    swapchain->desc = *swap_chain_desc;
    buffer_count = swap_chain_desc->BufferCount == 0u
                       ? RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS
                       : swap_chain_desc->BufferCount;
    if (buffer_count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS) {
        free(swapchain);
        ID3D11DeviceContext_Release(context);
        ID3D11Device_Release(device);
        return E_INVALIDARG;
    }
    swapchain->desc.BufferCount = buffer_count;
    swapchain->buffer_count = buffer_count;
    swapchain->desc.BufferDesc.Format = swap_chain_desc->BufferDesc.Format;
    memset(&output, 0, sizeof(output));
    output.struct_size = sizeof(output);
    output.version = RIN_GPU_PRESENTATION_VERSION;
    output.display_id = RIN_GPU_PRIMARY_DISPLAY;
    output.flags = (swap_chain_desc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
                       ? RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE
                       : RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    output.width = swap_chain_desc->BufferDesc.Width;
    output.height = swap_chain_desc->BufferDesc.Height;
    output.refresh_millihertz = swap_chain_desc->BufferDesc.RefreshRate.Denominator
                                    ? (UINT)(1000u * swap_chain_desc->BufferDesc.RefreshRate.Numerator /
                                             swap_chain_desc->BufferDesc.RefreshRate.Denominator)
                                    : 60000u;
    output.format = format;
    output.output_generation = 1u;
    output.device_generation = 1u;
    memset(&core_desc, 0, sizeof(core_desc));
    core_desc.struct_size = sizeof(core_desc);
    core_desc.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    core_desc.buffer_count = buffer_count;
    core_desc.flags = (swap_chain_desc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
                          ? RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING
                          : 0u;
    core_desc.output = output;
    memset(&window_owner, 0, sizeof(window_owner));
    window_owner.struct_size = sizeof(window_owner);
    window_owner.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    window_owner.native_window = swap_chain_desc->OutputWindow;
    window_owner.retain = native_swapchain_window_retain;
    window_owner.release = native_swapchain_window_release;
    window_owner.validate = native_swapchain_window_validate;
    memset(&backend, 0, sizeof(backend));
    backend.struct_size = sizeof(backend);
    backend.version = RIN_GPU_PRESENTATION_VERSION;
    backend.context = swapchain;
    backend.submit = native_swapchain_backend_submit;
    backend.cancel = native_swapchain_backend_cancel;
    result = core_result(rin_gpu_dxgi_swapchain_runtime_init(
        &swapchain->core, &core_desc, &window_owner, &backend));
    if (FAILED(result)) {
        free(swapchain);
        ID3D11DeviceContext_Release(context);
        ID3D11Device_Release(device);
        return result;
    }
    result = native_swapchain_create_buffers(swapchain);
    if (FAILED(result)) {
        native_swapchain_destroy_buffers(swapchain);
        (void)rin_gpu_dxgi_swapchain_runtime_shutdown(&swapchain->core);
        free(swapchain);
        ID3D11DeviceContext_Release(context);
        ID3D11Device_Release(device);
        return result;
    }
    ID3D11Device_AddRef(device);
    *swap_chain_out = &swapchain->iface;
    *device_out = device;
    if (context_out) *context_out = context;
    else ID3D11DeviceContext_Release(context);
    return S_OK;
}

#endif /* _WIN32 */
