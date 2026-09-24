/* SPDX-License-Identifier: MIT */
#include <rindx/swapchain.h>

#include <string.h>

#define DXGI_SWAPCHAIN_MAGIC UINT64_C(0x52494e4458534331)
#define DXGI_SWAPCHAIN_FIRST_TOKEN UINT64_C(0x4000)

typedef struct DxgiSwapchainState {
    uint64_t magic;
    RinGpuPresentationOutputV1 output;
    uint64_t image_tokens[RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS];
    RinGpuPresentationRuntime presentation;
    RinGpuDxgiWindowOwnerV1 window_owner;
    uint32_t buffer_count;
    uint32_t flags;
    uint32_t next_image;
    uint32_t output_changed;
    uint32_t suboptimal;
    uint32_t device_lost;
    uint64_t submitted_count;
    uint64_t completed_count;
} DxgiSwapchainState;

_Static_assert(sizeof(DxgiSwapchainState) <=
                   sizeof(((RinGpuDxgiSwapchainRuntime*)0)->opaque),
               "RinGPU DXGI swapchain state exceeds public storage");

static DxgiSwapchainState* swapchain_state(
    RinGpuDxgiSwapchainRuntime* runtime) {
    return runtime ? (DxgiSwapchainState*)(void*)runtime->opaque : NULL;
}

static int swapchain_ready(const DxgiSwapchainState* state) {
    return state && state->magic == DXGI_SWAPCHAIN_MAGIC;
}

static int map_presentation_result(int result) {
    switch (result) {
    case RIN_GPU_PRESENTATION_OK:
        return RIN_GPU_DXGI_SWAPCHAIN_OK;
    case RIN_GPU_PRESENTATION_BUSY:
        return RIN_GPU_DXGI_SWAPCHAIN_WAS_STILL_DRAWING;
    case RIN_GPU_PRESENTATION_STALE:
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    case RIN_GPU_PRESENTATION_DEVICE_LOST:
        return RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST;
    case RIN_GPU_PRESENTATION_UNSUPPORTED:
        return RIN_GPU_DXGI_SWAPCHAIN_UNSUPPORTED;
    case RIN_GPU_PRESENTATION_LIMIT:
        return RIN_GPU_DXGI_SWAPCHAIN_LIMIT;
    case RIN_GPU_PRESENTATION_TIMEOUT:
        return RIN_GPU_DXGI_SWAPCHAIN_WAS_STILL_DRAWING;
    default:
        return RIN_GPU_DXGI_SWAPCHAIN_BACKEND;
    }
}

static int output_valid(const RinGpuPresentationOutputV1* output) {
    return output && output->struct_size >= sizeof(*output) &&
           output->version == RIN_GPU_PRESENTATION_VERSION &&
           output->display_id != UINT32_MAX && output->width != 0u &&
           output->height != 0u && output->refresh_millihertz != 0u &&
           output->format != 0u && output->output_generation != 0u &&
           output->output_generation != UINT64_MAX &&
           output->device_generation != 0u &&
           (output->flags & ~RIN_GPU_PRESENTATION_OUTPUT_KNOWN_FLAGS) == 0u &&
           (output->flags & RIN_GPU_PRESENTATION_OUTPUT_FIFO) != 0u &&
           output->reserved[0] == 0u && output->reserved[1] == 0u;
}

static int descriptor_valid(const RinGpuDxgiSwapchainDescV1* descriptor) {
    return descriptor && descriptor->struct_size >= sizeof(*descriptor) &&
           descriptor->version == RIN_GPU_DXGI_SWAPCHAIN_VERSION &&
           descriptor->buffer_count >= RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS &&
           descriptor->buffer_count <= RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS &&
           (descriptor->flags & ~RIN_GPU_DXGI_SWAPCHAIN_KNOWN_FLAGS) == 0u &&
           output_valid(&descriptor->output) &&
           ((descriptor->flags & RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING) == 0u ||
            (descriptor->output.flags & RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE) !=
                0u) &&
           descriptor->reserved[0] == 0u && descriptor->reserved[1] == 0u;
}

static int window_owner_valid(const RinGpuDxgiWindowOwnerV1* owner) {
    return owner && owner->struct_size >= sizeof(*owner) &&
           owner->version == RIN_GPU_DXGI_SWAPCHAIN_VERSION &&
           owner->native_window != NULL && owner->retain && owner->release &&
           owner->validate && owner->reserved[0] == 0u &&
           owner->reserved[1] == 0u;
}

static int image_index(const DxgiSwapchainState* state, uint64_t token) {
    for (uint32_t index = 0u; index < state->buffer_count; ++index)
        if (state->image_tokens[index] == token) return (int)index;
    return -1;
}

static int register_images(DxgiSwapchainState* state) {
    uint32_t registered = 0u;
    for (uint32_t index = 0u; index < state->buffer_count; ++index) {
        RinGpuPresentationImageV1 image;
        int result;
        memset(&image, 0, sizeof(image));
        image.struct_size = sizeof(image);
        image.version = RIN_GPU_PRESENTATION_VERSION;
        image.image_token = DXGI_SWAPCHAIN_FIRST_TOKEN + index;
        image.display_id = state->output.display_id;
        image.width = state->output.width;
        image.height = state->output.height;
        image.format = state->output.format;
        image.usage = RIN_GPU_PRESENTATION_IMAGE_RENDER_TARGET |
                      RIN_GPU_PRESENTATION_IMAGE_PRESENT;
        image.output_generation = state->output.output_generation;
        image.device_generation = state->output.device_generation;
        result = rin_gpu_presentation_register_image(&state->presentation,
                                                     &image);
        if (result != RIN_GPU_PRESENTATION_OK) {
            for (uint32_t rollback = 0u; rollback < registered; ++rollback)
                (void)rin_gpu_presentation_unregister_image(
                    &state->presentation, state->image_tokens[rollback]);
            return result;
        }
        state->image_tokens[index] = image.image_token;
        ++registered;
    }
    return RIN_GPU_PRESENTATION_OK;
}

static void unregister_images(DxgiSwapchainState* state) {
    for (uint32_t index = 0u; index < state->buffer_count; ++index)
        (void)rin_gpu_presentation_unregister_image(
            &state->presentation, state->image_tokens[index]);
    memset(state->image_tokens, 0, sizeof(state->image_tokens));
}

static int remove_images_and_advance(DxgiSwapchainState* state,
                                     uint64_t next_generation) {
    int result = rin_gpu_presentation_remove_output(
        &state->presentation, state->output.display_id, next_generation);
    if (result != RIN_GPU_PRESENTATION_OK) return result;
    unregister_images(state);
    return RIN_GPU_PRESENTATION_OK;
}

int rin_gpu_dxgi_swapchain_runtime_init(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuDxgiSwapchainDescV1* descriptor,
    const RinGpuDxgiWindowOwnerV1* window_owner,
    const RinGpuPresentationBackendV1* presentation_backend) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    int result;
    if (!state || !descriptor_valid(descriptor) ||
        !window_owner_valid(window_owner) || !presentation_backend)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    if (window_owner->validate(window_owner->context,
                               window_owner->native_window,
                               descriptor->output.display_id) != 0)
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    memset(state, 0, sizeof(*state));
    state->output = descriptor->output;
    state->buffer_count = descriptor->buffer_count;
    state->flags = descriptor->flags;
    state->window_owner = *window_owner;
    result = window_owner->retain(window_owner->context,
                                  window_owner->native_window);
    if (result != 0) {
        memset(state, 0, sizeof(*state));
        return RIN_GPU_DXGI_SWAPCHAIN_BACKEND;
    }
    result = rin_gpu_presentation_runtime_init(
        &state->presentation, descriptor->output.device_generation,
        presentation_backend);
    if (result != RIN_GPU_PRESENTATION_OK) goto fail_window;
    result = rin_gpu_presentation_register_output(&state->presentation,
                                                  &state->output);
    if (result != RIN_GPU_PRESENTATION_OK) goto fail_presentation;
    result = register_images(state);
    if (result != RIN_GPU_PRESENTATION_OK) {
        (void)rin_gpu_presentation_remove_output(
            &state->presentation, state->output.display_id,
            state->output.output_generation + 1u);
        goto fail_presentation;
    }
    state->magic = DXGI_SWAPCHAIN_MAGIC;
    return RIN_GPU_DXGI_SWAPCHAIN_OK;

fail_presentation:
    memset(&state->presentation, 0, sizeof(state->presentation));
fail_window:
    state->window_owner.release(state->window_owner.context,
                                state->window_owner.native_window);
    memset(state, 0, sizeof(*state));
    return map_presentation_result(result);
}

int rin_gpu_dxgi_swapchain_acquire(
    RinGpuDxgiSwapchainRuntime* runtime,
    RinGpuPresentationAcquireV1* acquire_out) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    uint32_t start;
    if (acquire_out) memset(acquire_out, 0, sizeof(*acquire_out));
    if (!swapchain_ready(state) || !acquire_out)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    if (state->device_lost) return RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST;
    if (state->output_changed) return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    start = state->next_image;
    for (uint32_t offset = 0u; offset < state->buffer_count; ++offset) {
        uint32_t index = (start + offset) % state->buffer_count;
        RinGpuPresentationAcquireV1 acquire;
        int result;
        memset(&acquire, 0, sizeof(acquire));
        acquire.struct_size = sizeof(acquire);
        acquire.version = RIN_GPU_PRESENTATION_VERSION;
        acquire.display_id = state->output.display_id;
        acquire.mode = (state->flags & RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING) !=
                               0u
                           ? RIN_GPU_PRESENTATION_MODE_IMMEDIATE
                           : RIN_GPU_PRESENTATION_MODE_FIFO;
        acquire.image_token = state->image_tokens[index];
        acquire.output_generation = state->output.output_generation;
        acquire.device_generation = state->output.device_generation;
        result = rin_gpu_presentation_begin_frame(&state->presentation,
                                                  &acquire);
        if (result == RIN_GPU_PRESENTATION_BUSY) continue;
        if (result != RIN_GPU_PRESENTATION_OK)
            return map_presentation_result(result);
        state->next_image = (index + 1u) % state->buffer_count;
        *acquire_out = acquire;
        return state->suboptimal ? RIN_GPU_DXGI_SWAPCHAIN_SUBOPTIMAL
                                 : RIN_GPU_DXGI_SWAPCHAIN_OK;
    }
    return RIN_GPU_DXGI_SWAPCHAIN_WAS_STILL_DRAWING;
}

int rin_gpu_dxgi_swapchain_runtime_shutdown(
    RinGpuDxgiSwapchainRuntime* runtime) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    int result;
    if (!swapchain_ready(state)) return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    if (!state->device_lost && state->buffer_count != 0u) {
        result = rin_gpu_presentation_remove_output(
            &state->presentation, state->output.display_id,
            state->output.output_generation == UINT64_MAX
                ? UINT64_MAX
                : state->output.output_generation + 1u);
        if (result != RIN_GPU_PRESENTATION_OK)
            return map_presentation_result(result);
        unregister_images(state);
        state->buffer_count = 0u;
    }
    state->window_owner.release(state->window_owner.context,
                                state->window_owner.native_window);
    memset(state, 0, sizeof(*state));
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_present(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationSubmitV1* submit, uint64_t* fence_value_out) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    uint32_t expected_mode;
    int result;
    if (fence_value_out) *fence_value_out = 0u;
    if (!swapchain_ready(state) || !submit || !fence_value_out)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    expected_mode = (state->flags & RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING) != 0u
                        ? RIN_GPU_PRESENTATION_MODE_IMMEDIATE
                        : RIN_GPU_PRESENTATION_MODE_FIFO;
    if (state->device_lost) return RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST;
    if (state->output_changed) return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    if (submit->mode != expected_mode ||
        image_index(state, submit->image_token) < 0)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    result = rin_gpu_presentation_submit_frame(&state->presentation, submit,
                                               fence_value_out);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    ++state->submitted_count;
    return state->suboptimal ? RIN_GPU_DXGI_SWAPCHAIN_SUBOPTIMAL
                             : RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_complete(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationCompletionV1* completion) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    int result;
    if (!swapchain_ready(state) || !completion)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    result = rin_gpu_presentation_complete(&state->presentation, completion);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    ++state->completed_count;
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_resize_buffers(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationOutputV1* output, uint32_t buffer_count) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    int result;
    if (!swapchain_ready(state) || !output_valid(output) ||
        buffer_count < RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS ||
        buffer_count > RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS ||
        output->display_id != state->output.display_id ||
        output->device_generation != state->output.device_generation ||
        output->output_generation <= state->output.output_generation ||
        ((state->flags & RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING) != 0u &&
         (output->flags & RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE) == 0u))
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    if (state->window_owner.validate(state->window_owner.context,
                                    state->window_owner.native_window,
                                    output->display_id) != 0)
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    result = remove_images_and_advance(state, output->output_generation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    state->buffer_count = 0u;
    result = rin_gpu_presentation_update_output(&state->presentation, output);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    state->output = *output;
    state->buffer_count = buffer_count;
    result = register_images(state);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    state->next_image = 0u;
    state->output_changed = 0u;
    state->suboptimal = 0u;
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_set_fullscreen(
    RinGpuDxgiSwapchainRuntime* runtime, uint32_t fullscreen,
    const RinGpuPresentationOutputV1* output) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    RinGpuPresentationOutputV1 updated;
    uint32_t old_flags;
    int result;
    if (!swapchain_ready(state) || fullscreen > 1u || !output)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    updated = *output;
    old_flags = state->flags;
    if (fullscreen)
        state->flags |= RIN_GPU_DXGI_SWAPCHAIN_FULLSCREEN;
    else
        state->flags &= ~RIN_GPU_DXGI_SWAPCHAIN_FULLSCREEN;
    result = rin_gpu_dxgi_swapchain_resize_buffers(
        runtime, &updated, state->buffer_count);
    if (result != RIN_GPU_DXGI_SWAPCHAIN_OK) {
        state->flags = old_flags;
        return result;
    }
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_notify_output_change(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationOutputV1* observed_output) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    if (!swapchain_ready(state) || !output_valid(observed_output) ||
        observed_output->display_id != state->output.display_id ||
        observed_output->device_generation != state->output.device_generation)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    if (observed_output->output_generation <= state->output.output_generation)
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    if (observed_output->width != state->output.width ||
        observed_output->height != state->output.height ||
        observed_output->format != state->output.format) {
        state->output_changed = 1u;
        return RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE;
    }
    state->suboptimal = 1u;
    return RIN_GPU_DXGI_SWAPCHAIN_SUBOPTIMAL;
}

int rin_gpu_dxgi_swapchain_device_lost(RinGpuDxgiSwapchainRuntime* runtime) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    int result;
    if (!swapchain_ready(state)) return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    result = rin_gpu_presentation_device_lost(&state->presentation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    state->device_lost = 1u;
    state->output_changed = 1u;
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_device_reset(
    RinGpuDxgiSwapchainRuntime* runtime, uint64_t next_device_generation) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    RinGpuPresentationOutputV1 output;
    int result;
    if (!swapchain_ready(state) || next_device_generation == 0u ||
        next_device_generation <= state->output.device_generation)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    if (!state->device_lost) {
        result = rin_gpu_presentation_drop_pending(
            &state->presentation, state->output.display_id,
            state->output.output_generation, state->output.device_generation);
        if (result != RIN_GPU_PRESENTATION_OK)
            return map_presentation_result(result);
    }
    output = state->output;
    output.device_generation = next_device_generation;
    output.output_generation = output.output_generation == UINT64_MAX
                                   ? UINT64_MAX
                                   : output.output_generation + 1u;
    if (output.output_generation == UINT64_MAX)
        return RIN_GPU_DXGI_SWAPCHAIN_LIMIT;
    result = rin_gpu_presentation_device_reset(&state->presentation,
                                               next_device_generation);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    state->output = output;
    state->next_image = 0u;
    state->output_changed = 0u;
    state->suboptimal = 0u;
    state->device_lost = 0u;
    result = rin_gpu_presentation_register_output(&state->presentation,
                                                  &state->output);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    result = register_images(state);
    if (result != RIN_GPU_PRESENTATION_OK)
        return map_presentation_result(result);
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}

int rin_gpu_dxgi_swapchain_get_status(
    RinGpuDxgiSwapchainRuntime* runtime,
    RinGpuDxgiSwapchainStatusV1* status_out) {
    DxgiSwapchainState* state = swapchain_state(runtime);
    if (!swapchain_ready(state) || !status_out ||
        status_out->struct_size < sizeof(*status_out) ||
        status_out->version != RIN_GPU_DXGI_SWAPCHAIN_VERSION)
        return RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT;
    memset(status_out, 0, sizeof(*status_out));
    status_out->struct_size = sizeof(*status_out);
    status_out->version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    status_out->buffer_count = state->buffer_count;
    status_out->flags = state->flags;
    status_out->output_changed = state->output_changed || state->suboptimal;
    status_out->tearing_supported =
        (state->output.flags & RIN_GPU_PRESENTATION_OUTPUT_IMMEDIATE) != 0u;
    status_out->output_generation = state->output.output_generation;
    status_out->device_generation = state->output.device_generation;
    status_out->submitted_count = state->submitted_count;
    status_out->completed_count = state->completed_count;
    return RIN_GPU_DXGI_SWAPCHAIN_OK;
}
