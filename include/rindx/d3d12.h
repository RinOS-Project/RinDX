/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_D3D12_H
#define RINDX_PUBLIC_D3D12_H

#include <ringpu/runtime.h>
#include <rindx/com.h>
#include <rindx/swapchain.h>

#include <stdint.h>

#define RIN_DX_D3D12_VERSION 1u
#define RIN_DX_D3D12_FEATURE_LEVEL_12_0 UINT32_C(0xc000)
#define RIN_DX_D3D12_FEATURE_GRAPHICS_COMMANDS 1u
#define RIN_DX_D3D12_FEATURE_COMPUTE_COMMANDS 2u
#define RIN_DX_D3D12_FEATURE_DESCRIPTOR_TABLE 3u
#define RIN_DX_D3D12_FEATURE_RSH1_PIPELINE 4u
#define RIN_DX_D3D12_PIPELINE_CACHE_POLICY_CREATE_ONLY 1u
#define RIN_DX_D3D12_FEATURE_KNOWN \
    (RIN_DX_D3D12_FEATURE_GRAPHICS_COMMANDS | \
     RIN_DX_D3D12_FEATURE_COMPUTE_COMMANDS | \
     RIN_DX_D3D12_FEATURE_DESCRIPTOR_TABLE | \
     RIN_DX_D3D12_FEATURE_RSH1_PIPELINE)
#define RIN_DX_D3D12_MAP_READ 1u
#define RIN_DX_D3D12_MAP_WRITE 2u
#define RIN_DX_D3D12_MAP_READ_WRITE 3u
#define RIN_DX_D3D12_MAP_KNOWN \
    (RIN_DX_D3D12_MAP_READ | RIN_DX_D3D12_MAP_WRITE | \
     RIN_DX_D3D12_MAP_READ_WRITE)

typedef struct RinDxD3d12Device {
    uint32_t struct_size;
    uint32_t version;
    RinGpuRuntime* runtime;
    RinGpuHandle queue;
    RinGpuHandle fence;
    uint64_t submission_value;
    uint32_t feature_level;
    uint32_t state;
    RinGpuHandle mapped_buffer;
    void* mapped_data;
    uint64_t mapped_size;
    uint32_t mapped_type;
    uint32_t mapped_flags;
} RinDxD3d12Device;

typedef struct RinDxD3d12MappedResource {
    uint32_t struct_size;
    uint32_t version;
    RinGpuHandle buffer;
    void* data;
    uint64_t size_bytes;
    uint64_t row_pitch_bytes;
    uint64_t depth_pitch_bytes;
    uint32_t map_type;
    uint32_t flags;
} RinDxD3d12MappedResource;

typedef struct RinDxD3d12CommandAllocator {
    uint32_t struct_size;
    uint32_t version;
    RinDxD3d12Device* device;
    uint32_t recording_lists;
    uint32_t state;
} RinDxD3d12CommandAllocator;

typedef struct RinDxD3d12CommandList {
    uint32_t struct_size;
    uint32_t version;
    RinDxD3d12Device* device;
    RinDxD3d12CommandAllocator* allocator;
    RinGpuHandle command_list;
    RinGpuHandle active_color_target;
    RinGpuHandle active_depth_target;
    uint32_t render_pass_active;
    uint32_t state;
} RinDxD3d12CommandList;

/* Explicit host software D3D12 contract. The queue, allocator/list, resource
 * state transition, descriptor bind group, pipeline, draw/dispatch, fence and
 * readback calls all lower to RinGPU operations. Native D3D12 COM/DLL and
 * DXIL/root-signature translation must validate and lower into this owner. */
int rindx_d3d12_create_device(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    RinDxD3d12Device* device_out);
/* Bounded host CreateDeviceAndSwapChain sequencing. The swapchain remains the
 * existing versioned RinDX presentation owner; native HWND/COM back-buffer
 * identity is not inferred from this entry point. */
int rindx_d3d12_create_device_and_swapchain(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    const RinGpuDxgiSwapchainDescV1* swapchain_desc,
    const RinGpuDxgiWindowOwnerV1* window_owner,
    const RinGpuPresentationBackendV1* presentation_backend,
    RinDxD3d12Device* device_out,
    RinGpuDxgiSwapchainRuntime* swapchain_out);
int rindx_d3d12_destroy_device(RinDxD3d12Device* device);
/* Bounded adapter/feature query sourced from the validated RinGPU adapter
 * descriptor. Native node enumeration remains a separate boundary. */
int rindx_d3d12_get_adapter_info(const RinDxD3d12Device* device,
                                 RinGpuAdapterInfoV1* info);
/* Bounded host feature query. The result is derived only from the validated
 * adapter queue mask and this owner's RSH1/typed-table contract; unknown
 * native feature identifiers are rejected instead of reported supported. */
int rindx_d3d12_check_feature_support(const RinDxD3d12Device* device,
                                      uint32_t feature,
                                      uint32_t* supported_out);
/* The bounded RSH1 owner has no serialized native blob format. It exposes a
 * create-only policy so callers cannot mistake an unvalidated cache/library
 * blob for a hit; native ID3D12PipelineLibrary is a separate ABI boundary. */
int rindx_d3d12_get_pipeline_cache_policy(
    const RinDxD3d12Device* device, uint32_t* policy_out);
/* Bounded device-removal propagation from the RinGPU runtime. Native DXGI
 * HRESULT translation and physical reset are separate boundaries. */
int rindx_d3d12_get_device_removed_reason(
    const RinDxD3d12Device* device);
RinDxgiHresult rindx_d3d12_get_device_removed_reason_hresult(
    const RinDxD3d12Device* device);
int rindx_d3d12_mark_device_removed(RinDxD3d12Device* device);
int rindx_d3d12_create_command_allocator(
    RinDxD3d12Device* device, RinDxD3d12CommandAllocator* allocator_out);
int rindx_d3d12_reset_command_allocator(RinDxD3d12CommandAllocator* allocator);
int rindx_d3d12_destroy_command_allocator(
    RinDxD3d12CommandAllocator* allocator);
int rindx_d3d12_create_command_list(
    RinDxD3d12Device* device, RinDxD3d12CommandAllocator* allocator,
    RinDxD3d12CommandList* list_out);
int rindx_d3d12_reset_command_list(RinDxD3d12CommandList* list);
int rindx_d3d12_close_command_list(RinDxD3d12CommandList* list);
int rindx_d3d12_destroy_command_list(RinDxD3d12CommandList* list);

int rindx_d3d12_create_buffer(RinDxD3d12Device* device,
                              const RinGpuBufferDescV1* descriptor,
                              RinGpuHandle* buffer_out);
int rindx_d3d12_create_texture2d(RinDxD3d12Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out);
int rindx_d3d12_create_shader(RinDxD3d12Device* device, const void* rin_shader,
                              uint64_t shader_size,
                              RinGpuHandle* shader_out);
int rindx_d3d12_create_graphics_pipeline(
    RinDxD3d12Device* device, const RinGpuGraphicsPipelineNativeDescV2* desc,
    const RinGpuVertexAttributeV2* attributes, uint32_t attribute_count,
    const RinGpuVertexBufferLayoutV1* vertex_bindings,
    uint32_t vertex_binding_count, const RinGpuVaryingV1* varyings,
    uint32_t varying_count, RinGpuHandle* pipeline_out);
int rindx_d3d12_create_compute_pipeline(
    RinDxD3d12Device* device, RinGpuHandle shader,
    RinGpuHandle* pipeline_out);
int rindx_d3d12_create_sampler(
    RinDxD3d12Device* device, const RinGpuSamplerDescV1* descriptor,
    RinGpuHandle* sampler_out);
int rindx_d3d12_create_compute_bind_group(
    RinDxD3d12Device* device, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out);
int rindx_d3d12_create_descriptor_heap(
    RinDxD3d12Device* device, RinGpuHandle pipeline,
    const RinGpuGraphicsBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* heap_out);
int rindx_d3d12_destroy_object(RinDxD3d12Device* device, RinGpuHandle object);
int rindx_d3d12_upload_buffer(RinDxD3d12Device* device, RinGpuHandle buffer,
                             uint64_t offset, const void* source,
                             uint64_t size_bytes);
int rindx_d3d12_upload_image(RinDxD3d12Device* device, RinGpuHandle image,
                             const RinGpuImageUploadV1* upload,
                             const void* source, uint64_t source_size);
int rindx_d3d12_readback_buffer(
    RinDxD3d12Device* device, RinGpuHandle buffer, uint64_t source_offset,
    void* destination, uint64_t size_bytes);
int rindx_d3d12_map_buffer(RinDxD3d12Device* device, RinGpuHandle buffer,
                           uint32_t map_type, uint32_t flags,
                           RinDxD3d12MappedResource* mapped_out);
int rindx_d3d12_unmap_buffer(RinDxD3d12Device* device, RinGpuHandle buffer,
                             RinDxD3d12MappedResource* mapped);
/* Bounded software copy/resolve/clear owner. Explicit RinGPU regions are the
 * validated ABI; arbitrary native D3D12 command-list bytecode is rejected. */
int rindx_d3d12_copy_buffer(RinDxD3d12CommandList* list,
                            RinGpuHandle destination, uint64_t destination_offset,
                            RinGpuHandle source, uint64_t source_offset,
                            uint64_t size_bytes);
int rindx_d3d12_clear_buffer(RinDxD3d12CommandList* list,
                             RinGpuHandle destination,
                             const RinGpuBufferClearV1* clear);
int rindx_d3d12_copy_texture2d(
    RinDxD3d12CommandList* list, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageCopyRegionV1* region);
int rindx_d3d12_resolve_texture2d(
    RinDxD3d12CommandList* list, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageResolveV1* resolve);
int rindx_d3d12_clear_render_target(
    RinDxD3d12CommandList* list, RinGpuHandle target,
    float red, float green, float blue, float alpha);
int rindx_d3d12_clear_depth_stencil(
    RinDxD3d12CommandList* list, RinGpuHandle target, uint32_t clear_flags,
    float depth, uint32_t stencil);

int rindx_d3d12_transition_image(
    RinDxD3d12CommandList* list, RinGpuHandle image,
    uint32_t before_state, uint32_t after_state);
int rindx_d3d12_begin_render_pass(
    RinDxD3d12CommandList* list, RinGpuHandle color_target,
    RinGpuHandle depth_target, uint32_t clear_color,
    float clear_red, float clear_green, float clear_blue, float clear_alpha,
    float clear_depth);
int rindx_d3d12_set_raster_state(RinDxD3d12CommandList* list,
                                 const RinGpuRasterStateV1* state);
int rindx_d3d12_bind_descriptor_heap(RinDxD3d12CommandList* list,
                                     RinGpuHandle heap);
int rindx_d3d12_draw(RinDxD3d12CommandList* list,
                     const RinGpuDrawV1* draw);
int rindx_d3d12_draw_indexed_instanced(
    RinDxD3d12CommandList* list, const RinGpuDrawIndexedV2* draw);
/* Bounded ExecuteIndirect owners.  The command signature is fixed by the
 * RinGPU indirect packet ABI; arbitrary D3D12 command-signature bytecode
 * remains a separate COM/DXIL boundary and is rejected by this owner. */
int rindx_d3d12_execute_indirect_draw(
    RinDxD3d12CommandList* list, const RinGpuDrawIndirectV1* draw);
int rindx_d3d12_execute_indirect_draw_indexed(
    RinDxD3d12CommandList* list, const RinGpuDrawIndexedIndirectV1* draw);
int rindx_d3d12_dispatch(RinDxD3d12CommandList* list,
                         const RinGpuDispatchV1* dispatch);
int rindx_d3d12_execute_indirect_dispatch(
    RinDxD3d12CommandList* list, const RinGpuDispatchIndirectV1* dispatch);
int rindx_d3d12_end_render_pass(RinDxD3d12CommandList* list);
/* Bounded software presentation owner. The image must satisfy RinGPU's
 * present usage/state/display contract; native DXGI swap-chain Present and
 * ResizeBuffers translation remains a separate COM boundary. */
int rindx_d3d12_present(RinDxD3d12CommandList* list, RinGpuHandle image,
                        uint32_t display_id);
int rindx_d3d12_execute_command_lists(RinDxD3d12Device* device,
                                      RinDxD3d12CommandList* list,
                                      uint64_t* fence_value_out);
int rindx_d3d12_wait(RinDxD3d12Device* device, uint64_t fence_value,
                     uint64_t timeout_ns);
int rindx_d3d12_readback_image(
    RinDxD3d12Device* device, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size);

#if defined(__cplusplus)
static_assert(sizeof(RinDxD3d12Device) == 80u,
              "RinDX D3D12 device ABI drift");
static_assert(sizeof(RinDxD3d12MappedResource) == 56u,
              "RinDX D3D12 mapped-resource ABI drift");
static_assert(sizeof(RinDxD3d12CommandAllocator) == 24u,
              "RinDX D3D12 allocator ABI drift");
static_assert(sizeof(RinDxD3d12CommandList) == 56u,
              "RinDX D3D12 command list ABI drift");
#else
_Static_assert(sizeof(RinDxD3d12Device) == 80u,
               "RinDX D3D12 device ABI drift");
_Static_assert(sizeof(RinDxD3d12MappedResource) == 56u,
               "RinDX D3D12 mapped-resource ABI drift");
_Static_assert(sizeof(RinDxD3d12CommandAllocator) == 24u,
               "RinDX D3D12 allocator ABI drift");
_Static_assert(sizeof(RinDxD3d12CommandList) == 56u,
               "RinDX D3D12 command list ABI drift");
#endif

#endif /* RINDX_PUBLIC_D3D12_H */
