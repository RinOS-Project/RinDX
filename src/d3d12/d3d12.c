/* SPDX-License-Identifier: MIT */
#include <rindx/d3d12.h>

#include <string.h>

#define RIN_DX_D3D12_STATE_READY 1u
#define RIN_DX_D3D12_STATE_CLOSED 2u

static int device_valid(const RinDxD3d12Device* device)
{
    return device && device->struct_size == sizeof(*device) &&
           device->version == RIN_DX_D3D12_VERSION && device->runtime &&
           device->queue != 0u && device->fence != 0u &&
           device->feature_level == RIN_DX_D3D12_FEATURE_LEVEL_12_0 &&
           device->state == RIN_DX_D3D12_STATE_READY;
}

static int allocator_valid(const RinDxD3d12CommandAllocator* allocator)
{
    return allocator && allocator->struct_size == sizeof(*allocator) &&
           allocator->version == RIN_DX_D3D12_VERSION &&
           device_valid(allocator->device) && allocator->state ==
               RIN_DX_D3D12_STATE_READY;
}

static int list_shape_valid(const RinDxD3d12CommandList* list)
{
    return list && list->struct_size == sizeof(*list) &&
           list->version == RIN_DX_D3D12_VERSION &&
           allocator_valid(list->allocator) && list->command_list != 0u &&
           (list->state == RIN_DX_D3D12_STATE_READY ||
            list->state == RIN_DX_D3D12_STATE_CLOSED);
}

static int list_valid(const RinDxD3d12CommandList* list)
{
    return list_shape_valid(list) && list->state == RIN_DX_D3D12_STATE_READY;
}

static int feature_level_requested(const uint32_t* levels, uint32_t count)
{
    uint32_t index;
    if (count == 0u) return 1;
    if (!levels || count > 16u) return 0;
    for (index = 0u; index < count; ++index)
        if (levels[index] == RIN_DX_D3D12_FEATURE_LEVEL_12_0) return 1;
    return 0;
}

static int create_queue_and_fence(RinDxD3d12Device* device)
{
    RinGpuQueueDescV1 queue_desc;
    int result;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.abi_version = RIN_GPU_ABI_VERSION;
    queue_desc.struct_size = sizeof(queue_desc);
    queue_desc.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    result = ringpu_runtime_create_queue(device->runtime, &queue_desc,
                                         &device->queue);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_create_fence(device->runtime, 0u, &device->fence);
    if (result != RIN_GPU_OK) {
        (void)ringpu_runtime_destroy_object(device->runtime, device->queue);
        device->queue = 0u;
    }
    return result;
}

int rindx_d3d12_create_device(
    const RinGpuRuntimeSoftwareSurfaceDescV1* surface,
    const uint32_t* requested_feature_levels, uint32_t feature_level_count,
    RinDxD3d12Device* device_out)
{
    int result;
    if (!surface || !device_out || !feature_level_requested(
            requested_feature_levels, feature_level_count))
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(device_out, 0, sizeof(*device_out));
    result = ringpu_runtime_software_surface_create(surface,
                                                    &device_out->runtime);
    if (result != RIN_GPU_OK) return result;
    result = create_queue_and_fence(device_out);
    if (result != RIN_GPU_OK) {
        ringpu_runtime_destroy(device_out->runtime);
        memset(device_out, 0, sizeof(*device_out));
        return result;
    }
    device_out->struct_size = sizeof(*device_out);
    device_out->version = RIN_DX_D3D12_VERSION;
    device_out->feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;
    device_out->state = RIN_DX_D3D12_STATE_READY;
    return RIN_GPU_OK;
}

int rindx_d3d12_destroy_device(RinDxD3d12Device* device)
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

int rindx_d3d12_create_command_allocator(
    RinDxD3d12Device* device, RinDxD3d12CommandAllocator* allocator_out)
{
    if (!device_valid(device) || !allocator_out)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(allocator_out, 0, sizeof(*allocator_out));
    allocator_out->struct_size = sizeof(*allocator_out);
    allocator_out->version = RIN_DX_D3D12_VERSION;
    allocator_out->device = device;
    allocator_out->state = RIN_DX_D3D12_STATE_READY;
    return RIN_GPU_OK;
}

int rindx_d3d12_reset_command_allocator(RinDxD3d12CommandAllocator* allocator)
{
    if (!allocator_valid(allocator) || allocator->recording_lists != 0u)
        return RIN_GPU_ERROR_BUSY;
    return RIN_GPU_OK;
}

int rindx_d3d12_destroy_command_allocator(
    RinDxD3d12CommandAllocator* allocator)
{
    if (!allocator_valid(allocator) || allocator->recording_lists != 0u)
        return RIN_GPU_ERROR_BUSY;
    memset(allocator, 0, sizeof(*allocator));
    return RIN_GPU_OK;
}

int rindx_d3d12_create_command_list(
    RinDxD3d12Device* device, RinDxD3d12CommandAllocator* allocator,
    RinDxD3d12CommandList* list_out)
{
    RinGpuCommandListDescV1 descriptor;
    int result;
    if (!device_valid(device) || !allocator_valid(allocator) ||
        allocator->device != device || !list_out ||
        allocator->recording_lists != 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = RIN_GPU_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    descriptor.capabilities = RIN_GPU_QUEUE_COPY | RIN_GPU_QUEUE_COMPUTE |
                              RIN_GPU_QUEUE_GRAPHICS;
    memset(list_out, 0, sizeof(*list_out));
    result = ringpu_runtime_create_command_list(device->runtime, &descriptor,
                                                &list_out->command_list);
    if (result != RIN_GPU_OK) return result;
    list_out->struct_size = sizeof(*list_out);
    list_out->version = RIN_DX_D3D12_VERSION;
    list_out->device = device;
    list_out->allocator = allocator;
    list_out->state = RIN_DX_D3D12_STATE_READY;
    allocator->recording_lists = 1u;
    return RIN_GPU_OK;
}

int rindx_d3d12_reset_command_list(RinDxD3d12CommandList* list)
{
    int result;
    if (!list_shape_valid(list) || list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_command_list_reset(list->device->runtime,
                                               list->command_list);
    if (result != RIN_GPU_OK) return result;
    list->active_color_target = 0u;
    list->active_depth_target = 0u;
    list->state = RIN_DX_D3D12_STATE_READY;
    return RIN_GPU_OK;
}

int rindx_d3d12_close_command_list(RinDxD3d12CommandList* list)
{
    int result;
    if (!list_valid(list) || list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_command_list_close(list->device->runtime,
                                               list->command_list);
    if (result != RIN_GPU_OK) return result;
    list->state = RIN_DX_D3D12_STATE_CLOSED;
    return RIN_GPU_OK;
}

int rindx_d3d12_destroy_command_list(RinDxD3d12CommandList* list)
{
    int result;
    if (!list || list->struct_size != sizeof(*list) ||
        list->version != RIN_DX_D3D12_VERSION || !list->allocator ||
        list->command_list == 0u || list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_destroy_object(list->device->runtime,
                                           list->command_list);
    if (result != RIN_GPU_OK) return result;
    list->allocator->recording_lists = 0u;
    memset(list, 0, sizeof(*list));
    return RIN_GPU_OK;
}

int rindx_d3d12_create_buffer(RinDxD3d12Device* device,
                              const RinGpuBufferDescV1* descriptor,
                              RinGpuHandle* buffer_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_buffer(device->runtime, descriptor,
                                        buffer_out);
}

int rindx_d3d12_create_texture2d(RinDxD3d12Device* device,
                                 const RinGpuImageDescV1* descriptor,
                                 RinGpuHandle* image_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    if (!descriptor || descriptor->dimension != RIN_GPU_IMAGE_DIMENSION_2D)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_create_image(device->runtime, descriptor, image_out);
}

int rindx_d3d12_create_shader(RinDxD3d12Device* device, const void* rin_shader,
                              uint64_t shader_size,
                              RinGpuHandle* shader_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_shader_module(device->runtime, rin_shader,
                                               shader_size, shader_out);
}

int rindx_d3d12_create_graphics_pipeline(
    RinDxD3d12Device* device, const RinGpuGraphicsPipelineNativeDescV2* desc,
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

int rindx_d3d12_create_compute_pipeline(
    RinDxD3d12Device* device, RinGpuHandle shader,
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

int rindx_d3d12_create_compute_bind_group(
    RinDxD3d12Device* device, RinGpuHandle pipeline,
    const RinGpuBufferBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* bind_group_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_compute_bind_group(
        device->runtime, pipeline, bindings, binding_count, bind_group_out);
}

int rindx_d3d12_create_descriptor_heap(
    RinDxD3d12Device* device, RinGpuHandle pipeline,
    const RinGpuGraphicsBindingV1* bindings, uint32_t binding_count,
    RinGpuHandle* heap_out)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_create_graphics_bind_group_typed(
        device->runtime, pipeline, bindings, binding_count, heap_out);
}

int rindx_d3d12_destroy_object(RinDxD3d12Device* device, RinGpuHandle object)
{
    if (!device_valid(device) || object == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_destroy_object(device->runtime, object);
}

int rindx_d3d12_upload_buffer(RinDxD3d12Device* device, RinGpuHandle buffer,
                             uint64_t offset, const void* source,
                             uint64_t size_bytes)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_upload_buffer(device->runtime, buffer, offset,
                                        source, size_bytes);
}

int rindx_d3d12_transition_image(
    RinDxD3d12CommandList* list, RinGpuHandle image,
    uint32_t before_state, uint32_t after_state)
{
    RinGpuImageTransitionV1 transition;
    if (!list_valid(list) || image == 0u || before_state == after_state)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&transition, 0, sizeof(transition));
    transition.abi_version = RIN_GPU_ABI_VERSION;
    transition.struct_size = sizeof(transition);
    transition.mip_level_count = 1u;
    transition.array_layer_count = 1u;
    transition.before_state = before_state;
    transition.after_state = after_state;
    return ringpu_runtime_command_transition_image(
        list->device->runtime, list->command_list, image, &transition);
}

int rindx_d3d12_begin_render_pass(
    RinDxD3d12CommandList* list, RinGpuHandle color_target,
    RinGpuHandle depth_target, uint32_t clear_color,
    float clear_red, float clear_green, float clear_blue, float clear_alpha,
    float clear_depth)
{
    int result;
    if (!list_valid(list) || color_target == 0u || clear_color > 1u ||
        list->render_pass_active != 0u)
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
            list->device->runtime, list->command_list, &pass);
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
            list->device->runtime, list->command_list, &pass);
    }
    if (result != RIN_GPU_OK) return result;
    list->active_color_target = color_target;
    list->active_depth_target = depth_target;
    list->render_pass_active = 1u;
    return RIN_GPU_OK;
}

int rindx_d3d12_set_raster_state(RinDxD3d12CommandList* list,
                                 const RinGpuRasterStateV1* state)
{
    if (!list_valid(list)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_set_raster_state(
        list->device->runtime, list->command_list, state);
}

int rindx_d3d12_bind_descriptor_heap(RinDxD3d12CommandList* list,
                                     RinGpuHandle heap)
{
    if (!list_valid(list)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_bind_graphics_resources(
        list->device->runtime, list->command_list, heap);
}

int rindx_d3d12_draw(RinDxD3d12CommandList* list, const RinGpuDrawV1* draw)
{
    if (!list_valid(list) || !draw || draw->instance_count == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw(list->device->runtime,
                                       list->command_list, draw);
}

int rindx_d3d12_draw_indexed_instanced(
    RinDxD3d12CommandList* list, const RinGpuDrawIndexedV2* draw)
{
    if (!list_valid(list) || !draw || draw->instance_count == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw_indexed_v2(
        list->device->runtime, list->command_list, draw);
}

int rindx_d3d12_execute_indirect_draw(
    RinDxD3d12CommandList* list, const RinGpuDrawIndirectV1* draw)
{
    if (!list_valid(list) || !draw || draw->draw_count == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw_indirect(
        list->device->runtime, list->command_list, draw);
}

int rindx_d3d12_execute_indirect_draw_indexed(
    RinDxD3d12CommandList* list, const RinGpuDrawIndexedIndirectV1* draw)
{
    if (!list_valid(list) || !draw || draw->draw_count == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_command_draw_indexed_indirect(
        list->device->runtime, list->command_list, draw);
}

int rindx_d3d12_dispatch(RinDxD3d12CommandList* list,
                         const RinGpuDispatchV1* dispatch)
{
    if (!list_valid(list) || list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_dispatch(list->device->runtime,
                                           list->command_list, dispatch);
}

int rindx_d3d12_execute_indirect_dispatch(
    RinDxD3d12CommandList* list, const RinGpuDispatchIndirectV1* dispatch)
{
    if (!list_valid(list) || !dispatch || list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_command_dispatch_indirect(
        list->device->runtime, list->command_list, dispatch);
}

int rindx_d3d12_end_render_pass(RinDxD3d12CommandList* list)
{
    int result;
    if (!list_valid(list) || list->render_pass_active == 0u)
        return RIN_GPU_ERROR_STATE;
    result = ringpu_runtime_command_end_render_pass(list->device->runtime,
                                                    list->command_list);
    if (result != RIN_GPU_OK) return result;
    list->active_color_target = 0u;
    list->active_depth_target = 0u;
    list->render_pass_active = 0u;
    return RIN_GPU_OK;
}

int rindx_d3d12_execute_command_lists(RinDxD3d12Device* device,
                                      RinDxD3d12CommandList* list,
                                      uint64_t* fence_value_out)
{
    RinGpuSubmitInfoV1 submit;
    uint64_t value;
    int result;
    if (!device_valid(device) || !list || list->device != device ||
        list->state != RIN_DX_D3D12_STATE_READY ||
        list->render_pass_active != 0u)
        return RIN_GPU_ERROR_STATE;
    result = rindx_d3d12_close_command_list(list);
    if (result != RIN_GPU_OK) return result;
    value = device->submission_value + 1u;
    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = list->command_list;
    submit.signal_fence = device->fence;
    submit.signal_value = value;
    result = ringpu_runtime_queue_submit(device->runtime, device->queue,
                                         &submit);
    if (result != RIN_GPU_OK) return result;
    device->submission_value = value;
    if (fence_value_out) *fence_value_out = value;
    return RIN_GPU_OK;
}

int rindx_d3d12_wait(RinDxD3d12Device* device, uint64_t fence_value,
                     uint64_t timeout_ns)
{
    if (!device_valid(device) || fence_value == 0u)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    return ringpu_runtime_wait_fence(device->runtime, device->fence,
                                     fence_value, timeout_ns);
}

int rindx_d3d12_readback_image(
    RinDxD3d12Device* device, RinGpuHandle image,
    const RinGpuImageReadbackV1* readback, void* destination,
    uint64_t destination_size)
{
    if (!device_valid(device)) return RIN_GPU_ERROR_STATE;
    return ringpu_runtime_readback_image(device->runtime, image, readback,
                                         destination, destination_size);
}
