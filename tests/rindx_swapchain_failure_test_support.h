/* SPDX-License-Identifier: MIT */
#ifndef RINDX_SWAPCHAIN_FAILURE_TEST_SUPPORT_H
#define RINDX_SWAPCHAIN_FAILURE_TEST_SUPPORT_H

#include <rindx/swapchain.h>

#include <stdint.h>
#include <string.h>

typedef struct RinDxSwapchainFailureProbe {
    uint32_t validate_calls;
    uint32_t retain_calls;
    uint32_t release_calls;
    uint32_t present_calls;
    int fail_validation;
} RinDxSwapchainFailureProbe;

static int rindx_swapchain_test_retain(void* context, void* native_window)
{
    RinDxSwapchainFailureProbe* probe = context;
    if (!probe || !native_window) return -1;
    ++probe->retain_calls;
    return 0;
}

static void rindx_swapchain_test_release(void* context, void* native_window)
{
    RinDxSwapchainFailureProbe* probe = context;
    if (probe && native_window) ++probe->release_calls;
}

static int rindx_swapchain_test_validate(void* context, void* native_window,
                                         uint32_t display_id)
{
    RinDxSwapchainFailureProbe* probe = context;
    if (!probe || !native_window || display_id != 0u) return -1;
    ++probe->validate_calls;
    return probe->fail_validation ? 1 : 0;
}

static int rindx_swapchain_test_present(
    void* context, const RinGpuPresentationSubmitV1* submit,
    uint64_t fence_value)
{
    RinDxSwapchainFailureProbe* probe = context;
    if (!probe || !submit || fence_value == 0u) return -1;
    ++probe->present_calls;
    return 0;
}

static void rindx_swapchain_test_make_inputs(
    RinGpuDxgiSwapchainDescV1* desc,
    RinGpuDxgiWindowOwnerV1* window_owner,
    RinGpuPresentationBackendV1* presentation_backend,
    RinDxSwapchainFailureProbe* probe)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    desc->buffer_count = RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS;
    desc->output.struct_size = sizeof(desc->output);
    desc->output.version = RIN_GPU_PRESENTATION_VERSION;
    desc->output.display_id = 0u;
    desc->output.flags = RIN_GPU_PRESENTATION_OUTPUT_FIFO;
    desc->output.width = 2u;
    desc->output.height = 2u;
    desc->output.refresh_millihertz = 60000u;
    desc->output.format = RIN_GPU_FORMAT_RGBA8_UNORM;
    desc->output.output_generation = 1u;
    desc->output.device_generation = 1u;

    memset(window_owner, 0, sizeof(*window_owner));
    window_owner->struct_size = sizeof(*window_owner);
    window_owner->version = RIN_GPU_DXGI_SWAPCHAIN_VERSION;
    window_owner->context = probe;
    window_owner->native_window = (void*)(uintptr_t)1u;
    window_owner->retain = rindx_swapchain_test_retain;
    window_owner->release = rindx_swapchain_test_release;
    window_owner->validate = rindx_swapchain_test_validate;

    memset(presentation_backend, 0, sizeof(*presentation_backend));
    presentation_backend->struct_size = sizeof(*presentation_backend);
    presentation_backend->version = RIN_GPU_PRESENTATION_VERSION;
    presentation_backend->context = probe;
    presentation_backend->submit = rindx_swapchain_test_present;
}

#endif /* RINDX_SWAPCHAIN_FAILURE_TEST_SUPPORT_H */
