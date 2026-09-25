/* SPDX-License-Identifier: MIT */
#include <rindx/d3d12.h>

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", \
                                __FILE__, __LINE__, #condition); return 1; } \
} while (0)

typedef struct ShaderBlob {
    RinShaderHeaderV1 header;
    RinShaderInstructionV1 instructions[17];
} ShaderBlob;

static void instruction(RinShaderInstructionV1* out, uint16_t opcode,
                        uint16_t destination, uint16_t source0,
                        uint32_t immediate)
{
    memset(out, 0, sizeof(*out));
    out->opcode = opcode;
    out->destination = destination;
    out->source0 = source0;
    out->source1 = RIN_SHADER_UNUSED;
    out->resource = RIN_SHADER_UNUSED;
    out->immediate = immediate;
}

static uint32_t f32_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void make_shader(ShaderBlob* shader, uint32_t stage)
{
    static const float values[4] = {1.0f, 0.25f, 0.0f, 1.0f};
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = stage;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.input_count = stage == RIN_SHADER_STAGE_VERTEX ? 1u : 0u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    if (stage == RIN_SHADER_STAGE_VERTEX) {
        instruction(&shader->instructions[0], RIN_SHADER_OP_LOAD_INPUT_F32,
                    0u, RIN_SHADER_UNUSED, 0u);
        for (index = 1u; index < 4u; ++index)
            instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                        (uint16_t)index, RIN_SHADER_UNUSED,
                        f32_bits(index == 3u ? 1.0f : 0.0f));
    } else {
        for (index = 0u; index < 4u; ++index)
            instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                        (uint16_t)index, RIN_SHADER_UNUSED,
                        f32_bits(values[index]));
    }
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[4u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, index);
    }
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static void make_compute_shader(ShaderBlob* shader)
{
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_COMPUTE;
    shader->header.instruction_count = 1u;
    shader->header.register_count = 1u;
    shader->header.workgroup_x = 1u;
    shader->header.workgroup_y = 1u;
    shader->header.workgroup_z = 1u;
    shader->header.total_size = sizeof(shader->header) +
                                sizeof(shader->instructions[0]);
    instruction(&shader->instructions[0], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static int present(void* context, const RinGpuSoftwarePresentedImageV1* image)
{
    (void)context;
    return image && image->pixels ? RIN_GPU_OK : RIN_GPU_ERROR_INVALID_ARGUMENT;
}

static int acquire(void* context, const RinGpuImageDescV1* descriptor,
                   uint64_t allocation_bytes,
                   RinGpuSoftwareExternalImageV1* storage)
{
    (void)context;
    (void)descriptor;
    (void)allocation_bytes;
    if (!storage) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(storage, 0, sizeof(*storage));
    return RIN_GPU_OK;
}

static void make_surface(RinGpuRuntimeSoftwareSurfaceDescV1* surface)
{
    memset(surface, 0, sizeof(*surface));
    surface->struct_size = sizeof(*surface);
    surface->version = RIN_GPU_RUNTIME_VERSION;
    surface->device_generation = 1u;
    surface->handle_secret = UINT64_C(0x4453443132534f46);
    surface->max_buffer_size = 1024u * 1024u;
    surface->max_image_size = 1024u * 1024u;
    surface->max_total_allocation_size = 4u * 1024u * 1024u;
    surface->max_image_dimension = 64u;
    surface->max_image_layers = 1u;
    surface->max_image_mip_levels = 1u;
    surface->max_image_sample_count = 1u;
    surface->adapter.abi_version = RIN_GPU_ABI_VERSION;
    surface->adapter.struct_size = sizeof(surface->adapter);
    surface->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                          RIN_GPU_QUEUE_COMPUTE |
                                          RIN_GPU_QUEUE_GRAPHICS;
    memcpy(surface->adapter.name, "rindx-d3d12", 12u);
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
    memcpy(surface->display.name, "rindx-d3d12", 12u);
    surface->present_callback = present;
    surface->acquire_image = acquire;
}

int main(void)
{
    RinGpuRuntimeSoftwareSurfaceDescV1 surface;
    RinDxD3d12Device device;
    RinDxD3d12CommandAllocator allocator;
    RinDxD3d12CommandList list;
    ShaderBlob vertex;
    ShaderBlob fragment;
    ShaderBlob compute;
    RinGpuGraphicsPipelineNativeDescV2 pipeline_desc;
    RinGpuImageDescV1 image_desc;
    RinGpuImageTransitionV1 transition;
    RinGpuDrawIndexedV2 draw;
    RinGpuDrawIndirectV1 indirect_draw;
    RinGpuDrawIndexedIndirectV1 indirect_indexed_draw;
    RinGpuDispatchIndirectV1 indirect_dispatch;
    RinGpuBufferDescV1 vertex_desc;
    RinGpuBufferDescV1 index_desc;
    RinGpuBufferDescV1 indirect_desc;
    RinGpuVertexAttributeV2 vertex_attribute;
    RinGpuVertexBufferLayoutV1 vertex_layout;
    RinGpuImageReadbackV1 readback;
    RinGpuImageDescV1 transfer_desc;
    RinGpuImageUploadV1 transfer_upload;
    RinGpuImageCopyRegionV1 copy_region;
    RinGpuImageResolveV1 resolve;
    RinGpuBufferDescV1 transfer_buffer_desc;
    RinGpuBufferClearV1 buffer_clear;
    RinGpuSamplerDescV1 sampler_desc;
    RinGpuRasterStateV1 raster_state;
    RinDxD3d12MappedResource mapped;
    RinGpuHandle vertex_shader = 0u;
    RinGpuHandle fragment_shader = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle invalid_pipeline = 0u;
    RinGpuHandle depth_pipeline = 0u;
    RinGpuHandle invalid_depth_pipeline = 0u;
    RinGpuHandle compute_shader = 0u;
    RinGpuHandle compute_pipeline = 0u;
    RinGpuHandle compute_bind_group = 0u;
    RinGpuHandle sampler = 0u;
    RinGpuHandle invalid_sampler = 0u;
    RinGpuHandle image = 0u;
    RinGpuHandle vertex_buffer = 0u;
    RinGpuHandle non_cpu_buffer = 0u;
    RinGpuHandle index_buffer = 0u;
    RinGpuHandle indirect_buffer = 0u;
    RinGpuHandle transfer_source = 0u;
    RinGpuHandle transfer_destination = 0u;
    RinGpuHandle clear_target = 0u;
    RinGpuHandle transfer_buffer_source = 0u;
    RinGpuHandle transfer_buffer_destination = 0u;
    uint8_t pixels[16u] = {0};
    uint8_t copied_pixels[16u] = {0};
    uint8_t clear_pixels[16u] = {0};
    uint8_t buffer_readback[16u] = {0};
    static const uint8_t transfer_source_pixels[16u] = {
        49u, 48u, 47u, 255u, 59u, 58u, 57u, 255u,
        69u, 68u, 67u, 255u, 79u, 78u, 77u, 255u};
    static const uint8_t zero_pixels[16u] = {0};
    uint64_t fence_value = 0u;
    const uint32_t feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;

    make_surface(&surface);
    CHECK(rindx_d3d12_create_device(&surface, &feature_level, 1u, &device) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_create_command_allocator(&device, &allocator) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_create_command_list(&device, &allocator, &list) ==
          RIN_GPU_OK);
    make_shader(&vertex, RIN_SHADER_STAGE_VERTEX);
    make_shader(&fragment, RIN_SHADER_STAGE_FRAGMENT);
    CHECK(rindx_d3d12_create_shader(&device, &vertex, vertex.header.total_size,
                                    &vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_shader(&device, &fragment,
                                    fragment.header.total_size,
                                    &fragment_shader) == RIN_GPU_OK);
    memset(&pipeline_desc, 0, sizeof(pipeline_desc));
    pipeline_desc.base.abi_version = RIN_GPU_ABI_VERSION;
    pipeline_desc.base.struct_size = sizeof(pipeline_desc.base);
    pipeline_desc.base.vertex_shader = vertex_shader;
    pipeline_desc.base.fragment_shader = fragment_shader;
    pipeline_desc.base.color_format = RIN_GPU_FORMAT_RGBA8_UNORM;
    pipeline_desc.base.primitive_topology = RIN_GPU_PRIMITIVE_POINT_LIST;
    pipeline_desc.base.position_output_location = 0u;
    pipeline_desc.base.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
    pipeline_desc.base.blend_enabled = 1u;
    pipeline_desc.base.source_color_factor = RIN_GPU_BLEND_ONE;
    pipeline_desc.base.destination_color_factor = RIN_GPU_BLEND_ZERO;
    pipeline_desc.base.color_operation = RIN_GPU_BLEND_ADD;
    pipeline_desc.base.source_alpha_factor = RIN_GPU_BLEND_ONE;
    pipeline_desc.base.destination_alpha_factor = RIN_GPU_BLEND_ZERO;
    pipeline_desc.base.alpha_operation = RIN_GPU_BLEND_ADD;
    pipeline_desc.base.cull_mode = RIN_GPU_CULL_NONE;
    pipeline_desc.base.front_face = RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_desc.base.struct_size = sizeof(pipeline_desc);
    memset(&vertex_attribute, 0, sizeof(vertex_attribute));
    vertex_attribute.abi_version = RIN_GPU_ABI_VERSION;
    vertex_attribute.struct_size = sizeof(vertex_attribute);
    vertex_attribute.location = 0u;
    vertex_attribute.format = RIN_GPU_VERTEX_FLOAT32;
    vertex_attribute.offset = 0u;
    vertex_attribute.binding = 0u;
    memset(&vertex_layout, 0, sizeof(vertex_layout));
    vertex_layout.binding = 0u;
    vertex_layout.stride = sizeof(float);
    CHECK(rindx_d3d12_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u,
              &pipeline) == RIN_GPU_OK);
    pipeline_desc.base.source_color_factor = 99u;
    CHECK(rindx_d3d12_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &invalid_pipeline) != RIN_GPU_OK);
    pipeline_desc.base.source_color_factor = RIN_GPU_BLEND_ONE;
    pipeline_desc.base.depth_format = RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    pipeline_desc.base.depth_compare = RIN_GPU_COMPARE_LESS;
    pipeline_desc.base.depth_write_enabled = 1u;
    CHECK(rindx_d3d12_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &depth_pipeline) == RIN_GPU_OK);
    pipeline_desc.base.depth_compare = 99u;
    CHECK(rindx_d3d12_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &invalid_depth_pipeline) != RIN_GPU_OK);
    make_compute_shader(&compute);
    CHECK(rindx_d3d12_create_shader(&device, &compute,
                                    compute.header.total_size,
                                    &compute_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_compute_pipeline(&device, compute_shader,
                                              &compute_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_compute_bind_group(&device, compute_pipeline,
                                                NULL, 0u,
                                                &compute_bind_group) ==
          RIN_GPU_OK);
    memset(&sampler_desc, 0, sizeof(sampler_desc));
    sampler_desc.abi_version = RIN_GPU_ABI_VERSION;
    sampler_desc.struct_size = sizeof(sampler_desc);
    sampler_desc.min_filter = RIN_GPU_SAMPLER_FILTER_LINEAR;
    sampler_desc.mag_filter = RIN_GPU_SAMPLER_FILTER_LINEAR;
    sampler_desc.mip_filter = RIN_GPU_SAMPLER_MIP_FILTER_NONE;
    sampler_desc.address_u = RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler_desc.address_v = RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler_desc.address_w = RIN_GPU_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    sampler_desc.max_anisotropy = 1u;
    CHECK(rindx_d3d12_create_sampler(&device, &sampler_desc, &sampler) ==
          RIN_GPU_OK);
    sampler_desc.min_filter = 99u;
    CHECK(rindx_d3d12_create_sampler(&device, &sampler_desc, &invalid_sampler) !=
          RIN_GPU_OK);

    memset(&image_desc, 0, sizeof(image_desc));
    image_desc.abi_version = RIN_GPU_ABI_VERSION;
    image_desc.struct_size = sizeof(image_desc);
    image_desc.dimension = RIN_GPU_IMAGE_DIMENSION_2D;
    image_desc.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    image_desc.width = 2u;
    image_desc.height = 2u;
    image_desc.depth = 1u;
    image_desc.array_layers = 1u;
    image_desc.mip_levels = 1u;
    image_desc.sample_count = 1u;
    image_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                       RIN_GPU_IMAGE_COLOR_TARGET;
    image_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d12_create_texture2d(&device, &image_desc, &image) ==
          RIN_GPU_OK);
    transfer_desc = image_desc;
    transfer_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                          RIN_GPU_IMAGE_COPY_DESTINATION;
    transfer_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE |
                          RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d12_create_texture2d(&device, &transfer_desc,
                                       &transfer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_texture2d(&device, &transfer_desc,
                                       &transfer_destination) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_texture2d(&device, &transfer_desc,
                                       &clear_target) == RIN_GPU_OK);
    memset(&transfer_upload, 0, sizeof(transfer_upload));
    transfer_upload.abi_version = RIN_GPU_ABI_VERSION;
    transfer_upload.struct_size = sizeof(transfer_upload);
    transfer_upload.width = 2u;
    transfer_upload.height = 2u;
    transfer_upload.depth = 1u;
    CHECK(rindx_d3d12_upload_image(&device, transfer_source, &transfer_upload,
                                   transfer_source_pixels,
                                   sizeof(transfer_source_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d12_upload_image(&device, transfer_destination,
                                   &transfer_upload, zero_pixels,
                                   sizeof(zero_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d12_upload_image(&device, clear_target, &transfer_upload,
                                   zero_pixels, sizeof(zero_pixels)) == RIN_GPU_OK);
    memset(&transfer_buffer_desc, 0, sizeof(transfer_buffer_desc));
    transfer_buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    transfer_buffer_desc.struct_size = sizeof(transfer_buffer_desc);
    transfer_buffer_desc.size_bytes = 16u;
    transfer_buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                                 RIN_GPU_BUFFER_COPY_DESTINATION;
    transfer_buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d12_create_buffer(&device, &transfer_buffer_desc,
                                    &transfer_buffer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_buffer(&device, &transfer_buffer_desc,
                                    &transfer_buffer_destination) == RIN_GPU_OK);
    CHECK(rindx_d3d12_upload_buffer(&device, transfer_buffer_source, 0u,
                                    transfer_source_pixels,
                                    sizeof(transfer_source_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d12_upload_buffer(&device, transfer_buffer_destination, 0u,
                                    zero_pixels, sizeof(zero_pixels)) == RIN_GPU_OK);
    memset(&mapped, 0, sizeof(mapped));
    mapped.struct_size = sizeof(mapped);
    mapped.version = RIN_DX_D3D12_VERSION;
    CHECK(rindx_d3d12_map_buffer(&device, transfer_buffer_destination,
                                 RIN_DX_D3D12_MAP_READ_WRITE, 0u,
                                 &mapped) == RIN_GPU_OK);
    CHECK(mapped.data != NULL && mapped.size_bytes == sizeof(buffer_readback));
    ((uint8_t*)mapped.data)[0u] = 0x6au;
    CHECK(rindx_d3d12_unmap_buffer(&device, transfer_buffer_destination,
                                   &mapped) == RIN_GPU_OK);
    CHECK(rindx_d3d12_readback_buffer(&device, transfer_buffer_destination,
                                      0u, buffer_readback,
                                      sizeof(buffer_readback)) == RIN_GPU_OK);
    CHECK(buffer_readback[0u] == 0x6au);
    memset(&buffer_clear, 0, sizeof(buffer_clear));
    buffer_clear.abi_version = RIN_GPU_ABI_VERSION;
    buffer_clear.struct_size = sizeof(buffer_clear);
    buffer_clear.size_bytes = 16u;
    buffer_clear.pattern = UINT32_C(0x01020304);
    CHECK(rindx_d3d12_copy_buffer(&list, transfer_buffer_destination, 0u,
                                  transfer_buffer_source, 0u, 16u) == RIN_GPU_OK);
    CHECK(rindx_d3d12_clear_buffer(&list, transfer_buffer_destination,
                                   &buffer_clear) == RIN_GPU_OK);
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(rindx_d3d12_transition_image(&list, transfer_source,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    memset(&copy_region, 0, sizeof(copy_region));
    copy_region.abi_version = RIN_GPU_ABI_VERSION;
    copy_region.struct_size = sizeof(copy_region);
    copy_region.width = 2u;
    copy_region.height = 2u;
    copy_region.depth = 1u;
    CHECK(rindx_d3d12_copy_texture2d(&list, transfer_destination,
                                     transfer_source, &copy_region) == RIN_GPU_OK);
    CHECK(rindx_d3d12_clear_render_target(
              &list, clear_target, 0.0f, 0.0f, 1.0f, 1.0f) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(rindx_d3d12_transition_image(&list, transfer_destination,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d12_transition_image(&list, clear_target,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    memset(&resolve, 0, sizeof(resolve));
    resolve.abi_version = RIN_GPU_ABI_VERSION;
    resolve.struct_size = sizeof(resolve);
    resolve.width = 1u;
    resolve.height = 1u;
    CHECK(rindx_d3d12_resolve_texture2d(&list, transfer_destination,
                                        transfer_source, &resolve) !=
          RIN_GPU_OK);
    memset(&vertex_desc, 0, sizeof(vertex_desc));
    vertex_desc.abi_version = RIN_GPU_ABI_VERSION;
    vertex_desc.struct_size = sizeof(vertex_desc);
    vertex_desc.size_bytes = sizeof(float);
    vertex_desc.usage = RIN_GPU_BUFFER_VERTEX | RIN_GPU_BUFFER_COPY_DESTINATION;
    vertex_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d12_create_buffer(&device, &vertex_desc, &vertex_buffer) ==
          RIN_GPU_OK);
    {
        const float position = 0.0f;
        CHECK(rindx_d3d12_upload_buffer(&device, vertex_buffer, 0u, &position,
                                        sizeof(position)) == RIN_GPU_OK);
    }
    vertex_desc.flags = 0u;
    CHECK(rindx_d3d12_create_buffer(&device, &vertex_desc, &non_cpu_buffer) ==
          RIN_GPU_OK);
    memset(&mapped, 0, sizeof(mapped));
    mapped.struct_size = sizeof(mapped);
    mapped.version = RIN_DX_D3D12_VERSION;
    CHECK(rindx_d3d12_map_buffer(&device, non_cpu_buffer,
                                 RIN_DX_D3D12_MAP_READ, 0u,
                                 &mapped) == RIN_GPU_ERROR_STATE);
    memset(&index_desc, 0, sizeof(index_desc));
    index_desc.abi_version = RIN_GPU_ABI_VERSION;
    index_desc.struct_size = sizeof(index_desc);
    index_desc.size_bytes = 1u;
    index_desc.usage = RIN_GPU_BUFFER_INDEX | RIN_GPU_BUFFER_COPY_DESTINATION;
    index_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d12_create_buffer(&device, &index_desc, &index_buffer) ==
          RIN_GPU_OK);
    {
        const uint8_t index = 0u;
        CHECK(rindx_d3d12_upload_buffer(&device, index_buffer, 0u, &index,
                                        sizeof(index)) == RIN_GPU_OK);
    }
    memset(&indirect_desc, 0, sizeof(indirect_desc));
    indirect_desc.abi_version = RIN_GPU_ABI_VERSION;
    indirect_desc.struct_size = sizeof(indirect_desc);
    indirect_desc.size_bytes = 32u;
    indirect_desc.usage = RIN_GPU_BUFFER_INDIRECT | RIN_GPU_BUFFER_COPY_DESTINATION;
    indirect_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d12_create_buffer(&device, &indirect_desc, &indirect_buffer) ==
          RIN_GPU_OK);
    {
        const uint32_t indirect_packets[8] = {
            1u, 1u, 0u, 0u, 0u,
            1u, 1u, 1u
        };
        CHECK(rindx_d3d12_upload_buffer(&device, indirect_buffer, 0u,
                                        indirect_packets,
                                        sizeof(indirect_packets)) == RIN_GPU_OK);
    }
    CHECK(rindx_d3d12_transition_image(&list, image,
                                       RIN_GPU_IMAGE_STATE_UNDEFINED,
                                       RIN_GPU_IMAGE_STATE_COLOR_TARGET) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_begin_render_pass(&list, image, 0u, 1u,
                                        0.0f, 0.0f, 0.0f, 1.0f, 1.0f) ==
          RIN_GPU_OK);
    memset(&raster_state, 0, sizeof(raster_state));
    raster_state.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.struct_size = sizeof(raster_state);
    raster_state.viewport.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.viewport.struct_size = sizeof(raster_state.viewport);
    raster_state.viewport.width = 2.0f;
    raster_state.viewport.height = 2.0f;
    raster_state.viewport.max_depth = 1.0f;
    raster_state.scissor.abi_version = RIN_GPU_ABI_VERSION;
    raster_state.scissor.struct_size = sizeof(raster_state.scissor);
    raster_state.scissor.width = 2u;
    raster_state.scissor.height = 2u;
    raster_state.scissor.enabled = 1u;
    CHECK(rindx_d3d12_set_raster_state(&list, &raster_state) == RIN_GPU_OK);
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = pipeline;
    draw.color_target = image;
    draw.index_buffer = index_buffer;
    draw.index_format = RIN_GPU_INDEX_UINT8;
    draw.index_count = 1u;
    draw.instance_count = 1u;
    draw.vertex_count = 1u;
    draw.binding_count = 1u;
    draw.vertex_buffers[0].binding = 0u;
    draw.vertex_buffers[0].buffer = vertex_buffer;
    CHECK(rindx_d3d12_draw_indexed_instanced(&list, &draw) == RIN_GPU_OK);
    memset(&indirect_indexed_draw, 0, sizeof(indirect_indexed_draw));
    indirect_indexed_draw.abi_version = RIN_GPU_ABI_VERSION;
    indirect_indexed_draw.struct_size = sizeof(indirect_indexed_draw);
    indirect_indexed_draw.pipeline = pipeline;
    indirect_indexed_draw.color_target = image;
    indirect_indexed_draw.index_buffer = index_buffer;
    indirect_indexed_draw.indirect_buffer = indirect_buffer;
    indirect_indexed_draw.index_format = RIN_GPU_INDEX_UINT8;
    indirect_indexed_draw.draw_count = 1u;
    indirect_indexed_draw.stride = 20u;
    indirect_indexed_draw.vertex_count = 1u;
    indirect_indexed_draw.binding_count = 1u;
    indirect_indexed_draw.vertex_buffers[0].binding = 0u;
    indirect_indexed_draw.vertex_buffers[0].buffer = vertex_buffer;
    CHECK(rindx_d3d12_execute_indirect_draw_indexed(
              &list, &indirect_indexed_draw) == RIN_GPU_OK);
    memset(&indirect_draw, 0, sizeof(indirect_draw));
    indirect_draw.abi_version = RIN_GPU_ABI_VERSION;
    indirect_draw.struct_size = sizeof(indirect_draw);
    indirect_draw.pipeline = pipeline;
    indirect_draw.color_target = image;
    indirect_draw.indirect_buffer = indirect_buffer;
    indirect_draw.draw_count = 1u;
    indirect_draw.stride = 20u;
    indirect_draw.binding_count = 1u;
    indirect_draw.vertex_buffers[0].binding = 0u;
    indirect_draw.vertex_buffers[0].buffer = vertex_buffer;
    CHECK(rindx_d3d12_execute_indirect_draw(&list, &indirect_draw) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_end_render_pass(&list) == RIN_GPU_OK);
    {
        RinGpuDispatchV1 dispatch;
        memset(&dispatch, 0, sizeof(dispatch));
        dispatch.abi_version = RIN_GPU_ABI_VERSION;
        dispatch.struct_size = sizeof(dispatch);
        dispatch.pipeline = compute_pipeline;
        dispatch.bind_group = compute_bind_group;
        dispatch.group_count_x = 1u;
        dispatch.group_count_y = 1u;
        dispatch.group_count_z = 1u;
        CHECK(rindx_d3d12_dispatch(&list, &dispatch) == RIN_GPU_OK);
    }
    memset(&indirect_dispatch, 0, sizeof(indirect_dispatch));
    indirect_dispatch.abi_version = RIN_GPU_ABI_VERSION;
    indirect_dispatch.struct_size = sizeof(indirect_dispatch);
    indirect_dispatch.pipeline = compute_pipeline;
    indirect_dispatch.bind_group = compute_bind_group;
    indirect_dispatch.indirect_buffer = indirect_buffer;
    indirect_dispatch.indirect_offset = 20u;
    CHECK(rindx_d3d12_execute_indirect_dispatch(&list, &indirect_dispatch) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_transition_image(&list, image,
                                       RIN_GPU_IMAGE_STATE_COLOR_TARGET,
                                       RIN_GPU_IMAGE_STATE_COPY_SOURCE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_execute_command_lists(&device, &list, &fence_value) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_reset_command_list(&list) == RIN_GPU_OK);
    memset(&readback, 0, sizeof(readback));
    readback.abi_version = RIN_GPU_ABI_VERSION;
    readback.struct_size = sizeof(readback);
    readback.width = 2u;
    readback.height = 2u;
    readback.depth = 1u;
    CHECK(rindx_d3d12_readback_image(&device, image, &readback, pixels,
                                     sizeof(pixels)) == RIN_GPU_OK);
    CHECK(pixels[0] != 0u || pixels[1] != 0u || pixels[2] != 0u ||
          pixels[3] != 0u);
    CHECK(rindx_d3d12_readback_image(&device, transfer_destination, &readback,
                                     copied_pixels, sizeof(copied_pixels)) ==
          RIN_GPU_OK);
    CHECK(memcmp(copied_pixels, transfer_source_pixels,
                 sizeof(copied_pixels)) == 0);
    CHECK(rindx_d3d12_readback_image(&device, clear_target, &readback,
                                     clear_pixels, sizeof(clear_pixels)) ==
          RIN_GPU_OK);
    for (uint32_t pixel = 0u; pixel < sizeof(clear_pixels); pixel += 4u)
        CHECK(clear_pixels[pixel] == 0u && clear_pixels[pixel + 1u] == 0u &&
              clear_pixels[pixel + 2u] == 255u && clear_pixels[pixel + 3u] ==
              255u);
    CHECK(rindx_d3d12_readback_buffer(&device, transfer_buffer_destination,
                                      0u, buffer_readback,
                                      sizeof(buffer_readback)) == RIN_GPU_OK);
    for (uint32_t offset = 0u; offset < sizeof(buffer_readback); offset += 4u)
        CHECK(buffer_readback[offset] == 0x04u &&
              buffer_readback[offset + 1u] == 0x03u &&
              buffer_readback[offset + 2u] == 0x02u &&
              buffer_readback[offset + 3u] == 0x01u);
    CHECK(rindx_d3d12_destroy_command_list(&list) == RIN_GPU_OK);
    CHECK(rindx_d3d12_reset_command_allocator(&allocator) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_command_allocator(&allocator) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, depth_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, compute_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, sampler) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, compute_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, compute_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, fragment_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, vertex_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, non_cpu_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, index_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, indirect_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, transfer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, transfer_destination) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, clear_target) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, transfer_buffer_source) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, transfer_buffer_destination) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_object(&device, image) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_device(&device) == RIN_GPU_OK);
    return 0;
}
