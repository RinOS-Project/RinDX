/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_D3D12_H
#define RINDX_PUBLIC_D3D12_H

#include <ringpu/runtime.h>

#include <stdint.h>

#define RIN_DX_D3D12_VERSION 1u
#define RIN_DX_D3D12_FEATURE_LEVEL_12_0 UINT32_C(0xc000)

typedef struct RinDxD3d12Device {
    uint32_t struct_size;
    uint32_t version;
    RinGpuRuntime* runtime;
    RinGpuHandle queue;
    RinGpuHandle fence;
    uint64_t submission_value;
    uint32_t feature_level;
    uint32_t state;
} RinDxD3d12Device;

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
int rindx_d3d12_destroy_device(RinDxD3d12Device* device);
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
static_assert(sizeof(RinDxD3d12Device) == 48u,
              "RinDX D3D12 device ABI drift");
static_assert(sizeof(RinDxD3d12CommandAllocator) == 24u,
              "RinDX D3D12 allocator ABI drift");
static_assert(sizeof(RinDxD3d12CommandList) == 56u,
              "RinDX D3D12 command list ABI drift");
#else
_Static_assert(sizeof(RinDxD3d12Device) == 48u,
               "RinDX D3D12 device ABI drift");
_Static_assert(sizeof(RinDxD3d12CommandAllocator) == 24u,
               "RinDX D3D12 allocator ABI drift");
_Static_assert(sizeof(RinDxD3d12CommandList) == 56u,
               "RinDX D3D12 command list ABI drift");
#endif

#endif /* RINDX_PUBLIC_D3D12_H */
