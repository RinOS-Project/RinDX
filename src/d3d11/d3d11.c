/* SPDX-License-Identifier: MIT */
#include <rindx/d3d11.h>

#include <stdlib.h>
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
           context->deferred_context <= 1u && context->reserved0 == 0u &&
           context->multithread_protected <= 1u &&
           context->multithread_lock <= 1u &&
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
    if (ringpu_runtime_device_lost(device->runtime)) {
        ringpu_runtime_destroy(device->runtime);
        memset(device, 0, sizeof(*device));
        return RIN_GPU_OK;
    }
    result = ringpu_runtime_destroy_object(device->runtime, device->fence);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_destroy_object(device->runtime, device->queue);
    if (result != RIN_GPU_OK) return result;
    ringpu_runtime_destroy(device->runtime);
    memset(device, 0, sizeof(*device));
    return RIN_GPU_OK;
}

int rindx_d3d11_create_device_and_swapchain(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    const RinGpuDxgiSwapchainDescV1* swapchain_desc,
    const RinGpuDxgiWindowOwnerV1* window_owner,
    const RinGpuPresentationBackendV1* presentation_backend,
    RinDxD3d11Device* device_out,
    RinGpuDxgiSwapchainRuntime* swapchain_out)
{
    int result;
    int swapchain_result;
    if (!surface || !swapchain_desc || !window_owner ||
        !presentation_backend || !device_out || !swapchain_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(swapchain_out, 0, sizeof(*swapchain_out));
    result = rindx_d3d11_create_device(surface, requested_feature_levels,
                                       feature_level_count, device_out);
    if (result != RIN_GPU_OK) return result;
    swapchain_result = rin_gpu_dxgi_swapchain_runtime_init(
        swapchain_out, swapchain_desc, window_owner, presentation_backend);
    if (swapchain_result != RIN_GPU_DXGI_SWAPCHAIN_OK) {
        (void)rindx_d3d11_destroy_device(device_out);
        memset(swapchain_out, 0, sizeof(*swapchain_out));
        return RIN_GPU_ERROR_BACKEND;
    }
    return RIN_GPU_OK;
}

int rindx_d3d11_get_device_removed_reason(
    const RinDxD3d11Device* device)
{
    if (!device || device->struct_size != sizeof(*device) ||
        device->version != RIN_DX_D3D11_VERSION || !device->runtime)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_device_lost(device->runtime)
               ? RIN_GPU_ERROR_DEVICE_LOST
               : RIN_GPU_OK;
}

RinDxgiHresult rindx_d3d11_get_device_removed_reason_hresult(
    const RinDxD3d11Device* device)
{
    int result = rindx_d3d11_get_device_removed_reason(device);
    if (result == RIN_GPU_ERROR_INVALID_ARGUMENT)
        return RIN_DXGI_E_INVALIDARG;
    if (result == RIN_GPU_ERROR_DEVICE_LOST)
        return RIN_DXGI_ERROR_DEVICE_REMOVED;
    return result == RIN_GPU_OK ? RIN_DXGI_S_OK : RIN_DXGI_E_FAIL;
}

int rindx_d3d11_mark_device_removed(RinDxD3d11Device* device)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    ringpu_runtime_mark_device_lost(device->runtime);
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

int rindx_d3d11_create_deferred_context(RinDxD3d11Device* device,
                                        RinDxD3d11Context* context_out)
{
    int result = rindx_d3d11_create_context(device, context_out);
    if (result != RIN_GPU_OK) return result;
    context_out->deferred_context = 1u;
    return RIN_GPU_OK;
}

int rindx_d3d11_destroy_context(RinDxD3d11Context* context)
{
    int result;
    if (!context_valid(context) || context->render_pass_active != 0u ||
        context->mapped_buffer != 0u || context->multithread_lock != 0u)
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

int rindx_d3d11_set_multithread_protected(RinDxD3d11Context* context,
                                          uint32_t enabled)
{
    if (!context_valid(context) || enabled > 1u ||
        context->multithread_lock != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    context->multithread_protected = enabled;
    return RIN_GPU_OK;
}

int rindx_d3d11_enter_multithread(RinDxD3d11Context* context)
{
    uint32_t expected = 0u;
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    if (context->multithread_protected == 0u) return RIN_GPU_OK;
    if (!__atomic_compare_exchange_n(&context->multithread_lock, &expected, 1u,
                                     0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return RIN_GPU_ERROR_BUSY;
    return RIN_GPU_OK;
}

int rindx_d3d11_leave_multithread(RinDxD3d11Context* context)
{
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    if (context->multithread_protected == 0u ||
        __atomic_exchange_n(&context->multithread_lock, 0u,
                            __ATOMIC_RELEASE) == 0u)
        return RIN_GPU_ERROR_STATE;
    return RIN_GPU_OK;
}

int rindx_d3d11_finish_command_list(RinDxD3d11Context* context,
                                    RinGpuHandle* command_list_out)
{
    RinGpuHandle closed_list;
    RinGpuHandle replacement;
    int result;

    if (command_list_out) *command_list_out = 0u;
    if (!context_valid(context) || context->deferred_context == 0u ||
        !command_list_out || context->render_pass_active != 0u ||
        context->mapped_buffer != 0u || context->predicate_query != 0u)
        return RIN_GPU_ERROR_STATE;
    closed_list = context->command_list;
    result = ringpu_runtime_command_list_close(context->device->runtime,
                                               closed_list);
    if (result != RIN_GPU_OK) return result;
    result = create_command_list(context, &replacement);
    if (result != RIN_GPU_OK) {
        context->state = RIN_DX_D3D11_STATE_CLOSED;
        context->pending_command_list = closed_list;
        return result;
    }
    context->command_list = replacement;
    *command_list_out = closed_list;
    return RIN_GPU_OK;
}

int rindx_d3d11_execute_command_list(RinDxD3d11Context* context,
                                     RinGpuHandle command_list,
                                     uint32_t restore_state,
                                     uint64_t* fence_value_out)
{
    RinGpuSubmitInfoV1 submit;
    uint64_t value;
    int result;

    if (fence_value_out) *fence_value_out = 0u;
    if (!context_valid(context) || context->deferred_context != 0u ||
        command_list == 0u || restore_state != 0u ||
        context->pending_command_list != 0u || !fence_value_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    value = context->device->submission_value + 1u;
    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = command_list;
    submit.signal_fence = context->device->fence;
    submit.signal_value = value;
    result = ringpu_runtime_queue_submit(context->device->runtime,
                                         context->device->queue, &submit);
    if (result != RIN_GPU_OK) return result;
    context->device->submission_value = value;
    context->pending_command_list = command_list;
    *fence_value_out = value;
    return RIN_GPU_OK;
}

static int d3d11_query_type_valid(uint32_t query_type)
{
    return query_type == RIN_DX_D3D11_QUERY_TIMESTAMP ||
           query_type == RIN_DX_D3D11_QUERY_OCCLUSION ||
           query_type == RIN_DX_D3D11_QUERY_PIPELINE_STATISTICS;
}

int rindx_d3d11_create_query(RinDxD3d11Device* device, uint32_t query_type,
                             RinGpuHandle* query_out)
{
    RinGpuQueryDescV1 descriptor;
    if (!device_valid(device) || !d3d11_query_type_valid(query_type) ||
        !query_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.query_type = query_type;
    return ringpu_runtime_create_query(device->runtime, &descriptor,
                                       query_out);
}

int rindx_d3d11_begin_query(RinDxD3d11Context* context, RinGpuHandle query)
{
    if (!context_valid(context) || query == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_begin_query(context->device->runtime,
                                              context->command_list, query);
}

int rindx_d3d11_end_query(RinDxD3d11Context* context, RinGpuHandle query)
{
    if (!context_valid(context) || query == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_end_query(context->device->runtime,
                                            context->command_list, query);
}

int rindx_d3d11_reset_query(RinDxD3d11Context* context, RinGpuHandle query)
{
    if (!context_valid(context) || query == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_reset_query(context->device->runtime,
                                              context->command_list, query);
}

int rindx_d3d11_get_query_data(RinDxD3d11Device* device,
                               RinGpuHandle query, uint32_t flags,
                               RinGpuQueryResultV1* result)
{
    if (!device_valid(device) || query == 0u ||
        (flags & ~RIN_DX_D3D11_QUERY_RESULT_WAIT) != 0u || !result)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_get_query_result(device->runtime, query, flags,
                                           result);
}

int rindx_d3d11_set_predication(RinDxD3d11Context* context,
                                RinGpuHandle query, uint32_t predicate_value)
{
    RinGpuQueryResultV1 result;

    if (!context_valid(context) || predicate_value > 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (query == 0u) {
        context->predicate_query = 0u;
        context->predicate_value = 0u;
        context->predication_enabled = 0u;
        return RIN_GPU_OK;
    }
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = RIN_GPU_ABI_VERSION;
    if (ringpu_runtime_get_query_result(context->device->runtime, query, 0u,
                                        &result) != RIN_GPU_OK ||
        result.query_type != RIN_DX_D3D11_QUERY_OCCLUSION ||
        result.available == 0u)
        return RIN_GPU_ERROR_BUSY;
    context->predicate_query = query;
    context->predicate_value = predicate_value;
    context->predication_enabled = 1u;
    return RIN_GPU_OK;
}

static int d3d11_predication_allows(RinDxD3d11Context* context,
                                    uint32_t* allowed_out)
{
    RinGpuQueryResultV1 result;
    int status;

    if (!context || !allowed_out) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    *allowed_out = 1u;
    if (context->predication_enabled == 0u) return RIN_GPU_OK;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = RIN_GPU_ABI_VERSION;
    status = ringpu_runtime_get_query_result(
        context->device->runtime, context->predicate_query, 0u, &result);
    if (status != RIN_GPU_OK) return status;
    if (result.query_type != RIN_DX_D3D11_QUERY_OCCLUSION ||
        result.available == 0u)
        return RIN_GPU_ERROR_BUSY;
    *allowed_out =
        ((result.values[0] != 0u) == (context->predicate_value != 0u)) ? 1u : 0u;
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

int rindx_d3d11_create_texture1d(RinDxD3d11Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    if (!descriptor || descriptor->dimension != RIN_GPU_IMAGE_DIMENSION_1D ||
        descriptor->height != 1u || descriptor->depth != 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_create_image(device->runtime, descriptor, image_out);
}

int rindx_d3d11_create_texture3d(RinDxD3d11Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    if (!descriptor || descriptor->dimension != RIN_GPU_IMAGE_DIMENSION_3D ||
        descriptor->array_layers != 1u)
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

int rindx_d3d11_create_sampler(
    RinDxD3d11Device* device, const RinGpuSamplerDescV1* descriptor,
    RinGpuHandle* sampler_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_sampler(device->runtime, descriptor,
                                         sampler_out);
}

int rindx_d3d11_create_graphics_bind_group(
    RinDxD3d11Device* device, RinGpuHandle pipeline,
    const RinGpuGraphicsBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_graphics_bind_group_typed(
        device->runtime, pipeline, bindings, binding_count, bind_group_out);
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

int rindx_d3d11_update_subresource_buffer(
    RinDxD3d11Device* device, RinGpuHandle buffer, uint64_t offset,
    const void* source, uint64_t size_bytes)
{
    return rindx_d3d11_upload_buffer(device, buffer, offset, source,
                                     size_bytes);
}

int rindx_d3d11_update_subresource_texture2d(
    RinDxD3d11Device* device, RinGpuHandle image,
    const RinGpuImageUploadV1* upload, const void* source,
    uint64_t source_size)
{
    return rindx_d3d11_upload_image(device, image, upload, source, source_size);
}

int rindx_d3d11_readback_buffer(
    RinDxD3d11Device* device, RinGpuHandle buffer, uint64_t source_offset,
    void* destination, uint64_t size_bytes)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_readback_buffer(device->runtime, buffer,
                                          source_offset, destination,
                                          size_bytes);
}

static int d3d11_map_type_valid(uint32_t map_type)
{
    return map_type == RIN_DX_D3D11_MAP_READ ||
        map_type == RIN_DX_D3D11_MAP_WRITE ||
        map_type == RIN_DX_D3D11_MAP_READ_WRITE ||
        map_type == RIN_DX_D3D11_MAP_WRITE_DISCARD ||
        map_type == RIN_DX_D3D11_MAP_WRITE_NO_OVERWRITE;
}

static int d3d11_map_type_reads(uint32_t map_type)
{
    return map_type == RIN_DX_D3D11_MAP_READ ||
        map_type == RIN_DX_D3D11_MAP_READ_WRITE;
}

static int d3d11_map_type_writes(uint32_t map_type)
{
    return map_type != RIN_DX_D3D11_MAP_READ;
}

int rindx_d3d11_map_buffer(RinDxD3d11Context* context, RinGpuHandle buffer,
                           uint32_t map_type, uint32_t flags,
                           RinDxD3d11MappedResource* mapped_out)
{
    RinGpuBufferInfoV1 info;
    void* data;
    int result;

    if (!context_valid(context) || buffer == 0u ||
        !d3d11_map_type_valid(map_type) || flags != 0u || !mapped_out ||
        mapped_out->struct_size != sizeof(*mapped_out) ||
        mapped_out->version != RIN_DX_D3D11_VERSION ||
        context->mapped_buffer != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (context->pending_command_list != 0u) {
        result = ringpu_runtime_command_list_reset(
            context->device->runtime, context->pending_command_list);
        if (result != RIN_GPU_OK) return result;
        context->pending_command_list = 0u;
    }
    memset(&info, 0, sizeof(info));
    result = ringpu_runtime_get_buffer_info(context->device->runtime, buffer,
                                             &info);
    if (result != RIN_GPU_OK) return result;
    if ((info.flags & RIN_GPU_BUFFER_CPU_VISIBLE) == 0u)
        return RIN_GPU_ERROR_STATE;
    if (d3d11_map_type_reads(map_type) &&
        (info.usage & RIN_GPU_BUFFER_COPY_SOURCE) == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (d3d11_map_type_writes(map_type) &&
        (info.usage & RIN_GPU_BUFFER_COPY_DESTINATION) == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (info.size_bytes > (uint64_t)SIZE_MAX)
        return RIN_GPU_ERROR_LIMIT;
    data = calloc(1u, (size_t)info.size_bytes);
    if (!data) return RIN_GPU_ERROR_NO_MEMORY;
    if (d3d11_map_type_reads(map_type)) {
        result = ringpu_runtime_readback_buffer(
            context->device->runtime, buffer, 0u, data, info.size_bytes);
        if (result != RIN_GPU_OK) {
            free(data);
            return result;
        }
    }
    context->mapped_buffer = buffer;
    context->mapped_data = data;
    context->mapped_size = info.size_bytes;
    context->mapped_type = map_type;
    context->mapped_flags = flags;
    mapped_out->buffer = buffer;
    mapped_out->data = data;
    mapped_out->size_bytes = info.size_bytes;
    mapped_out->row_pitch_bytes = info.size_bytes;
    mapped_out->depth_pitch_bytes = info.size_bytes;
    mapped_out->map_type = map_type;
    mapped_out->flags = flags;
    return RIN_GPU_OK;
}

int rindx_d3d11_unmap_buffer(RinDxD3d11Context* context, RinGpuHandle buffer,
                             RinDxD3d11MappedResource* mapped)
{
    int result = RIN_GPU_OK;

    if (!context_valid(context) || buffer == 0u || !mapped ||
        mapped->struct_size != sizeof(*mapped) ||
        mapped->version != RIN_DX_D3D11_VERSION ||
        context->mapped_buffer != buffer ||
        mapped->buffer != buffer || mapped->data != context->mapped_data ||
        mapped->size_bytes != context->mapped_size ||
        mapped->map_type != context->mapped_type ||
        mapped->flags != context->mapped_flags)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (d3d11_map_type_writes(context->mapped_type)) {
        result = ringpu_runtime_upload_buffer(
            context->device->runtime, buffer, 0u, context->mapped_data,
            context->mapped_size);
        if (result != RIN_GPU_OK) return result;
    }
    free(context->mapped_data);
    context->mapped_buffer = 0u;
    context->mapped_data = NULL;
    context->mapped_size = 0u;
    context->mapped_type = 0u;
    context->mapped_flags = 0u;
    memset(mapped, 0, sizeof(*mapped));
    return RIN_GPU_OK;
}

static int d3d11_full_copy_region(const RinGpuImageInfoV1* destination,
                                  const RinGpuImageInfoV1* source,
                                  RinGpuImageCopyRegionV1* region)
{
    const RinGpuImageDescV1* dst;
    const RinGpuImageDescV1* src;
    if (!destination || !source || !region) return RIN_GPU_ERROR_INVALID_ARGUMENT;
    dst = &destination->descriptor;
    src = &source->descriptor;
    if (dst->dimension != RIN_GPU_IMAGE_DIMENSION_2D ||
        src->dimension != RIN_GPU_IMAGE_DIMENSION_2D ||
        dst->format != src->format || dst->width != src->width ||
        dst->height != src->height || dst->depth != src->depth ||
        dst->array_layers != 1u || src->array_layers != 1u ||
        dst->mip_levels != 1u || src->mip_levels != 1u ||
        dst->sample_count != 1u || src->sample_count != 1u) {
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    }
    memset(region, 0, sizeof(*region));
    region->abi_version = RIN_GPU_ABI_VERSION;
    region->struct_size = sizeof(*region);
    region->width = dst->width;
    region->height = dst->height;
    region->depth = dst->depth;
    return RIN_GPU_OK;
}

int rindx_d3d11_copy_buffer(RinDxD3d11Context* context,
                            RinGpuHandle destination, uint64_t destination_offset,
                            RinGpuHandle source, uint64_t source_offset,
                            uint64_t size_bytes)
{
    if (!context_valid(context) || destination == 0u || source == 0u ||
        size_bytes == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_copy_buffer(
        context->device->runtime, context->command_list, destination,
        destination_offset, source, source_offset, size_bytes);
}

int rindx_d3d11_clear_buffer(RinDxD3d11Context* context,
                             RinGpuHandle destination,
                             const RinGpuBufferClearV1* clear)
{
    if (!context_valid(context) || destination == 0u || !clear)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_clear_buffer(
        context->device->runtime, context->command_list, destination, clear);
}

int rindx_d3d11_copy_resource(RinDxD3d11Context* context,
                              RinGpuHandle destination, RinGpuHandle source)
{
    RinGpuImageInfoV1 destination_info;
    RinGpuImageInfoV1 source_info;
    RinGpuImageCopyRegionV1 region;
    int result;
    if (!context_valid(context) || destination == 0u || source == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&destination_info, 0, sizeof(destination_info));
    memset(&source_info, 0, sizeof(source_info));
    result = ringpu_runtime_get_image_info(context->device->runtime,
                                           destination, &destination_info);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_get_image_info(context->device->runtime, source,
                                           &source_info);
    if (result != RIN_GPU_OK) return result;
    result = d3d11_full_copy_region(&destination_info, &source_info, &region);
    if (result != RIN_GPU_OK) return result;
    return ringpu_runtime_command_copy_image(
        context->device->runtime, context->command_list, destination, source,
        &region);
}

int rindx_d3d11_copy_subresource_region(
    RinDxD3d11Context* context, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageCopyRegionV1* region)
{
    if (!context_valid(context) || destination == 0u || source == 0u || !region)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_copy_image(
        context->device->runtime, context->command_list, destination, source,
        region);
}

int rindx_d3d11_resolve_subresource(
    RinDxD3d11Context* context, RinGpuHandle destination, RinGpuHandle source,
    const RinGpuImageResolveV1* resolve)
{
    if (!context_valid(context) || destination == 0u || source == 0u ||
        !resolve)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_resolve_image(
        context->device->runtime, context->command_list, destination, source,
        resolve);
}

int rindx_d3d11_clear_render_target_view(
    RinDxD3d11Context* context, RinGpuHandle target,
    float red, float green, float blue, float alpha)
{
    RinGpuImageClearV1 clear;
    if (!context_valid(context) || target == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&clear, 0, sizeof(clear));
    clear.abi_version = RIN_GPU_ABI_VERSION;
    clear.struct_size = sizeof(clear);
    clear.aspects = RIN_GPU_IMAGE_CLEAR_COLOR;
    clear.color_red = red;
    clear.color_green = green;
    clear.color_blue = blue;
    clear.color_alpha = alpha;
    return ringpu_runtime_command_clear_image(
        context->device->runtime, context->command_list, target, &clear);
}

int rindx_d3d11_clear_depth_stencil_view(
    RinDxD3d11Context* context, RinGpuHandle target, uint32_t clear_flags,
    float depth, uint32_t stencil)
{
    RinGpuImageClearV1 clear;
    if (!context_valid(context) || target == 0u || clear_flags == 0u ||
        (clear_flags & ~RIN_GPU_IMAGE_CLEAR_KNOWN_ASPECTS) != 0u ||
        (clear_flags & (RIN_GPU_IMAGE_CLEAR_DEPTH |
                        RIN_GPU_IMAGE_CLEAR_STENCIL)) == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&clear, 0, sizeof(clear));
    clear.abi_version = RIN_GPU_ABI_VERSION;
    clear.struct_size = sizeof(clear);
    clear.aspects = clear_flags & (RIN_GPU_IMAGE_CLEAR_DEPTH |
                                   RIN_GPU_IMAGE_CLEAR_STENCIL);
    clear.depth = depth;
    clear.stencil = stencil;
    return ringpu_runtime_command_clear_image(
        context->device->runtime, context->command_list, target, &clear);
}

int rindx_d3d11_generate_mips(RinDxD3d11Context* context, RinGpuHandle image)
{
    RinGpuImageInfoV1 info;
    RinGpuImageTransitionV1 transition;
    RinGpuImageBlitV1 blit;
    uint32_t mip;
    int result;
    if (!context_valid(context) || image == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&info, 0, sizeof(info));
    result = ringpu_runtime_get_image_info(context->device->runtime, image,
                                           &info);
    if (result != RIN_GPU_OK) return result;
    if (info.descriptor.dimension != RIN_GPU_IMAGE_DIMENSION_2D ||
        info.descriptor.mip_levels < 2u || info.descriptor.sample_count != 1u ||
        (info.descriptor.usage & (RIN_GPU_IMAGE_COPY_SOURCE |
                                  RIN_GPU_IMAGE_COPY_DESTINATION)) !=
            (RIN_GPU_IMAGE_COPY_SOURCE | RIN_GPU_IMAGE_COPY_DESTINATION) ||
        info.descriptor.width == 0u || info.descriptor.height == 0u)
        return RIN_GPU_ERROR_UNSUPPORTED;
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.base_mip_level = 0u;
    transition.mip_level_count = 1u;
    transition.base_array_layer = 0u;
    transition.array_layer_count = 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
    result = ringpu_runtime_command_transition_image(
        context->device->runtime, context->command_list, image, &transition);
    if (result != RIN_GPU_OK) return result;
    transition.base_mip_level = 1u;
    transition.mip_level_count = info.descriptor.mip_levels - 1u;
    transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    transition.after_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
    /* A transition with equal states is not a command. The remaining mips
     * are upload-ready COPY_DESTINATION subresources by contract. */
    memset(&blit, 0, sizeof(blit));
    blit.abi_version = RIN_GPU_ABI_VERSION;
    blit.struct_size = sizeof(blit);
    blit.source_array_layer = 0u;
    blit.destination_array_layer = 0u;
    blit.filter = RIN_GPU_IMAGE_BLIT_LINEAR;
    for (mip = 1u; mip < info.descriptor.mip_levels; ++mip) {
        uint32_t source_width = info.descriptor.width >> (mip - 1u);
        uint32_t source_height = info.descriptor.height >> (mip - 1u);
        uint32_t destination_width = info.descriptor.width >> mip;
        uint32_t destination_height = info.descriptor.height >> mip;
        if (source_width == 0u || source_height == 0u ||
            destination_width == 0u || destination_height == 0u)
            return RIN_GPU_ERROR_BOUNDS;
        blit.source_mip_level = mip - 1u;
        blit.source_width = source_width;
        blit.source_height = source_height;
        blit.destination_mip_level = mip;
        blit.destination_width = destination_width;
        blit.destination_height = destination_height;
        result = ringpu_runtime_command_blit_image(
            context->device->runtime, context->command_list, image, image,
            &blit);
        if (result != RIN_GPU_OK) return result;
        transition.base_mip_level = mip;
        transition.mip_level_count = 1u;
        transition.before_state = RIN_GPU_IMAGE_STATE_COPY_DESTINATION;
        transition.after_state = RIN_GPU_IMAGE_STATE_COPY_SOURCE;
        result = ringpu_runtime_command_transition_image(
            context->device->runtime, context->command_list, image,
            &transition);
        if (result != RIN_GPU_OK) return result;
    }
    return RIN_GPU_OK;
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
    uint32_t allowed;
    if (!context_valid(context) || !draw || draw->flags != RIN_DX_D3D11_DRAW_FLAG_MASK)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (d3d11_predication_allows(context, &allowed) != RIN_GPU_OK)
        return RIN_GPU_ERROR_BUSY;
    if (!allowed) return RIN_GPU_OK;
    return ringpu_runtime_command_draw(context->device->runtime,
                                       context->command_list, draw);
}

int rindx_d3d11_draw_vertices(RinDxD3d11Context* context,
                              const RinGpuDrawVerticesV2* draw)
{
    uint32_t allowed;
    if (!context_valid(context)) return RIN_GPU_ERROR_STATE;
    if (d3d11_predication_allows(context, &allowed) != RIN_GPU_OK)
        return RIN_GPU_ERROR_BUSY;
    if (!allowed) return RIN_GPU_OK;
    return ringpu_runtime_command_draw_vertices_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_draw_indexed(RinDxD3d11Context* context,
                             const RinGpuDrawIndexedV2* draw)
{
    uint32_t allowed;
    if (!context_valid(context) || !draw || draw->instance_count != 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (d3d11_predication_allows(context, &allowed) != RIN_GPU_OK)
        return RIN_GPU_ERROR_BUSY;
    if (!allowed) return RIN_GPU_OK;
    return ringpu_runtime_command_draw_indexed_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_draw_indexed_instanced(
    RinDxD3d11Context* context, const RinGpuDrawIndexedV2* draw)
{
    uint32_t allowed;
    if (!context_valid(context) || !draw || draw->instance_count <= 1u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (d3d11_predication_allows(context, &allowed) != RIN_GPU_OK)
        return RIN_GPU_ERROR_BUSY;
    if (!allowed) return RIN_GPU_OK;
    return ringpu_runtime_command_draw_indexed_v2(
        context->device->runtime, context->command_list, draw);
}

int rindx_d3d11_dispatch(RinDxD3d11Context* context,
                         const RinGpuDispatchV1* dispatch)
{
    uint32_t allowed;
    if (!context_valid(context) || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    if (d3d11_predication_allows(context, &allowed) != RIN_GPU_OK)
        return RIN_GPU_ERROR_BUSY;
    if (!allowed) return RIN_GPU_OK;
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

int rindx_d3d11_present(RinDxD3d11Context* context, RinGpuHandle image,
                        uint32_t display_id)
{
    RinGpuPresentV1 present;
    if (!context_valid(context) || image == 0u ||
        display_id == UINT32_MAX || context->render_pass_active != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&present, 0, sizeof(present));
    present.abi_version = RIN_GPU_ABI_VERSION;
    present.struct_size = sizeof(present);
    present.image = image;
    present.display_id = display_id;
    return ringpu_runtime_command_present(context->device->runtime,
                                          context->command_list, &present);
}

static int d3d11_swapchain_image_matches(
    RinDxD3d11Context* context, RinGpuDxgiSwapchainRuntime* swapchain,
    RinGpuHandle image, const RinGpuPresentationSubmitV1* submit)
{
    RinGpuImageInfoV1 info;
    for (uint32_t index = 0u; index < RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS;
         ++index) {
        RinGpuDxgiSwapchainBufferV1 buffer;
        int result;
        memset(&buffer, 0, sizeof(buffer));
        buffer.struct_size = sizeof(buffer);
        buffer.version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
        result = rin_gpu_dxgi_swapchain_get_buffer(swapchain, index, &buffer);
        if (result != RIN_GPU_DXGI_SWAPCHAIN_OK) break;
        if (buffer.image_token != submit->image_token) continue;
        if (buffer.resource_handle != image ||
            buffer.display_id != submit->display_id)
            return 0;
        memset(&info, 0, sizeof(info));
        info.abi_version = RIN_GPU_ABI_VERSION;
        info.struct_size = sizeof(info);
        if (ringpu_runtime_get_image_info(context->device->runtime, image,
                                          &info) != RIN_GPU_OK)
            return 0;
        return (info.descriptor.usage & RIN_GPU_IMAGE_PRESENT) != 0u &&
               info.descriptor.dimension == RIN_GPU_IMAGE_DIMENSION_2D &&
               info.descriptor.array_layers == 1u &&
               info.descriptor.mip_levels == 1u &&
               info.descriptor.sample_count == 1u &&
               info.descriptor.width == buffer.width &&
               info.descriptor.height == buffer.height &&
               info.descriptor.format == buffer.format;
    }
    return 0;
}

int rindx_d3d11_present_to_swapchain(
    RinDxD3d11Context* context, RinGpuDxgiSwapchainRuntime* swapchain,
    RinGpuHandle image, const RinGpuPresentationSubmitV1* submit,
    uint64_t timeout_ns, uint64_t* gpu_fence_value_out,
    uint64_t* presentation_fence_value_out)
{
    uint64_t gpu_fence = 0u;
    int result;
    int presentation_result;
    if (gpu_fence_value_out) *gpu_fence_value_out = 0u;
    if (presentation_fence_value_out) *presentation_fence_value_out = 0u;
    if (!context_valid(context) || !swapchain || image == 0u || !submit ||
        submit->struct_size < sizeof(*submit) ||
        submit->version != RIN_GPU_PRESENTATION_VERSION ||
        submit->display_id == UINT32_MAX)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    if (!d3d11_swapchain_image_matches(context, swapchain, image, submit))
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    result = rindx_d3d11_present(context, image, submit->display_id);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d11_close_and_submit(context, &gpu_fence);
    if (result != RIN_GPU_OK) return result;
    if (gpu_fence_value_out) *gpu_fence_value_out = gpu_fence;
    result = rindx_d3d11_wait(context->device, gpu_fence, timeout_ns);
    if (result != RIN_GPU_OK) return result;
    presentation_result = rin_gpu_dxgi_swapchain_present(
        swapchain, submit, presentation_fence_value_out);
    if (presentation_result != RIN_GPU_DXGI_SWAPCHAIN_OK)
        return RIN_GPU_ERROR_BACKEND;
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
