/* SPDX-License-Identifier: MIT */
#include <rindx/d3d11.h>

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

typedef struct PresentCapture {
    uint32_t calls;
} PresentCapture;

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

static void make_vertex(ShaderBlob* shader)
{
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_VERTEX;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.input_count = 1u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    instruction(&shader->instructions[0], RIN_SHADER_OP_LOAD_INPUT_F32,
                0u, RIN_SHADER_UNUSED, 0u);
    for (index = 1u; index < 4u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED,
                    f32_bits(index == 3u ? 1.0f : 0.0f));
    }
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[4u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, index);
    }
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static void make_fragment(ShaderBlob* shader)
{
    static const float color[4] = {1.0f, 0.25f, 0.0f, 1.0f};
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_FRAGMENT;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED,
                    f32_bits(color[index]));
        instruction(&shader->instructions[4u + index],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)index, index);
    }
    instruction(&shader->instructions[8], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static void make_storage_fragment(ShaderBlob* shader)
{
    static const float color[4] = {1.0f, 0.25f, 0.0f, 1.0f};
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_FRAGMENT;
    shader->header.instruction_count = 13u;
    shader->header.register_count = 7u;
    shader->header.output_count = 4u;
    shader->header.resource_count = 1u;
    shader->header.total_size = sizeof(shader->header) +
                                13u * sizeof(shader->instructions[0]);
    instruction(&shader->instructions[0], RIN_SHADER_OP_CONST_I32,
                0u, RIN_SHADER_UNUSED, 0u);
    instruction(&shader->instructions[1], RIN_SHADER_OP_CONST_I32,
                1u, RIN_SHADER_UNUSED, 0u);
    instruction(&shader->instructions[2], RIN_SHADER_OP_CONST_I32,
                2u, RIN_SHADER_UNUSED, 123u);
    instruction(&shader->instructions[3], RIN_SHADER_OP_STORE_IMAGE_2D_I32,
                2u, 0u, 0u);
    shader->instructions[3].source1 = 1u;
    shader->instructions[3].resource = 0u;
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[index + 4u],
                    RIN_SHADER_OP_CONST_F32, (uint16_t)(index + 3u),
                    RIN_SHADER_UNUSED, f32_bits(color[index]));
        instruction(&shader->instructions[index + 8u],
                    RIN_SHADER_OP_STORE_OUTPUT_F32, RIN_SHADER_UNUSED,
                    (uint16_t)(index + 3u), index);
    }
    instruction(&shader->instructions[12], RIN_SHADER_OP_RETURN,
                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
}

static void make_sample_fragment(ShaderBlob* shader)
{
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_FRAGMENT;
    shader->header.instruction_count = 11u;
    shader->header.register_count = 6u;
    shader->header.output_count = 4u;
    shader->header.resource_count = 2u;
    shader->header.total_size = sizeof(shader->header) +
                                11u * sizeof(shader->instructions[0]);
    instruction(&shader->instructions[0], RIN_SHADER_OP_CONST_F32, 0u,
                RIN_SHADER_UNUSED, f32_bits(0.25f));
    instruction(&shader->instructions[1], RIN_SHADER_OP_CONST_F32, 1u,
                RIN_SHADER_UNUSED, f32_bits(0.25f));
    instruction(&shader->instructions[2], RIN_SHADER_OP_SAMPLE_IMAGE_2D_F32,
                2u, 0u, 1u);
    shader->instructions[2].source1 = 1u;
    shader->instructions[2].resource = 0u;
    shader->instructions[2].flags = RIN_SHADER_SAMPLE_COMPONENT_RED;
    instruction(&shader->instructions[3], RIN_SHADER_OP_CONST_F32, 3u,
                RIN_SHADER_UNUSED, f32_bits(0.0f));
    instruction(&shader->instructions[4], RIN_SHADER_OP_CONST_F32, 4u,
                RIN_SHADER_UNUSED, f32_bits(0.0f));
    instruction(&shader->instructions[5], RIN_SHADER_OP_CONST_F32, 5u,
                RIN_SHADER_UNUSED, f32_bits(1.0f));
    instruction(&shader->instructions[6], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 2u, 0u);
    instruction(&shader->instructions[7], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 3u, 1u);
    instruction(&shader->instructions[8], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 4u, 2u);
    instruction(&shader->instructions[9], RIN_SHADER_OP_STORE_OUTPUT_F32,
                RIN_SHADER_UNUSED, 5u, 3u);
    instruction(&shader->instructions[10], RIN_SHADER_OP_RETURN,
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
    PresentCapture* capture = (PresentCapture*)context;
    if (!capture || !image || image->struct_size != sizeof(*image) ||
        image->version != RIN_GPU_SOFTWARE_BACKEND_VERSION ||
        !image->pixels || image->format != RIN_GPU_FORMAT_RGBA8_UNORM ||
        image->width != 2u || image->height != 2u ||
        image->display_id != RIN_GPU_PRIMARY_DISPLAY ||
        image->row_pitch_bytes < 8u || image->size_bytes < 16u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    capture->calls++;
    return RIN_GPU_OK;
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
    surface->handle_secret = UINT64_C(0x4453443131534f46);
    surface->max_buffer_size = 1024u * 1024u;
    surface->max_image_size = 1024u * 1024u;
    surface->max_total_allocation_size = 4u * 1024u * 1024u;
    surface->max_image_dimension = 64u;
    surface->max_image_layers = 1u;
    surface->max_image_mip_levels = 4u;
    surface->max_image_sample_count = 1u;
    surface->adapter.abi_version = RIN_GPU_ABI_VERSION;
    surface->adapter.struct_size = sizeof(surface->adapter);
    surface->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                          RIN_GPU_QUEUE_COMPUTE |
                                          RIN_GPU_QUEUE_GRAPHICS;
    memcpy(surface->adapter.name, "rindx-d3d11", 12u);
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
    memcpy(surface->display.name, "rindx-d3d11", 12u);
    surface->present_callback = present;
    surface->acquire_image = acquire;
}

int main(void)
{
    RinGpuRuntimeSoftwareSurfaceDescV1 surface;
    RinDxD3d11Device device;
    RinDxD3d11Context context;
    ShaderBlob vertex;
    ShaderBlob fragment;
    ShaderBlob sample_fragment;
    ShaderBlob compute;
    RinGpuGraphicsPipelineNativeDescV2 pipeline_desc;
    RinGpuVertexAttributeV2 vertex_attribute;
    RinGpuVertexBufferLayoutV1 vertex_layout;
    RinGpuImageDescV1 image_desc;
    RinGpuImageDescV1 texture1d_desc;
    RinGpuImageDescV1 texture3d_desc;
    RinGpuImageUploadV1 texture3d_upload;
    RinGpuImageReadbackV1 texture3d_readback;
    RinGpuImageDescV1 storage_desc;
    RinGpuImageDescV1 sampled_desc;
    RinGpuImageUploadV1 sampled_upload;
    RinGpuImageTransitionV1 transition;
    RinGpuDrawIndexedV2 draw;
    RinGpuDrawIndexedV2 sample_draw;
    RinGpuBufferDescV1 vertex_desc;
    RinGpuBufferDescV1 index_desc;
    RinGpuImageReadbackV1 readback;
    RinGpuImageReadbackV1 storage_readback;
    RinGpuImageDescV1 transfer_desc;
    RinGpuImageUploadV1 transfer_upload;
    RinGpuImageCopyRegionV1 copy_region;
    RinGpuImageResolveV1 resolve;
    RinGpuImageDescV1 mip_desc;
    RinGpuImageUploadV1 mip_upload;
    RinGpuImageReadbackV1 mip_readback;
    RinGpuBufferDescV1 transfer_buffer_desc;
    RinGpuBufferClearV1 buffer_clear;
    RinGpuImageDescV1 depth_desc;
    RinGpuImageReadbackV1 depth_readback;
    RinGpuSamplerDescV1 sampler_desc;
    RinGpuRasterStateV1 raster_state;
    RinGpuQueryResultV1 query_result;
    RinDxD3d11MappedResource mapped;
    RinDxD3d11Context predicate_context;
    RinDxD3d11Context predicate_false_context;
    RinDxD3d11Context deferred_context;
    RinDxD3d11Context execute_context;
    RinGpuHandle vertex_shader = 0u;
    RinGpuHandle fragment_shader = 0u;
    RinGpuHandle sample_fragment_shader = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle sample_pipeline = 0u;
    RinGpuHandle invalid_pipeline = 0u;
    RinGpuHandle depth_pipeline = 0u;
    RinGpuHandle invalid_depth_pipeline = 0u;
    RinGpuHandle compute_shader = 0u;
    RinGpuHandle compute_pipeline = 0u;
    RinGpuHandle compute_bind_group = 0u;
    RinGpuHandle graphics_bind_group = 0u;
    RinGpuHandle sample_bind_group = 0u;
    RinGpuHandle sampler = 0u;
    RinGpuHandle invalid_sampler = 0u;
    RinGpuHandle occlusion_query = 0u;
    RinGpuHandle timestamp_query = 0u;
    RinGpuHandle pipeline_query = 0u;
    RinGpuHandle invalid_query = 0u;
    RinGpuHandle image = 0u;
    RinGpuHandle texture1d = 0u;
    RinGpuHandle texture3d = 0u;
    RinGpuHandle deferred_command_list = 0u;
    RinGpuHandle storage_image = 0u;
    RinGpuHandle sampled_image = 0u;
    RinGpuHandle sample_target = 0u;
    RinGpuHandle vertex_buffer = 0u;
    RinGpuHandle non_cpu_buffer = 0u;
    RinGpuHandle index_buffer = 0u;
    RinGpuHandle transfer_source = 0u;
    RinGpuHandle transfer_destination = 0u;
    RinGpuHandle subresource_destination = 0u;
    RinGpuHandle clear_target = 0u;
    RinGpuHandle present_target = 0u;
    RinGpuHandle mip_image = 0u;
    RinGpuHandle transfer_buffer_source = 0u;
    RinGpuHandle transfer_buffer_destination = 0u;
    RinGpuHandle depth_image = 0u;
    uint8_t pixels[16u] = {0};
    uint8_t storage_pixels[4u] = {0};
    uint8_t copied_pixels[16u] = {0};
    uint8_t subresource_pixels[16u] = {0};
    uint8_t clear_pixels[16u] = {0};
    uint8_t mip0_pixels[64u] = {0};
    uint8_t mip1_pixels[16u] = {0};
    uint8_t mip2_pixels[4u] = {0};
    uint8_t mip_readback_pixels[16u] = {0};
    uint8_t depth_pixels[32u] = {0};
    uint8_t texture3d_source[32u] = {0};
    uint8_t texture3d_pixels[32u] = {0};
    uint8_t sampled_source_pixels[16u] = {
        64u, 32u, 16u, 255u, 64u, 32u, 16u, 255u,
        64u, 32u, 16u, 255u, 64u, 32u, 16u, 255u};
    uint8_t sampled_pixels[16u] = {0};
    uint8_t buffer_readback[16u] = {0};
    static const uint8_t transfer_source_pixels[16u] = {
        9u, 8u, 7u, 255u, 19u, 18u, 17u, 255u,
        29u, 28u, 27u, 255u, 39u, 38u, 37u, 255u};
    static const uint8_t zero_pixels[16u] = {0};
    uint64_t fence_value = 0u;
    PresentCapture present_capture;
    const uint32_t feature_level = RIN_DX_D3D11_FEATURE_LEVEL_11_0;
    int create_result;

    make_surface(&surface);
    memset(&present_capture, 0, sizeof(present_capture));
    surface.present_context = &present_capture;
    create_result = rindx_d3d11_create_device(&surface, &feature_level, 1u,
                                              &device);
    if (create_result != RIN_GPU_OK)
        fprintf(stderr, "create device result: %d size=%u/%zu version=%u gen=%llu secret=%llu callbacks=%p/%p adapter=%u/%u display=%u/%u %ux%u flags=%u/%u/%llu/%llu\n",
                create_result, surface.struct_size, sizeof(surface),
                surface.version, (unsigned long long)surface.device_generation,
                (unsigned long long)surface.handle_secret,
                (void*)surface.present_callback, (void*)surface.acquire_image,
                surface.adapter.struct_size, surface.adapter.abi_version,
                surface.display.struct_size, surface.display.abi_version,
                surface.display.width, surface.display.height, surface.flags,
                surface.reserved0, (unsigned long long)surface.reserved[0],
                (unsigned long long)surface.reserved[1]);
    CHECK(create_result == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_context(&device, &context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_set_multithread_protected(&context, 1u) == RIN_GPU_OK);
    CHECK(rindx_d3d11_enter_multithread(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_enter_multithread(&context) == RIN_GPU_ERROR_BUSY);
    CHECK(rindx_d3d11_leave_multithread(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_leave_multithread(&context) == RIN_GPU_ERROR_STATE);
    CHECK(rindx_d3d11_set_multithread_protected(&context, 0u) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_query(&device, RIN_DX_D3D11_QUERY_OCCLUSION,
                                   &occlusion_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_query(&device, RIN_DX_D3D11_QUERY_TIMESTAMP,
                                   &timestamp_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_query(
              &device, RIN_DX_D3D11_QUERY_PIPELINE_STATISTICS,
              &pipeline_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_query(&device, 99u, &invalid_query) !=
          RIN_GPU_OK);
    make_vertex(&vertex);
    make_storage_fragment(&fragment);
    make_sample_fragment(&sample_fragment);
    CHECK(rindx_d3d11_create_shader(&device, &vertex, vertex.header.total_size,
                                    &vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_shader(&device, &fragment,
                                    fragment.header.total_size,
                                    &fragment_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_shader(&device, &sample_fragment,
                                    sample_fragment.header.total_size,
                                    &sample_fragment_shader) == RIN_GPU_OK);
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
    vertex_attribute.binding = 0u;
    memset(&vertex_layout, 0, sizeof(vertex_layout));
    vertex_layout.binding = 0u;
    vertex_layout.stride = sizeof(float);
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u,
              &pipeline) == RIN_GPU_OK);
    pipeline_desc.base.fragment_shader = sample_fragment_shader;
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &sample_pipeline) == RIN_GPU_OK);
    pipeline_desc.base.fragment_shader = fragment_shader;
    pipeline_desc.base.source_color_factor = 99u;
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &invalid_pipeline) != RIN_GPU_OK);
    pipeline_desc.base.source_color_factor = RIN_GPU_BLEND_ONE;
    pipeline_desc.base.depth_format = RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    pipeline_desc.base.depth_compare = RIN_GPU_COMPARE_LESS;
    pipeline_desc.base.depth_write_enabled = 1u;
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &depth_pipeline) == RIN_GPU_OK);
    pipeline_desc.base.depth_compare = 99u;
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, &vertex_attribute, 1u, &vertex_layout,
              1u, NULL, 0u, &invalid_depth_pipeline) != RIN_GPU_OK);
    make_compute_shader(&compute);
    CHECK(rindx_d3d11_create_shader(&device, &compute,
                                    compute.header.total_size,
                                    &compute_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_compute_pipeline(&device, compute_shader,
                                              &compute_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_compute_bind_group(&device, compute_pipeline,
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
    CHECK(rindx_d3d11_create_sampler(&device, &sampler_desc, &sampler) ==
          RIN_GPU_OK);
    sampler_desc.min_filter = 99u;
    CHECK(rindx_d3d11_create_sampler(&device, &sampler_desc, &invalid_sampler) !=
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
    CHECK(rindx_d3d11_create_texture2d(&device, &image_desc, &image) ==
          RIN_GPU_OK);
    {
        RinGpuImageDescV1 present_desc = image_desc;
        present_desc.usage = RIN_GPU_IMAGE_PRESENT |
                             RIN_GPU_IMAGE_COLOR_TARGET;
        present_desc.flags = 0u;
        CHECK(rindx_d3d11_create_texture2d(&device, &present_desc,
                                           &present_target) == RIN_GPU_OK);
    }
    texture1d_desc = image_desc;
    texture1d_desc.dimension = RIN_GPU_IMAGE_DIMENSION_1D;
    texture1d_desc.height = 1u;
    texture1d_desc.depth = 1u;
    texture1d_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                           RIN_GPU_IMAGE_COPY_DESTINATION;
    texture1d_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE |
                           RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture1d(&device, &texture1d_desc, &texture1d) ==
          RIN_GPU_OK);
    texture3d_desc = image_desc;
    texture3d_desc.dimension = RIN_GPU_IMAGE_DIMENSION_3D;
    texture3d_desc.width = 2u;
    texture3d_desc.height = 2u;
    texture3d_desc.depth = 2u;
    texture3d_desc.array_layers = 1u;
    texture3d_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                           RIN_GPU_IMAGE_COPY_DESTINATION;
    texture3d_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE |
                           RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture3d(&device, &texture3d_desc, &texture3d) ==
          RIN_GPU_OK);
    for (uint32_t byte = 0u; byte < sizeof(texture3d_source); ++byte)
        texture3d_source[byte] = (uint8_t)(byte + 17u);
    memset(&texture3d_upload, 0, sizeof(texture3d_upload));
    texture3d_upload.abi_version = RIN_GPU_ABI_VERSION;
    texture3d_upload.struct_size = sizeof(texture3d_upload);
    texture3d_upload.width = 2u;
    texture3d_upload.height = 2u;
    texture3d_upload.depth = 2u;
    CHECK(rindx_d3d11_upload_image(&device, texture3d, &texture3d_upload,
                                   texture3d_source,
                                   sizeof(texture3d_source)) == RIN_GPU_OK);
    sampled_desc = image_desc;
    sampled_desc.usage = RIN_GPU_IMAGE_SAMPLED |
                         RIN_GPU_IMAGE_COPY_DESTINATION;
    sampled_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE;
    CHECK(rindx_d3d11_create_texture2d(&device, &sampled_desc,
                                       &sampled_image) == RIN_GPU_OK);
    sampled_desc = image_desc;
    sampled_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                         RIN_GPU_IMAGE_COLOR_TARGET;
    sampled_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture2d(&device, &sampled_desc,
                                       &sample_target) == RIN_GPU_OK);
    memset(&sampled_upload, 0, sizeof(sampled_upload));
    sampled_upload.abi_version = RIN_GPU_ABI_VERSION;
    sampled_upload.struct_size = sizeof(sampled_upload);
    sampled_upload.width = 2u;
    sampled_upload.height = 2u;
    sampled_upload.depth = 1u;
    CHECK(rindx_d3d11_upload_image(&device, sampled_image, &sampled_upload,
                                   sampled_source_pixels,
                                   sizeof(sampled_source_pixels)) == RIN_GPU_OK);
    {
        RinGpuGraphicsBindingV1 sampled_bindings[2];
        memset(sampled_bindings, 0, sizeof(sampled_bindings));
        sampled_bindings[0].abi_version = RIN_GPU_ABI_VERSION;
        sampled_bindings[0].struct_size = sizeof(sampled_bindings[0]);
        sampled_bindings[0].binding = 0u;
        sampled_bindings[0].kind = RIN_SHADER_RESOURCE_SAMPLED_IMAGE;
        sampled_bindings[0].access = RIN_GPU_RESOURCE_READ;
        sampled_bindings[0].resource = sampled_image;
        sampled_bindings[1].abi_version = RIN_GPU_ABI_VERSION;
        sampled_bindings[1].struct_size = sizeof(sampled_bindings[1]);
        sampled_bindings[1].binding = 1u;
        sampled_bindings[1].kind = RIN_SHADER_RESOURCE_SAMPLER;
        sampled_bindings[1].access = 0u;
        sampled_bindings[1].resource = sampler;
        CHECK(rindx_d3d11_create_graphics_bind_group(
                  &device, sample_pipeline, sampled_bindings, 2u,
                  &sample_bind_group) == RIN_GPU_OK);
    }
    CHECK(rindx_d3d11_transition_image(
              &context, texture3d, RIN_GPU_IMAGE_STATE_COPY_DESTINATION,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
    storage_desc = image_desc;
    storage_desc.format = RIN_GPU_FORMAT_R8_UNORM;
    storage_desc.usage = RIN_GPU_IMAGE_STORAGE | RIN_GPU_IMAGE_COPY_SOURCE;
    storage_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture2d(&device, &storage_desc,
                                       &storage_image) == RIN_GPU_OK);
    {
        RinGpuGraphicsBindingV1 binding;
        memset(&binding, 0, sizeof(binding));
        binding.abi_version = RIN_GPU_ABI_VERSION;
        binding.struct_size = sizeof(binding);
        binding.binding = 0u;
        binding.kind = RIN_SHADER_RESOURCE_STORAGE_IMAGE;
        binding.access = RIN_GPU_RESOURCE_WRITE;
        binding.resource = storage_image;
        CHECK(rindx_d3d11_create_graphics_bind_group(
                  &device, pipeline, &binding, 1u,
                  &graphics_bind_group) == RIN_GPU_OK);
    }
    transfer_desc = image_desc;
    transfer_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                          RIN_GPU_IMAGE_COPY_DESTINATION;
    transfer_desc.flags = RIN_GPU_IMAGE_CPU_VISIBLE |
                          RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture2d(&device, &transfer_desc,
                                       &transfer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_texture2d(&device, &transfer_desc,
                                       &transfer_destination) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_texture2d(&device, &transfer_desc,
                                       &subresource_destination) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_texture2d(&device, &transfer_desc,
                                       &clear_target) == RIN_GPU_OK);
    memset(&transfer_upload, 0, sizeof(transfer_upload));
    transfer_upload.abi_version = RIN_GPU_ABI_VERSION;
    transfer_upload.struct_size = sizeof(transfer_upload);
    transfer_upload.width = 2u;
    transfer_upload.height = 2u;
    transfer_upload.depth = 1u;
    CHECK(rindx_d3d11_upload_image(&device, transfer_source, &transfer_upload,
                                   transfer_source_pixels,
                                   sizeof(transfer_source_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d11_upload_image(&device, transfer_destination,
                                   &transfer_upload, zero_pixels,
                                   sizeof(zero_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d11_upload_image(&device, subresource_destination,
                                   &transfer_upload, zero_pixels,
                                   sizeof(zero_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d11_upload_image(&device, clear_target, &transfer_upload,
                                   zero_pixels, sizeof(zero_pixels)) == RIN_GPU_OK);
    memset(&transfer_buffer_desc, 0, sizeof(transfer_buffer_desc));
    transfer_buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    transfer_buffer_desc.struct_size = sizeof(transfer_buffer_desc);
    transfer_buffer_desc.size_bytes = 16u;
    transfer_buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                                 RIN_GPU_BUFFER_COPY_DESTINATION;
    transfer_buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d11_create_buffer(&device, &transfer_buffer_desc,
                                    &transfer_buffer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_buffer(&device, &transfer_buffer_desc,
                                    &transfer_buffer_destination) == RIN_GPU_OK);
    CHECK(rindx_d3d11_upload_buffer(&device, transfer_buffer_source, 0u,
                                    transfer_source_pixels,
                                    sizeof(transfer_source_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d11_upload_buffer(&device, transfer_buffer_destination, 0u,
                                    zero_pixels, sizeof(zero_pixels)) == RIN_GPU_OK);
    depth_desc = transfer_desc;
    depth_desc.format = RIN_GPU_FORMAT_D32_FLOAT_S8_UINT;
    depth_desc.usage = RIN_GPU_IMAGE_COPY_SOURCE |
                       RIN_GPU_IMAGE_COPY_DESTINATION |
                       RIN_GPU_IMAGE_DEPTH_STENCIL;
    depth_desc.flags = RIN_GPU_IMAGE_CPU_READABLE;
    CHECK(rindx_d3d11_create_texture2d(&device, &depth_desc, &depth_image) ==
          RIN_GPU_OK);
    mip_desc = transfer_desc;
    mip_desc.width = 4u;
    mip_desc.height = 4u;
    mip_desc.mip_levels = 3u;
    CHECK(rindx_d3d11_create_texture2d(&device, &mip_desc, &mip_image) ==
          RIN_GPU_OK);
    for (uint32_t pixel = 0u; pixel < sizeof(mip0_pixels); pixel += 4u) {
        mip0_pixels[pixel] = 100u;
        mip0_pixels[pixel + 1u] = 50u;
        mip0_pixels[pixel + 2u] = 25u;
        mip0_pixels[pixel + 3u] = 255u;
    }
    memset(&mip_upload, 0, sizeof(mip_upload));
    mip_upload.abi_version = RIN_GPU_ABI_VERSION;
    mip_upload.struct_size = sizeof(mip_upload);
    mip_upload.mip_level = 0u;
    mip_upload.width = 4u;
    mip_upload.height = 4u;
    mip_upload.depth = 1u;
    CHECK(rindx_d3d11_update_subresource_texture2d(
              &device, mip_image, &mip_upload, mip0_pixels,
              sizeof(mip0_pixels)) == RIN_GPU_OK);
    mip_upload.mip_level = 1u;
    mip_upload.width = 2u;
    mip_upload.height = 2u;
    CHECK(rindx_d3d11_update_subresource_texture2d(
              &device, mip_image, &mip_upload, mip1_pixels,
              sizeof(mip1_pixels)) == RIN_GPU_OK);
    mip_upload.mip_level = 2u;
    mip_upload.width = 1u;
    mip_upload.height = 1u;
    CHECK(rindx_d3d11_update_subresource_texture2d(
              &device, mip_image, &mip_upload, mip2_pixels,
              sizeof(mip2_pixels)) == RIN_GPU_OK);
    CHECK(rindx_d3d11_generate_mips(&context, mip_image) == RIN_GPU_OK);
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(rindx_d3d11_transition_image(&context, transfer_source,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d11_copy_resource(&context, transfer_destination,
                                    transfer_source) == RIN_GPU_OK);
    memset(&copy_region, 0, sizeof(copy_region));
    copy_region.abi_version = RIN_GPU_ABI_VERSION;
    copy_region.struct_size = sizeof(copy_region);
    copy_region.destination_x = 1u;
    copy_region.destination_y = 1u;
    copy_region.width = 1u;
    copy_region.height = 1u;
    copy_region.depth = 1u;
    CHECK(rindx_d3d11_copy_subresource_region(
              &context, subresource_destination, transfer_source,
              &copy_region) == RIN_GPU_OK);
    CHECK(rindx_d3d11_clear_render_target_view(
              &context, clear_target, 0.0f, 1.0f, 0.0f, 1.0f) == RIN_GPU_OK);
    memset(&buffer_clear, 0, sizeof(buffer_clear));
    buffer_clear.abi_version = RIN_GPU_ABI_VERSION;
    buffer_clear.struct_size = sizeof(buffer_clear);
    buffer_clear.size_bytes = 16u;
    buffer_clear.pattern = UINT32_C(0x01020304);
    CHECK(rindx_d3d11_copy_buffer(&context, transfer_buffer_destination, 0u,
                                  transfer_buffer_source, 0u, 16u) == RIN_GPU_OK);
    CHECK(rindx_d3d11_clear_buffer(&context, transfer_buffer_destination,
                                   &buffer_clear) == RIN_GPU_OK);
    CHECK(rindx_d3d11_clear_depth_stencil_view(
              &context, clear_target, RIN_GPU_IMAGE_CLEAR_DEPTH, 1.0f, 0u) !=
          RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, depth_image, RIN_GPU_IMAGE_STATE_UNDEFINED,
              RIN_GPU_IMAGE_STATE_COPY_DESTINATION) == RIN_GPU_OK);
    CHECK(rindx_d3d11_clear_depth_stencil_view(
              &context, depth_image,
              RIN_GPU_IMAGE_CLEAR_DEPTH | RIN_GPU_IMAGE_CLEAR_STENCIL,
              0.5f, 0x7fu) == RIN_GPU_OK);
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    CHECK(rindx_d3d11_transition_image(&context, transfer_destination,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(&context, subresource_destination,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(&context, clear_target,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(&context, depth_image,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    memset(&resolve, 0, sizeof(resolve));
    resolve.abi_version = RIN_GPU_ABI_VERSION;
    resolve.struct_size = sizeof(resolve);
    resolve.width = 1u;
    resolve.height = 1u;
    CHECK(rindx_d3d11_resolve_subresource(
              &context, transfer_destination, transfer_source, &resolve) !=
          RIN_GPU_OK);
    memset(&vertex_desc, 0, sizeof(vertex_desc));
    vertex_desc.abi_version = RIN_GPU_ABI_VERSION;
    vertex_desc.struct_size = sizeof(vertex_desc);
    vertex_desc.size_bytes = sizeof(float);
    vertex_desc.usage = RIN_GPU_BUFFER_VERTEX | RIN_GPU_BUFFER_COPY_DESTINATION;
    vertex_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d11_create_buffer(&device, &vertex_desc, &vertex_buffer) ==
          RIN_GPU_OK);
    {
        const float position = 0.0f;
        CHECK(rindx_d3d11_upload_buffer(&device, vertex_buffer, 0u, &position,
                                        sizeof(position)) == RIN_GPU_OK);
    }
    vertex_desc.flags = 0u;
    CHECK(rindx_d3d11_create_buffer(&device, &vertex_desc, &non_cpu_buffer) ==
          RIN_GPU_OK);
    memset(&index_desc, 0, sizeof(index_desc));
    index_desc.abi_version = RIN_GPU_ABI_VERSION;
    index_desc.struct_size = sizeof(index_desc);
    index_desc.size_bytes = 1u;
    index_desc.usage = RIN_GPU_BUFFER_INDEX | RIN_GPU_BUFFER_COPY_DESTINATION;
    index_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    CHECK(rindx_d3d11_create_buffer(&device, &index_desc, &index_buffer) ==
          RIN_GPU_OK);
    {
        const uint8_t index = 0u;
        CHECK(rindx_d3d11_upload_buffer(&device, index_buffer, 0u, &index,
                                        sizeof(index)) == RIN_GPU_OK);
    }
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_UNDEFINED;
    transition.after_state = RIN_GPU_IMAGE_STATE_COLOR_TARGET;
    CHECK(rindx_d3d11_transition_image(&context, image,
                                       transition.before_state,
                                       transition.after_state) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, sampled_image, RIN_GPU_IMAGE_STATE_COPY_DESTINATION,
              RIN_GPU_IMAGE_STATE_SHADER_READ) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, sample_target, RIN_GPU_IMAGE_STATE_UNDEFINED,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, storage_image, RIN_GPU_IMAGE_STATE_UNDEFINED,
              RIN_GPU_IMAGE_STATE_SHADER_READ) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_query(&context, occlusion_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_query(&context, timestamp_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_query(&context, pipeline_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_render_pass(&context, image, 0u, 1u,
                                        0.0f, 0.0f, 0.0f, 1.0f,
                                        1.0f) == RIN_GPU_OK);
    CHECK(rindx_d3d11_bind_graphics_resources(&context, graphics_bind_group) ==
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
    CHECK(rindx_d3d11_set_raster_state(&context, &raster_state) == RIN_GPU_OK);
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = pipeline;
    draw.color_target = image;
    draw.index_buffer = index_buffer;
    draw.index_format = RIN_GPU_INDEX_UINT8;
    draw.index_count = 1u;
    draw.instance_count = 2u;
    draw.vertex_count = 1u;
    draw.binding_count = 1u;
    draw.vertex_buffers[0].binding = 0u;
    draw.vertex_buffers[0].buffer = vertex_buffer;
    CHECK(rindx_d3d11_draw_indexed_instanced(&context, &draw) == RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_render_pass(&context, sample_target, 0u, 1u,
                                        0.0f, 0.0f, 0.0f, 1.0f,
                                        1.0f) == RIN_GPU_OK);
    CHECK(rindx_d3d11_bind_graphics_resources(&context, sample_bind_group) ==
          RIN_GPU_OK);
    sample_draw = draw;
    sample_draw.pipeline = sample_pipeline;
    sample_draw.color_target = sample_target;
    CHECK(rindx_d3d11_draw_indexed_instanced(&context, &sample_draw) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_end_query(&context, occlusion_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_end_query(&context, timestamp_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, storage_image, RIN_GPU_IMAGE_STATE_SHADER_READ,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
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
        CHECK(rindx_d3d11_dispatch(&context, &dispatch) == RIN_GPU_OK);
    }
    CHECK(rindx_d3d11_end_query(&context, pipeline_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(&context, image,
                                       RIN_GPU_IMAGE_STATE_COLOR_TARGET,
                                       RIN_GPU_IMAGE_STATE_COPY_SOURCE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, sample_target, RIN_GPU_IMAGE_STATE_COLOR_TARGET,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, present_target, RIN_GPU_IMAGE_STATE_UNDEFINED,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_render_pass(&context, present_target, 0u, 1u,
                                        0.25f, 0.5f, 0.75f, 1.0f, 1.0f) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &context, present_target, RIN_GPU_IMAGE_STATE_COLOR_TARGET,
              RIN_GPU_IMAGE_STATE_PRESENT) == RIN_GPU_OK);
    CHECK(rindx_d3d11_present(&context, present_target,
                              RIN_GPU_PRIMARY_DISPLAY) == RIN_GPU_OK);
    CHECK(rindx_d3d11_close_and_submit(&context, &fence_value) == RIN_GPU_OK);
    CHECK(rindx_d3d11_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
    CHECK(present_capture.calls == 1u);
    memset(&texture3d_readback, 0, sizeof(texture3d_readback));
    texture3d_readback.abi_version = RIN_GPU_ABI_VERSION;
    texture3d_readback.struct_size = sizeof(texture3d_readback);
    texture3d_readback.width = 2u;
    texture3d_readback.height = 2u;
    texture3d_readback.depth = 2u;
    CHECK(rindx_d3d11_readback_image(&device, texture3d, &texture3d_readback,
                                     texture3d_pixels,
                                     sizeof(texture3d_pixels)) == RIN_GPU_OK);
    CHECK(memcmp(texture3d_pixels, texture3d_source,
                 sizeof(texture3d_pixels)) == 0);
    memset(&readback, 0, sizeof(readback));
    readback.abi_version = RIN_GPU_ABI_VERSION;
    readback.struct_size = sizeof(readback);
    readback.width = 2u;
    readback.height = 2u;
    readback.depth = 1u;
    CHECK(rindx_d3d11_readback_image(&device, sample_target, &readback,
                                     sampled_pixels,
                                     sizeof(sampled_pixels)) == RIN_GPU_OK);
    CHECK(sampled_pixels[0u] >= 60u && sampled_pixels[1u] == 0u &&
          sampled_pixels[2u] == 0u && sampled_pixels[3u] == 255u);
    memset(&query_result, 0, sizeof(query_result));
    query_result.struct_size = sizeof(query_result);
    query_result.abi_version = RIN_GPU_ABI_VERSION;
    CHECK(rindx_d3d11_get_query_data(&device, occlusion_query, 0u,
                                     &query_result) == RIN_GPU_OK);
    CHECK(query_result.query_type == RIN_DX_D3D11_QUERY_OCCLUSION &&
          query_result.available != 0u && query_result.values[0] != 0u);
    memset(&query_result, 0, sizeof(query_result));
    query_result.struct_size = sizeof(query_result);
    query_result.abi_version = RIN_GPU_ABI_VERSION;
    CHECK(rindx_d3d11_get_query_data(&device, timestamp_query, 0u,
                                     &query_result) == RIN_GPU_OK);
    CHECK(query_result.query_type == RIN_DX_D3D11_QUERY_TIMESTAMP &&
          query_result.available != 0u && query_result.values[0] != 0u);
    memset(&query_result, 0, sizeof(query_result));
    query_result.struct_size = sizeof(query_result);
    query_result.abi_version = RIN_GPU_ABI_VERSION;
    CHECK(rindx_d3d11_get_query_data(&device, pipeline_query, 0u,
                                     &query_result) == RIN_GPU_OK);
    CHECK(query_result.query_type ==
              RIN_DX_D3D11_QUERY_PIPELINE_STATISTICS &&
          query_result.available != 0u &&
          query_result.values[RIN_GPU_PIPELINE_STAT_INPUT_ASSEMBLY_VERTICES] !=
              0u &&
          query_result.values[RIN_GPU_PIPELINE_STAT_DRAW_CALLS] >= 2u &&
          query_result.values[RIN_GPU_PIPELINE_STAT_DISPATCH_CALLS] >= 1u);
    memset(&predicate_context, 0, sizeof(predicate_context));
    CHECK(rindx_d3d11_create_context(&device, &predicate_context) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_set_predication(
              &predicate_context, timestamp_query, 1u) == RIN_GPU_ERROR_BUSY);
    CHECK(rindx_d3d11_set_predication(&predicate_context, occlusion_query, 1u) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &predicate_context, sample_target,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_render_pass(&predicate_context, sample_target, 0u,
                                        1u, 0.0f, 0.0f, 0.0f, 1.0f,
                                        1.0f) == RIN_GPU_OK);
    CHECK(rindx_d3d11_bind_graphics_resources(&predicate_context,
                                              sample_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d11_draw_indexed_instanced(&predicate_context, &sample_draw) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&predicate_context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &predicate_context, sample_target,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
    CHECK(rindx_d3d11_close_and_submit(&predicate_context, &fence_value) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_context(&predicate_context) == RIN_GPU_OK);
    memset(sampled_pixels, 0, sizeof(sampled_pixels));
    CHECK(rindx_d3d11_readback_image(&device, sample_target, &readback,
                                     sampled_pixels,
                                     sizeof(sampled_pixels)) == RIN_GPU_OK);
    CHECK(sampled_pixels[0u] >= 60u && sampled_pixels[1u] == 0u &&
          sampled_pixels[2u] == 0u && sampled_pixels[3u] == 255u);
    memset(&predicate_false_context, 0, sizeof(predicate_false_context));
    CHECK(rindx_d3d11_create_context(&device, &predicate_false_context) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_set_predication(&predicate_false_context, occlusion_query,
                                      0u) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &predicate_false_context, sample_target,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET) == RIN_GPU_OK);
    CHECK(rindx_d3d11_begin_render_pass(
              &predicate_false_context, sample_target, 0u, 1u, 0.0f, 0.0f,
              0.0f, 1.0f, 1.0f) == RIN_GPU_OK);
    CHECK(rindx_d3d11_bind_graphics_resources(&predicate_false_context,
                                              sample_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d11_draw_indexed_instanced(&predicate_false_context,
                                             &sample_draw) == RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&predicate_false_context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &predicate_false_context, sample_target,
              RIN_GPU_IMAGE_STATE_COLOR_TARGET,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
    CHECK(rindx_d3d11_close_and_submit(&predicate_false_context, &fence_value) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_context(&predicate_false_context) == RIN_GPU_OK);
    memset(sampled_pixels, 0, sizeof(sampled_pixels));
    CHECK(rindx_d3d11_readback_image(&device, sample_target, &readback,
                                     sampled_pixels,
                                     sizeof(sampled_pixels)) == RIN_GPU_OK);
    CHECK(sampled_pixels[0u] == 0u && sampled_pixels[1u] == 0u &&
          sampled_pixels[2u] == 0u && sampled_pixels[3u] == 255u);
    memset(&deferred_context, 0, sizeof(deferred_context));
    CHECK(rindx_d3d11_create_deferred_context(&device, &deferred_context) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &deferred_context, clear_target,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE,
              RIN_GPU_IMAGE_STATE_COPY_DESTINATION) == RIN_GPU_OK);
    CHECK(rindx_d3d11_clear_render_target_view(&deferred_context, clear_target,
                                               0.0f, 0.0f, 1.0f, 1.0f) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(
              &deferred_context, clear_target,
              RIN_GPU_IMAGE_STATE_COPY_DESTINATION,
              RIN_GPU_IMAGE_STATE_COPY_SOURCE) == RIN_GPU_OK);
    deferred_command_list = 0u;
    CHECK(rindx_d3d11_finish_command_list(&deferred_context,
                                          &deferred_command_list) ==
          RIN_GPU_OK);
    CHECK(deferred_command_list != 0u);
    CHECK(rindx_d3d11_destroy_context(&deferred_context) == RIN_GPU_OK);
    memset(&execute_context, 0, sizeof(execute_context));
    CHECK(rindx_d3d11_create_context(&device, &execute_context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_execute_command_list(&execute_context,
                                           deferred_command_list, 1u,
                                           &fence_value) ==
          RIN_GPU_ERROR_INVALID_ARGUMENT);
    CHECK(rindx_d3d11_execute_command_list(&execute_context,
                                           deferred_command_list, 0u,
                                           &fence_value) == RIN_GPU_OK);
    CHECK(rindx_d3d11_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_context(&execute_context) == RIN_GPU_OK);
    memset(sampled_pixels, 0, sizeof(sampled_pixels));
    CHECK(rindx_d3d11_readback_image(&device, clear_target, &readback,
                                     sampled_pixels,
                                     sizeof(sampled_pixels)) == RIN_GPU_OK);
    for (uint32_t pixel = 0u; pixel < sizeof(sampled_pixels); pixel += 4u)
        CHECK(sampled_pixels[pixel] == 0u &&
              sampled_pixels[pixel + 1u] == 0u &&
              sampled_pixels[pixel + 2u] == 255u &&
              sampled_pixels[pixel + 3u] == 255u);
    memset(&readback, 0, sizeof(readback));
    readback.abi_version = RIN_GPU_ABI_VERSION;
    readback.struct_size = sizeof(readback);
    readback.width = 2u;
    readback.height = 2u;
    readback.depth = 1u;
    CHECK(rindx_d3d11_readback_image(&device, image, &readback, pixels,
                                     sizeof(pixels)) == RIN_GPU_OK);
    CHECK(pixels[0] != 0u || pixels[1] != 0u || pixels[2] != 0u ||
          pixels[3] != 0u);
    storage_readback = readback;
    CHECK(rindx_d3d11_readback_image(&device, storage_image,
                                     &storage_readback, storage_pixels,
                                     sizeof(storage_pixels)) == RIN_GPU_OK);
    CHECK(storage_pixels[0] == 123u);
    CHECK(rindx_d3d11_readback_image(&device, transfer_destination, &readback,
                                     copied_pixels, sizeof(copied_pixels)) ==
          RIN_GPU_OK);
    CHECK(memcmp(copied_pixels, transfer_source_pixels,
                 sizeof(copied_pixels)) == 0);
    CHECK(rindx_d3d11_readback_image(&device, subresource_destination,
                                     &readback, subresource_pixels,
                                     sizeof(subresource_pixels)) == RIN_GPU_OK);
    CHECK(subresource_pixels[12u] == transfer_source_pixels[0u] &&
          subresource_pixels[13u] == transfer_source_pixels[1u] &&
          subresource_pixels[14u] == transfer_source_pixels[2u] &&
          subresource_pixels[15u] == transfer_source_pixels[3u]);
    CHECK(rindx_d3d11_readback_image(&device, clear_target, &readback,
                                     clear_pixels, sizeof(clear_pixels)) ==
          RIN_GPU_OK);
    for (uint32_t pixel = 0u; pixel < sizeof(clear_pixels); pixel += 4u)
        CHECK(clear_pixels[pixel] == 0u && clear_pixels[pixel + 1u] == 0u &&
              clear_pixels[pixel + 2u] == 255u && clear_pixels[pixel + 3u] ==
              255u);
    memset(&depth_readback, 0, sizeof(depth_readback));
    depth_readback.abi_version = RIN_GPU_ABI_VERSION;
    depth_readback.struct_size = sizeof(depth_readback);
    depth_readback.width = 2u;
    depth_readback.height = 2u;
    depth_readback.depth = 1u;
    CHECK(rindx_d3d11_readback_image(&device, depth_image, &depth_readback,
                                     depth_pixels, sizeof(depth_pixels)) ==
          RIN_GPU_OK);
    {
        float depth_value;
        memcpy(&depth_value, depth_pixels, sizeof(depth_value));
        CHECK(depth_value == 0.5f && depth_pixels[4u] == 0x7fu);
    }
    CHECK(rindx_d3d11_readback_buffer(&device, transfer_buffer_destination,
                                      0u, buffer_readback,
                                      sizeof(buffer_readback)) == RIN_GPU_OK);
    for (uint32_t offset = 0u; offset < sizeof(buffer_readback); offset += 4u)
        CHECK(buffer_readback[offset] == 0x04u &&
              buffer_readback[offset + 1u] == 0x03u &&
              buffer_readback[offset + 2u] == 0x02u &&
              buffer_readback[offset + 3u] == 0x01u);
    memset(&mapped, 0, sizeof(mapped));
    mapped.struct_size = sizeof(mapped);
    mapped.version = RIN_DX_D3D11_VERSION;
    CHECK(rindx_d3d11_map_buffer(&context, transfer_buffer_destination,
                                 RIN_DX_D3D11_MAP_READ_WRITE, 0u,
                                 &mapped) == RIN_GPU_OK);
    CHECK(mapped.data != NULL && mapped.size_bytes == sizeof(buffer_readback) &&
          mapped.row_pitch_bytes == sizeof(buffer_readback));
    ((uint8_t*)mapped.data)[0u] = 0xa5u;
    ((uint8_t*)mapped.data)[1u] = 0x5au;
    CHECK(rindx_d3d11_unmap_buffer(&context, transfer_buffer_destination,
                                   &mapped) == RIN_GPU_OK);
    CHECK(rindx_d3d11_readback_buffer(&device, transfer_buffer_destination,
                                      0u, buffer_readback,
                                      sizeof(buffer_readback)) == RIN_GPU_OK);
    CHECK(buffer_readback[0u] == 0xa5u && buffer_readback[1u] == 0x5au);
    memset(&mapped, 0, sizeof(mapped));
    mapped.struct_size = sizeof(mapped);
    mapped.version = RIN_DX_D3D11_VERSION;
    CHECK(rindx_d3d11_map_buffer(&context, non_cpu_buffer,
                                 RIN_DX_D3D11_MAP_READ, 0u,
                                 &mapped) == RIN_GPU_ERROR_STATE);
    memset(&mip_readback, 0, sizeof(mip_readback));
    mip_readback.abi_version = RIN_GPU_ABI_VERSION;
    mip_readback.struct_size = sizeof(mip_readback);
    mip_readback.mip_level = 1u;
    mip_readback.width = 2u;
    mip_readback.height = 2u;
    mip_readback.depth = 1u;
    CHECK(rindx_d3d11_readback_image(&device, mip_image, &mip_readback,
                                     mip_readback_pixels,
                                     sizeof(mip_readback_pixels)) == RIN_GPU_OK);
    CHECK(mip_readback_pixels[0u] == 100u &&
          mip_readback_pixels[1u] == 50u && mip_readback_pixels[2u] == 25u &&
          mip_readback_pixels[3u] == 255u);
    CHECK(rindx_d3d11_destroy_context(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, occlusion_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, timestamp_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, pipeline_query) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, compute_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sample_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, graphics_bind_group) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sample_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, depth_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sampler) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, compute_pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, compute_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sample_fragment_shader) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, fragment_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, vertex_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, non_cpu_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, index_buffer) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, transfer_source) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, transfer_destination) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, subresource_destination) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, clear_target) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, storage_image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sampled_image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, sample_target) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, mip_image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, transfer_buffer_source) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, transfer_buffer_destination) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, depth_image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, present_target) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, texture1d) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, texture3d) == RIN_GPU_OK);
    CHECK(rindx_d3d11_get_device_removed_reason(&device) == RIN_GPU_OK);
    CHECK(rindx_d3d11_mark_device_removed(&device) == RIN_GPU_OK);
    CHECK(rindx_d3d11_get_device_removed_reason(&device) ==
          RIN_GPU_ERROR_DEVICE_LOST);
    CHECK(rindx_d3d11_destroy_device(&device) == RIN_GPU_OK);
    return 0;
}
