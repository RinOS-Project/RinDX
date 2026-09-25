/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_D3D11_H
#define RINDX_PUBLIC_D3D11_H

#include <ringpu/runtime.h>

#include <stdint.h>

#define RIN_DX_D3D11_VERSION 1u
#define RIN_DX_D3D11_FEATURE_LEVEL_11_0 UINT32_C(0xb000)
#define RIN_DX_D3D11_MAP_READ 1u
#define RIN_DX_D3D11_MAP_WRITE 2u
#define RIN_DX_D3D11_MAP_READ_WRITE 3u
#define RIN_DX_D3D11_MAP_WRITE_DISCARD 4u
#define RIN_DX_D3D11_MAP_WRITE_NO_OVERWRITE 5u
#define RIN_DX_D3D11_MAP_KNOWN \
    (RIN_DX_D3D11_MAP_READ | RIN_DX_D3D11_MAP_WRITE | \
     RIN_DX_D3D11_MAP_READ_WRITE | RIN_DX_D3D11_MAP_WRITE_DISCARD | \
     RIN_DX_D3D11_MAP_WRITE_NO_OVERWRITE)

typedef struct RinDxD3d11Device {
    uint32_t struct_size;
    uint32_t version;
    RinGpuRuntime* runtime;
    RinGpuHandle queue;
    RinGpuHandle fence;
    uint64_t submission_value;
    uint32_t feature_level;
    uint32_t state;
} RinDxD3d11Device;

typedef struct RinDxD3d11Context {
    uint32_t struct_size;
    uint32_t version;
    RinDxD3d11Device* device;
    RinGpuHandle command_list;
    RinGpuHandle active_color_target;
    RinGpuHandle active_depth_target;
    RinGpuHandle pending_command_list;
    uint32_t render_pass_active;
    uint32_t state;
    RinGpuHandle mapped_buffer;
    void* mapped_data;
    uint64_t mapped_size;
    uint32_t mapped_type;
    uint32_t mapped_flags;
} RinDxD3d11Context;

typedef struct RinDxD3d11MappedResource {
    uint32_t struct_size;
    uint32_t version;
    RinGpuHandle buffer;
    void* data;
    uint64_t size_bytes;
    uint64_t row_pitch_bytes;
    uint64_t depth_pitch_bytes;
    uint32_t map_type;
    uint32_t flags;
} RinDxD3d11MappedResource;

/* This is the RinOS D3D11 software execution contract. It deliberately uses
 * RinGPU's versioned resources and RSH1 modules rather than accepting an
 * unchecked DXBC/DXIL pointer. A Windows COM/DLL adapter must validate its
 * native ABI and lower into this contract before calling it. */
int rindx_d3d11_create_device(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    RinDxD3d11Device* device_out);
int rindx_d3d11_destroy_device(RinDxD3d11Device* device);
int rindx_d3d11_create_context(RinDxD3d11Device* device,
                               RinDxD3d11Context* context_out);
int rindx_d3d11_destroy_context(RinDxD3d11Context* context);

int rindx_d3d11_create_buffer(RinDxD3d11Device* device,
                              const RinGpuBufferDescV1* descriptor,
                              RinGpuHandle* buffer_out);
int rindx_d3d11_create_texture2d(RinDxD3d11Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out);
int rindx_d3d11_create_shader(RinDxD3d11Device* device, const void* rin_shader,
                              uint64_t shader_size,
                              RinGpuHandle* shader_out);
int rindx_d3d11_create_graphics_pipeline(
    RinDxD3d11Device* device, const RinGpuGraphicsPipelineNativeDescV2* desc,
    const RinGpuVertexAttributeV2* attributes, uint32_t attribute_count,
    const RinGpuVertexBufferLayoutV1* vertex_bindings,
    uint32_t vertex_binding_count, const RinGpuVaryingV1* varyings,
    uint32_t varying_count, RinGpuHandle* pipeline_out);
int rindx_d3d11_create_compute_pipeline(
    RinDxD3d11Device* device, RinGpuHandle shader,
    RinGpuHandle* pipeline_out);
int rindx_d3d11_create_compute_bind_group(
    RinDxD3d11Device* device, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out);
int rindx_d3d11_destroy_object(RinDxD3d11Device* device, RinGpuHandle object);
int rindx_d3d11_upload_buffer(RinDxD3d11Device* device, RinGpuHandle buffer,
                             uint64_t offset, const void* source,
                             uint64_t size_bytes);
int rindx_d3d11_upload_image(RinDxD3d11Device* device, RinGpuHandle image,
                             const RinGpuImageUploadV1* upload,
                             const void* source, uint64_t source_size);
int rindx_d3d11_update_subresource_buffer(
    RinDxD3d11Device* device, RinGpuHandle buffer, uint64_t offset,
    const void* source, uint64_t size_bytes);
int rindx_d3d11_update_subresource_texture2d(
    RinDxD3d11Device* device, RinGpuHandle image,
    const RinGpuImageUploadV1* upload, const void* source,
    uint64_t source_size);
int rindx_d3d11_readback_buffer(
    RinDxD3d11Device* device, RinGpuHandle buffer, uint64_t source_offset,
    void* destination, uint64_t size_bytes);
int rindx_d3d11_map_buffer(RinDxD3d11Context* context, RinGpuHandle buffer,
                           uint32_t map_type, uint32_t flags,
                           RinDxD3d11MappedResource* mapped_out);
int rindx_d3d11_unmap_buffer(RinDxD3d11Context* context, RinGpuHandle buffer,
                             RinDxD3d11MappedResource* mapped);
/* Bounded software resource-transfer owner. Native D3D11 COM/view
 * translation remains outside this ABI and is rejected until validated. */
int rindx_d3d11_copy_buffer(RinDxD3d11Context* context,
                            RinGpuHandle destination, uint64_t destination_offset,
                            RinGpuHandle source, uint64_t source_offset,
                            uint64_t size_bytes);
int rindx_d3d11_clear_buffer(RinDxD3d11Context* context,
                             RinGpuHandle destination,
                             const RinGpuBufferClearV1* clear);
int rindx_d3d11_copy_resource(RinDxD3d11Context* context,
                              RinGpuHandle destination, RinGpuHandle source);
int rindx_d3d11_copy_subresource_region(
    RinDxD3d11Context* context, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageCopyRegionV1* region);
int rindx_d3d11_resolve_subresource(
    RinDxD3d11Context* context, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageResolveV1* resolve);
int rindx_d3d11_clear_render_target_view(
    RinDxD3d11Context* context, RinGpuHandle target,
    float red, float green, float blue, float alpha);
int rindx_d3d11_clear_depth_stencil_view(
    RinDxD3d11Context* context, RinGpuHandle target, uint32_t clear_flags,
    float depth, uint32_t stencil);
int rindx_d3d11_generate_mips(RinDxD3d11Context* context, RinGpuHandle image);

int rindx_d3d11_transition_image(
    RinDxD3d11Context* context, RinGpuHandle image,
    uint32_t before_state, uint32_t after_state);
int rindx_d3d11_begin_render_pass(
    RinDxD3d11Context* context, RinGpuHandle color_target,
    RinGpuHandle depth_target, uint32_t clear_color,
    float clear_red, float clear_green, float clear_blue, float clear_alpha,
    float clear_depth);
int rindx_d3d11_set_raster_state(RinDxD3d11Context* context,
                                 const RinGpuRasterStateV1* state);
int rindx_d3d11_bind_graphics_resources(RinDxD3d11Context* context,
                                        RinGpuHandle bind_group);
int rindx_d3d11_draw(RinDxD3d11Context* context, const RinGpuDrawV1* draw);
int rindx_d3d11_draw_vertices(RinDxD3d11Context* context,
                              const RinGpuDrawVerticesV2* draw);
int rindx_d3d11_draw_indexed(RinDxD3d11Context* context,
                             const RinGpuDrawIndexedV2* draw);
int rindx_d3d11_draw_indexed_instanced(
    RinDxD3d11Context* context, const RinGpuDrawIndexedV2* draw);
int rindx_d3d11_dispatch(RinDxD3d11Context* context,
                         const RinGpuDispatchV1* dispatch);
int rindx_d3d11_end_render_pass(RinDxD3d11Context* context);
int rindx_d3d11_close_and_submit(RinDxD3d11Context* context,
                                uint64_t* fence_value_out);
int rindx_d3d11_wait(RinDxD3d11Device* device, uint64_t fence_value,
                     uint64_t timeout_ns);
int rindx_d3d11_readback_image(
    RinDxD3d11Device* device, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size);

#if defined(__cplusplus)
static_assert(sizeof(RinDxD3d11Device) == 48u,
              "RinDX D3D11 device ABI drift");
static_assert(sizeof(RinDxD3d11Context) == 88u,
              "RinDX D3D11 context ABI drift");
static_assert(sizeof(RinDxD3d11MappedResource) == 56u,
              "RinDX D3D11 mapped-resource ABI drift");
#else
_Static_assert(sizeof(RinDxD3d11Device) == 48u,
               "RinDX D3D11 device ABI drift");
_Static_assert(sizeof(RinDxD3d11Context) == 88u,
               "RinDX D3D11 context ABI drift");
_Static_assert(sizeof(RinDxD3d11MappedResource) == 56u,
               "RinDX D3D11 mapped-resource ABI drift");
#endif

#endif /* RINDX_PUBLIC_D3D11_H */
