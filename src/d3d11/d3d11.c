/* SPDX-License-Identifier: MIT */
#include <rindx/d3d11.h>

#include <string.h>

#define RIN_DX_D3D11_STATE_READY 1u
#define RIN_DX_D3D11_STATE_CLOSED 2u
#define RIN_DX_D3D11_DRAW_FLAG_MASK 0u

static int device_valid(const RinDxD3d11Device* device)
{
    return device && device->struct_size == sizeof(*device) &&
           device->version == RIN_DX_D3D11_VERSION &&
           device->runtime && device->queue != 0u && device->fence != 0u &&
           device->feature_level == RIN_DX_D3D11_FEATURE_LEVEL_11_0 &&
           device->state == RIN_DX_D3D11_STATE_READY;
}

static int context_valid(const RinDxD3d11Context* context)
{
    return context && context->struct_size == sizeof(*context) &&
           context->version == RIN_DX_D3D11_VERSION &&
           device_valid(context->device) && context->command_list != 0u &&
           context->state == RIN_DX_D3D11_STATE_READY;
}

static int feature_level_requested(const uint32_t* levels, uint32_t count)
{
    uint32_t index;
    if (count == 0u) return 1;
    if (!levels || count > 16u) return 0;
    for (index = 0u; index < count; ++index)
        if (levels[index] == RIN_DX_D3D11_FEATURE_LEVEL_11_0) return 1;
    return 0;
}

static int create_command_list(RinDxD3d11Context* context,
                               RinGpuHandle* command_list_out)
{
    RinGpuCommandListDescV1 descriptor;
    if (!context || !command_list_out) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    return ringpu_runtime_create_command_list(context->device->runtime,
                                              &descriptor,
                                              command_list_out);
}

int rindx_d3d11_create_device(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    RinDxD3d11Device* device_out)
{
    RinGpuQueueDescV1 queue_desc;
    int result;
    if (!surface || !device_out || !feature_level_requested(
            requested_feature_levels, feature_level_count))
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(device_out, 0, sizeof(*device_out));
    result = ringpu_runtime_software_surface_create(surface,
                                                    &device_out->runtime);
    if (result != RIN_GPU_OK) return result;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.abi_version = RIN_GPU_ABI_VERSION;
    queue_desc.struct_size = sizeof(queue_desc);
    queue_desc.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    result = ringpu_runtime_create_queue(device_out->runtime, &queue_desc,
                                         &device_out->queue);
    if (result != RIN_GPU_OK) goto fail_runtime;
    result = ringpu_runtime_create_fence(device_out->runtime, 0u,
                                         &device_out->fence);
    if (result != RIN_GPU_OK) goto fail_queue;
    device_out->struct_size = sizeof(*device_out);
    device_out->version = RIN_DX_D3D11_VERSION;
    device_out->feature_level = RIN_DX_D3D11_FEATURE_LEVEL_11_0;
    device_out->state = RIN_DX_D3D11_STATE_READY;
    return RIN_GPU_OK;
fail_queue:
    (void)ringpu_runtime_destroy_object(device_out->runtime,
                                        device_out->queue);
fail_runtime:
    ringpu_runtime_destroy(device_out->runtime);
    memset(device_out, 0, sizeof(*device_out));
    return result;
}

int rindx_d3d11_destroy_device(RinDxD3d11Device* device)
{
    int result;
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_destroy_object(device->runtime, device->fence);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_destroy_object(device->runtime, device->queue);
    if (result != RIN_GPU_OK) return result;
    ringpu_runtime_destroy(device->runtime);
    memset(device, 0, sizeof(*device));
    return RIN_GPU_OK;
}

int rindx_d3d11_create_context(RinDxD3d11Device* device,
                               RinDxD3d11Context* context_out)
{
    int result;
    if (!device_valid(device) || !context_out) return RIN_GPU_ERROR_STATE;
    memset(context_out, 0, sizeof(*context_out));
    context_out->device = device;
    result = create_command_list(context_out, &context_out->command_list);
    if (result != RIN_GPU_OK) {
        memset(context_out, 0, sizeof(*context_out));
        return result;
    }
    context_out->struct_size = sizeof(*context_out);
    context_out->version = RIN_DX_D3D11_VERSION;
    context_out->state = RIN_DX_D3D11_STATE_READY;
    return RIN_GPU_OK;
}

int rindx_d3d11_destroy_context(RinDxD3d11Context* context)
{
    int result;
    if (!context_valid(context) || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_destroy_object(context->device->runtime,
                                           context->command_list);
    if (result != RIN_GPU_OK) return result;
    if (context->pending_command_list != 0u) {
        result = ringpu_runtime_destroy_object(
            context->device->runtime, context->pending_command_list);
        if (result != RIN_GPU_OK) return result;
    }
    memset(context, 0, sizeof(*context));
    return RIN_GPU_OK;
}

int rindx_d3d11_create_buffer(RinDxD3d11Device* device,
                              const RinGpuBufferDescV1* descriptor,
                              RinGpuHandle* buffer_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_buffer(device->runtime, descriptor,
                                        buffer_out);
}

int rindx_d3d11_create_texture2d(RinDxD3d11Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    if (!descriptor || descriptor->dimension != RIN_GPU_IMAGE_DIMENSION_2D)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_create_image(device->runtime, descriptor, image_out);
}

int rindx_d3d11_create_shader(RinDxD3d11Device* device, const void* rin_shader,
                              uint64_t shader_size,
                              RinGpuHandle* shader_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_shader_module(device->runtime, rin_shader,
                                               shader_size, shader_out);
}

int rindx_d3d11_create_graphics_pipeline(
    RinDxD3d11Device* device, const RinGpuGraphicsPipelineNativeDescV2* desc,
    const RinGpuVertexAttributeV2* attributes, uint32_t attribute_count,
    const RinGpuVertexBufferLayoutV1* vertex_bindings,
    uint32_t vertex_binding_count, const RinGpuVaryingV1* varyings,
    uint32_t varying_count, RinGpuHandle* pipeline_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_graphics_pipeline_native_vertex_bindings_v2(
        device->runtime, desc, attributes, attribute_count, vertex_bindings,
        vertex_binding_count, varyings, varying_count, pipeline_out);
}

int rindx_d3d11_create_compute_pipeline(
    RinDxD3d11Device* device, RinGpuHandle shader,
    RinGpuHandle* pipeline_out)
{
    RinGpuComputePipelineDescV1 descriptor;
    if (!device_valid(device) || shader == 0u || !pipeline_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.shader_module = shader;
    return ringpu_runtime_create_compute_pipeline(device->runtime, &descriptor,
                                                  pipeline_out);
}

int rindx_d3d11_create_compute_bind_group(
    RinDxD3d11Device* device, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_compute_bind_group(
        device->runtime, pipeline, bindings, binding_count, bind_group_out);
}

int rindx_d3d11_destroy_object(RinDxD3d11Device* device, RinGpuHandle object)
{
    if (!device_valid(device) || object == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_destroy_object(device->runtime, object);
}

int rindx_d3d11_upload_buffer(RinDxD3d11Device* device, RinGpuHandle buffer,
                             uint64_t offset, const void* source,
                             uint64_t size_bytes)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_upload_buffer(device->runtime, buffer, offset,
                                        source, size_bytes);
}

int rindx_d3d11_upload_image(RinDxD3d11Device* device, RinGpuHandle image,
                             const RinGpuImageUploadV1* upload,
                             const void* source, uint64_t source_size)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_upload_image(device->runtime, image, upload, source,
                                       source_size);
}

int rindx_d3d11_transition_image(
    RinDxD3d11Context* context, RinGpuHandle image,
    uint32_t before_state, uint32_t after_state)
{
    RinGpuImageTransitionV1 transition;
    if (!context_valid(context) || image == 0u || before_state == after_state)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = before_state;
    transition.after_state = after_state;
    return ringpu_runtime_command_transition_image(
        context->device->runtime, context->command_list, image, &transition);
}

int rindx_d3d11_begin_render_pass(
    RinDxD3d11Context* context, RinGpuHandle color_target,
    RinGpuHandle depth_target, uint32_t clear_color,
    float clear_red, float clear_green, float clear_blue, float clear_alpha,
    float clear_depth)
{
    int result;
    if (!context_valid(context) || color_target == 0u ||
        clear_color > 1u || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (depth_target == 0u) {
        RinGpuRenderPassDescV1 pass;
        memset(&pass, 0, sizeof(pass));
        pass.abi_version = RIN_GPU_ABI_VERSION;
        pass.struct_size = sizeof(pass);
        pass.color_target = color_target;
        pass.load_op = clear_color ? RIN_GPU_RENDER_CLEAR : RIN_GPU_RENDER_LOAD;
        pass.store_op = RIN_GPU_RENDER_STORE;
        pass.clear_red = clear_red;
        pass.clear_green = clear_green;
        pass.clear_blue = clear_blue;
        pass.clear_alpha = clear_alpha;
        pass.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
        result = ringpu_runtime_command_begin_render_pass(
            context->device->runtime, context->command_list, &pass);
    } else {
        RinGpuRenderPassDepthDescV1 pass;
        memset(&pass, 0, sizeof(pass));
        pass.abi_version = RIN_GPU_ABI_VERSION;
        pass.struct_size = sizeof(pass);
        pass.color_target = color_target;
        pass.depth_target = depth_target;
        pass.color_load_op = clear_color ? RIN_GPU_RENDER_CLEAR
                                         : RIN_GPU_RENDER_LOAD;
        pass.color_store_op = RIN_GPU_RENDER_STORE;
        pass.depth_load_op = RIN_GPU_RENDER_CLEAR;
        pass.depth_store_op = RIN_GPU_RENDER_STORE;
        pass.clear_red = clear_red;
        pass.clear_green = clear_green;
        pass.clear_blue = clear_blue;
        pass.clear_alpha = clear_alpha;
        pass.clear_depth = clear_depth;
        pass.color_write_mask = RIN_GPU_COLOR_WRITE_ALL;
        result = ringpu_runtime_command_begin_render_pass_depth(
            context->device->runtime, context->command_list, &pass);
    }
    if (result != RIN_GPU_OK) return result;
    context->active_color_target = color_target;
    context->active_depth_target = depth_target;
    context->render_pass_active = 1u;
    return RIN_GPU_OK;
}

int rindx_d3d11_set_raster_state(RinDxD3d11Context* context,
                                 const RinGpuRasterStateV1* state)
{
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_set_raster_state(
        context->device->runtime, context->command_list, state);
}

int rindx_d3d11_bind_graphics_resources(RinDxD3d11Context* context,
                                        RinGpuHandle bind_group)
{
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_bind_graphics_resources(
        context->device->runtime, context->command_list, bind_group);
}

int rindx_d3d11_draw(RinDxD3d11Context* context, const RinGpuDrawV1* draw)
{
    if (!context_valid(context) || !draw || draw->flags != RIN_DX_D3D11_DRAW_FLAG_MASK)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw(context->device->runtime,
                                       context->command_list, draw);
}

int rindx_d3d11_draw_vertices(RinDxD3d11Context* context,
                              const RinGpuDrawVerticesV2* draw)
{
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_draw_vertices_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_draw_indexed(RinDxD3d11Context* context,
                             const RinGpuDrawIndexedV2* draw)
{
    if (!context_valid(context) || !draw || draw->instance_count != 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw_indexed_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_draw_indexed_instanced(
    RinDxD3d11Context* context, const RinGpuDrawIndexedV2* draw)
{
    if (!context_valid(context) || !draw || draw->instance_count <= 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw_indexed_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_dispatch(RinDxD3d11Context* context,
                         const RinGpuDispatchV1* dispatch)
{
    if (!context_valid(context) || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_dispatch(context->device->runtime,
                                           context->command_list, dispatch);
}

int rindx_d3d11_end_render_pass(RinDxD3d11Context* context)
{
    int result;
    if (!context_valid(context) || context->render_pass_active == 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_command_end_render_pass(
        context->device->runtime, context->command_list);
    if (result != RIN_GPU_OK) return result;
    context->active_color_target = 0u;
    context->active_depth_target = 0u;
    context->render_pass_active = 0u;
    return RIN_GPU_OK;
}

int rindx_d3d11_close_and_submit(RinDxD3d11Context* context,
                                uint64_t* fence_value_out)
{
    RinGpuSubmitInfoV1 submit;
    RinGpuHandle closed_list;
    RinGpuHandle replacement;
    uint64_t value;
    int result;
    if (!context_valid(context) || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    closed_list = context->command_list;
    result = ringpu_runtime_command_list_close(context->device->runtime,
                                               closed_list);
    if (result != RIN_GPU_OK) return result;
    value = context->device->submission_value + 1u;
    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = closed_list;
    submit.signal_fence = context->device->fence;
    submit.signal_value = value;
    result = ringpu_runtime_queue_submit(context->device->runtime,
                                         context->device->queue, &submit);
    if (result != RIN_GPU_OK) {
        context->state = RIN_DX_D3D11_STATE_CLOSED;
        context->pending_command_list = closed_list;
        return result;
    }
    result = create_command_list(context, &replacement);
    if (result != RIN_GPU_OK) {
        context->state = RIN_DX_D3D11_STATE_CLOSED;
        context->pending_command_list = closed_list;
        return result;
    }
    context->device->submission_value = value;
    context->pending_command_list = closed_list;
    context->command_list = replacement;
    if (fence_value_out) *fence_value_out = value;
    return RIN_GPU_OK;
}

int rindx_d3d11_wait(RinDxD3d11Device* device, uint64_t fence_value,
                     uint64_t timeout_ns)
{
    if (!device_valid(device) || fence_value == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_wait_fence(device->runtime, device->fence,
                                     fence_value, timeout_ns);
}

int rindx_d3d11_readback_image(
    RinDxD3d11Device* device, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_readback_image(device->runtime, image, readback,
                                         destination, destination_size);
}
