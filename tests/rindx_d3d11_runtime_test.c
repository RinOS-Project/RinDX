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
    static const float position[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint32_t index;
    memset(shader, 0, sizeof(*shader));
    shader->header.magic = RIN_SHADER_MAGIC;
    shader->header.version = RIN_SHADER_IR_VERSION;
    shader->header.header_size = sizeof(shader->header);
    shader->header.stage = RIN_SHADER_STAGE_VERTEX;
    shader->header.instruction_count = 9u;
    shader->header.register_count = 4u;
    shader->header.output_count = 4u;
    shader->header.total_size = sizeof(shader->header) +
                                9u * sizeof(shader->instructions[0]);
    for (index = 0u; index < 4u; ++index) {
        instruction(&shader->instructions[index], RIN_SHADER_OP_CONST_F32,
                    (uint16_t)index, RIN_SHADER_UNUSED,
                    f32_bits(position[index]));
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
    surface->handle_secret = UINT64_C(0x4453443131534f46);
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
    RinGpuGraphicsPipelineNativeDescV2 pipeline_desc;
    RinGpuImageDescV1 image_desc;
    RinGpuImageTransitionV1 transition;
    RinGpuDrawV1 draw;
    RinGpuImageReadbackV1 readback;
    RinGpuHandle vertex_shader = 0u;
    RinGpuHandle fragment_shader = 0u;
    RinGpuHandle pipeline = 0u;
    RinGpuHandle image = 0u;
    uint8_t pixels[16u] = {0};
    uint64_t fence_value = 0u;
    const uint32_t feature_level = RIN_DX_D3D11_FEATURE_LEVEL_11_0;
    int create_result;

    make_surface(&surface);
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
    make_vertex(&vertex);
    make_fragment(&fragment);
    CHECK(rindx_d3d11_create_shader(&device, &vertex, vertex.header.total_size,
                                    &vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_create_shader(&device, &fragment,
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
    pipeline_desc.base.cull_mode = RIN_GPU_CULL_NONE;
    pipeline_desc.base.front_face = RIN_GPU_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_desc.base.struct_size = sizeof(pipeline_desc);
    CHECK(rindx_d3d11_create_graphics_pipeline(
              &device, &pipeline_desc, NULL, 0u, NULL, 0u, NULL, 0u,
              &pipeline) == RIN_GPU_OK);

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
    CHECK(rindx_d3d11_begin_render_pass(&context, image, 0u, 1u,
                                        0.0f, 0.0f, 0.0f, 1.0f,
                                        1.0f) == RIN_GPU_OK);
    memset(&draw, 0, sizeof(draw));
    draw.abi_version = RIN_GPU_ABI_VERSION;
    draw.struct_size = sizeof(draw);
    draw.pipeline = pipeline;
    draw.color_target = image;
    draw.vertex_count = 1u;
    draw.instance_count = 1u;
    CHECK(rindx_d3d11_draw(&context, &draw) == RIN_GPU_OK);
    CHECK(rindx_d3d11_end_render_pass(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_transition_image(&context, image,
                                       RIN_GPU_IMAGE_STATE_COLOR_TARGET,
                                       RIN_GPU_IMAGE_STATE_COPY_SOURCE) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_close_and_submit(&context, &fence_value) == RIN_GPU_OK);
    CHECK(rindx_d3d11_wait(&device, fence_value, RIN_GPU_TIMEOUT_INFINITE) ==
          RIN_GPU_OK);
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
    CHECK(rindx_d3d11_destroy_context(&context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, pipeline) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, fragment_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, vertex_shader) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_object(&device, image) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_device(&device) == RIN_GPU_OK);
    return 0;
}
