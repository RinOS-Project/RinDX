/* SPDX-License-Identifier: MIT */
#include <rindx/d3d11.h>

#include "rindx_swapchain_failure_test_support.h"

#include "../../RinGPU/src/core/core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static const RinGpuBackendOpsV1* delegated_ops;
static uint32_t create_calls;
static uint32_t delegated_create_calls;
static uint32_t fail_next_create;
static uint32_t fail_next_partial_create;
static uint32_t destroyed_buffer_calls;
static uint32_t image_create_calls;
static uint32_t delegated_image_create_calls;
static uint32_t fail_next_image_create;
static uint32_t fail_next_partial_image_create;
static uint32_t destroyed_image_calls;
static uint32_t submit_calls;
static uint32_t fail_next_submit;
static uint32_t wait_calls;
static uint32_t fail_next_wait;

static int injected_create_buffer(void* context,
                                 const RinGpuBufferDescV1* desc,
                                 uint64_t* cookie)
{
    int result;
    if (!context || !desc || !cookie || !delegated_ops ||
        !delegated_ops->create_buffer)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    ++create_calls;
    if (fail_next_create != 0u) {
        --fail_next_create;
        return RIN_GPU_ERROR_NO_MEMORY;
    }
    result = delegated_ops->create_buffer(context, desc, cookie);
    if (result != RIN_GPU_OK) return result;
    ++delegated_create_calls;
    if (fail_next_partial_create != 0u) {
        --fail_next_partial_create;
        return RIN_GPU_ERROR_BACKEND;
    }
    return result;
}

static void injected_destroy_buffer(void* context, uint64_t cookie)
{
    if (!delegated_ops || !delegated_ops->destroy_buffer) return;
    ++destroyed_buffer_calls;
    delegated_ops->destroy_buffer(context, cookie);
}

static int injected_create_image(void* context,
                                const RinGpuImageDescV1* desc,
                                uint64_t allocation_bytes, uint64_t* cookie)
{
    int result;
    if (!context || !desc || !cookie || !delegated_ops ||
        !delegated_ops->create_image)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    ++image_create_calls;
    if (fail_next_image_create != 0u) {
        --fail_next_image_create;
        return RIN_GPU_ERROR_NO_MEMORY;
    }
    result = delegated_ops->create_image(context, desc, allocation_bytes,
                                          cookie);
    if (result != RIN_GPU_OK) return result;
    ++delegated_image_create_calls;
    if (fail_next_partial_image_create != 0u) {
        --fail_next_partial_image_create;
        return RIN_GPU_ERROR_BACKEND;
    }
    return result;
}

static void injected_destroy_image(void* context, uint64_t cookie)
{
    if (!delegated_ops || !delegated_ops->destroy_image) return;
    ++destroyed_image_calls;
    delegated_ops->destroy_image(context, cookie);
}

static int injected_submit(void* context,
                           const RinGpuBackendCommandV1* commands,
                           uint32_t command_count)
{
    if (!context || !delegated_ops || !delegated_ops->submit_commands)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    ++submit_calls;
    if (fail_next_submit != 0u) {
        --fail_next_submit;
        return RIN_GPU_ERROR_BACKEND;
    }
    return delegated_ops->submit_commands(context, commands, command_count);
}

static int injected_wait(void* context, uint64_t timeout_ns)
{
    if (!context || !delegated_ops || !delegated_ops->wait_for_completion)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    ++wait_calls;
    if (fail_next_wait != 0u) {
        --fail_next_wait;
        return RIN_GPU_ERROR_BACKEND;
    }
    return delegated_ops->wait_for_completion(context, timeout_ns);
}

static void make_runtime_desc(RinGpuRuntimeDescV1* desc,
                              const RinGpuBackendOpsV1* ops,
                              RinGpuSoftwareBackend* backend)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_RUNTIME_VERSION;
    desc->device_generation = 1u;
    desc->handle_secret = UINT64_C(0x445831314641494c);
    desc->max_buffer_size = UINT64_C(1) * 1024u * 1024u;
    desc->max_image_size = UINT64_C(1) * 1024u * 1024u;
    desc->max_total_allocation_size = UINT64_C(4) * 1024u * 1024u;
    desc->max_image_dimension = 64u;
    desc->max_image_layers = 1u;
    desc->max_image_mip_levels = 1u;
    desc->max_image_sample_count = 1u;
    desc->adapter.abi_version = RIN_GPU_ABI_VERSION;
    desc->adapter.struct_size = sizeof(desc->adapter);
    desc->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                       RIN_GPU_QUEUE_COMPUTE |
                                       RIN_GPU_QUEUE_GRAPHICS;
    memcpy(desc->adapter.name, "rindx-d3d11-fault", 18u);
    desc->flags = RIN_GPU_RUNTIME_FLAG_HEADLESS;
    desc->backend_ops = ops;
    desc->backend_context = backend;
    desc->backend_family = RIN_GPU_RUNTIME_BACKEND_FAMILY_VIRTIO;
}

int main(void)
{
    RinGpuSoftwareBackendDescV4 backend_desc;
    RinGpuSoftwareBackend* backend = NULL;
    RinGpuBackendOpsV1 ops;
    RinGpuRuntimeDescV1 runtime_desc;
    RinDxD3d11Device device;
    RinDxD3d11Context immediate_context;
    RinDxD3d11Context deferred_context;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuImageDescV1 image_desc;
    RinGpuDxgiSwapchainDescV1 swapchain_desc;
    RinGpuDxgiWindowOwnerV1 window_owner;
    RinGpuPresentationBackendV1 presentation_backend;
    RinGpuDxgiSwapchainRuntime swapchain_runtime;
    RinDxSwapchainFailureProbe owner_probe = {0};
    RinDxD3d11Device swapchain_device;
    RinGpuHandle buffer = UINT64_C(0xdeadbeef);
    RinGpuHandle image = UINT64_C(0xdeadbeef);
    RinGpuHandle command_list = 0u;
    uint64_t fence_value = UINT64_MAX;
    const uint32_t feature_level = RIN_DX_D3D11_FEATURE_LEVEL_11_0;

    memset(&backend_desc, 0, sizeof(backend_desc));
    backend_desc.base.base.base.struct_size = sizeof(backend_desc);
    backend_desc.base.base.base.version =
        RIN_GPU_SOFTWARE_BACKEND_VERSION_4;
    backend_desc.base.base.base.max_total_bytes = 4u * 1024u * 1024u;
    backend_desc.base.base.base.flags = RIN_GPU_SOFTWARE_BACKEND_FLAG_HEADLESS;
    CHECK(ringpu_software_backend_create(&backend_desc.base.base.base,
                                         &backend) == RIN_GPU_OK);

    delegated_ops = ringpu_software_backend_ops();
    CHECK(delegated_ops != NULL && delegated_ops->create_buffer != NULL &&
          delegated_ops->create_image != NULL &&
          delegated_ops->submit_commands != NULL &&
          delegated_ops->wait_for_completion != NULL);
    ops = *delegated_ops;
    ops.create_buffer = injected_create_buffer;
    ops.destroy_buffer = injected_destroy_buffer;
    ops.create_image = injected_create_image;
    ops.destroy_image = injected_destroy_image;
    ops.submit_commands = injected_submit;
    ops.wait_for_completion = injected_wait;
    make_runtime_desc(&runtime_desc, &ops, backend);

    rindx_swapchain_test_make_inputs(&swapchain_desc, &window_owner,
                                     &presentation_backend, &owner_probe);
    owner_probe.fail_validation = 1;
    memset(&swapchain_device, 0xa5, sizeof(swapchain_device));
    memset(&swapchain_runtime, 0xa5, sizeof(swapchain_runtime));
    CHECK(rindx_d3d11_create_device_and_swapchain(
              &runtime_desc, &feature_level, 1u, &swapchain_desc,
              &window_owner, &presentation_backend, &swapchain_device,
              &swapchain_runtime) == RIN_GPU_ERROR_BACKEND);
    {
        const RinDxD3d11Device zero_device = {0};
        const RinGpuDxgiSwapchainRuntime zero_swapchain = {0};
        CHECK(memcmp(&swapchain_device, &zero_device,
                     sizeof(swapchain_device)) == 0);
        CHECK(memcmp(&swapchain_runtime, &zero_swapchain,
                     sizeof(swapchain_runtime)) == 0);
    }
    CHECK(owner_probe.validate_calls == 1u &&
          owner_probe.retain_calls == 0u && owner_probe.release_calls == 0u &&
          owner_probe.present_calls == 0u);
    owner_probe.fail_validation = 0;

    owner_probe.fail_retain = 1;
    memset(&swapchain_device, 0xa5, sizeof(swapchain_device));
    memset(&swapchain_runtime, 0xa5, sizeof(swapchain_runtime));
    CHECK(rindx_d3d11_create_device_and_swapchain(
              &runtime_desc, &feature_level, 1u, &swapchain_desc,
              &window_owner, &presentation_backend, &swapchain_device,
              &swapchain_runtime) == RIN_GPU_ERROR_BACKEND);
    {
        const RinDxD3d11Device zero_device = {0};
        const RinGpuDxgiSwapchainRuntime zero_swapchain = {0};
        CHECK(memcmp(&swapchain_device, &zero_device,
                     sizeof(swapchain_device)) == 0);
        CHECK(memcmp(&swapchain_runtime, &zero_swapchain,
                     sizeof(swapchain_runtime)) == 0);
    }
    CHECK(owner_probe.validate_calls == 2u &&
          owner_probe.retain_calls == 1u && owner_probe.release_calls == 0u &&
          owner_probe.present_calls == 0u);
    owner_probe.fail_retain = 0;

    presentation_backend.submit = NULL;
    memset(&swapchain_device, 0xa5, sizeof(swapchain_device));
    memset(&swapchain_runtime, 0xa5, sizeof(swapchain_runtime));
    CHECK(rindx_d3d11_create_device_and_swapchain(
              &runtime_desc, &feature_level, 1u, &swapchain_desc,
              &window_owner, &presentation_backend, &swapchain_device,
              &swapchain_runtime) == RIN_GPU_ERROR_BACKEND);
    {
        const RinDxD3d11Device zero_device = {0};
        const RinGpuDxgiSwapchainRuntime zero_swapchain = {0};
        CHECK(memcmp(&swapchain_device, &zero_device,
                     sizeof(swapchain_device)) == 0);
        CHECK(memcmp(&swapchain_runtime, &zero_swapchain,
                     sizeof(swapchain_runtime)) == 0);
    }
    CHECK(owner_probe.validate_calls == 3u &&
          owner_probe.retain_calls == 2u && owner_probe.release_calls == 1u &&
          owner_probe.present_calls == 0u);
    presentation_backend.submit = rindx_swapchain_test_present;

    CHECK(rindx_d3d11_create_device_and_swapchain(
              &runtime_desc, &feature_level, 1u, &swapchain_desc,
              &window_owner, &presentation_backend, &swapchain_device,
              &swapchain_runtime) == RIN_GPU_OK);
    CHECK(owner_probe.validate_calls == 4u && owner_probe.retain_calls == 3u &&
          owner_probe.release_calls == 1u);
    CHECK(rin_gpu_dxgi_swapchain_runtime_shutdown(&swapchain_runtime) ==
          RIN_GPU_DXGI_SWAPCHAIN_OK);
    CHECK(owner_probe.release_calls == 2u && owner_probe.present_calls == 0u);
    CHECK(rindx_d3d11_destroy_device(&swapchain_device) == RIN_GPU_OK);

    CHECK(rindx_d3d11_create_device(&runtime_desc, &feature_level, 1u,
                                   &device) == RIN_GPU_OK);
    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = 16u;
    buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                        RIN_GPU_BUFFER_COPY_DESTINATION;
    buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;

    fail_next_create = 1u;
    create_calls = 0u;
    delegated_create_calls = 0u;
    CHECK(rindx_d3d11_create_buffer(&device, &buffer_desc, &buffer) ==
          RIN_GPU_ERROR_NO_MEMORY);
    CHECK(buffer == 0u);
    CHECK(create_calls == 1u && delegated_create_calls == 0u);

    CHECK(rindx_d3d11_create_buffer(&device, &buffer_desc, &buffer) ==
          RIN_GPU_OK);
    CHECK(buffer != 0u && create_calls == 2u &&
          delegated_create_calls == 1u);
    CHECK(rindx_d3d11_destroy_object(&device, buffer) == RIN_GPU_OK);

    {
        uint32_t destroys_before = destroyed_buffer_calls;
        fail_next_partial_create = 1u;
        create_calls = 0u;
        delegated_create_calls = 0u;
        buffer = UINT64_MAX;
        CHECK(rindx_d3d11_create_buffer(&device, &buffer_desc, &buffer) ==
              RIN_GPU_ERROR_BACKEND);
        CHECK(buffer == 0u && create_calls == 1u &&
              delegated_create_calls == 1u);
        CHECK(destroyed_buffer_calls == destroys_before + 1u);
        CHECK(rindx_d3d11_create_buffer(&device, &buffer_desc, &buffer) ==
              RIN_GPU_OK);
        CHECK(buffer != 0u && create_calls == 2u &&
              delegated_create_calls == 2u);
        CHECK(rindx_d3d11_destroy_object(&device, buffer) == RIN_GPU_OK);
        CHECK(destroyed_buffer_calls == destroys_before + 2u);
    }

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
                       RIN_GPU_IMAGE_COPY_DESTINATION;
    fail_next_image_create = 1u;
    image_create_calls = 0u;
    delegated_image_create_calls = 0u;
    CHECK(rindx_d3d11_create_texture2d(&device, &image_desc, &image) ==
          RIN_GPU_ERROR_NO_MEMORY);
    CHECK(image == 0u);
    CHECK(image_create_calls == 1u && delegated_image_create_calls == 0u);
    CHECK(rindx_d3d11_create_texture2d(&device, &image_desc, &image) ==
          RIN_GPU_OK);
    CHECK(image != 0u && image_create_calls == 2u &&
          delegated_image_create_calls == 1u);
    CHECK(rindx_d3d11_destroy_object(&device, image) == RIN_GPU_OK);

    {
        uint32_t destroys_before = destroyed_image_calls;
        fail_next_partial_image_create = 1u;
        image_create_calls = 0u;
        delegated_image_create_calls = 0u;
        image = UINT64_MAX;
        CHECK(rindx_d3d11_create_texture2d(&device, &image_desc, &image) ==
              RIN_GPU_ERROR_BACKEND);
        CHECK(image == 0u && image_create_calls == 1u &&
              delegated_image_create_calls == 1u);
        CHECK(destroyed_image_calls == destroys_before + 1u);
        CHECK(rindx_d3d11_create_texture2d(&device, &image_desc, &image) ==
              RIN_GPU_OK);
        CHECK(image != 0u && image_create_calls == 2u &&
              delegated_image_create_calls == 2u);
        CHECK(rindx_d3d11_destroy_object(&device, image) == RIN_GPU_OK);
        CHECK(destroyed_image_calls == destroys_before + 2u);
    }

    CHECK(rindx_d3d11_create_context(&device, &immediate_context) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_create_deferred_context(&device, &deferred_context) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d11_finish_command_list(&deferred_context, &command_list) ==
          RIN_GPU_OK);
    CHECK(command_list != 0u);
    fail_next_submit = 1u;
    submit_calls = 0u;
    CHECK(rindx_d3d11_execute_command_list(&immediate_context, command_list,
                                           0u, &fence_value) ==
          RIN_GPU_ERROR_BACKEND);
    CHECK(fence_value == 0u && device.submission_value == 0u &&
          submit_calls == 1u);
    CHECK(rindx_d3d11_execute_command_list(&immediate_context, command_list,
                                           0u, &fence_value) == RIN_GPU_OK);
    CHECK(fence_value == 1u && device.submission_value == 1u &&
          submit_calls == 2u);

    fail_next_wait = 1u;
    wait_calls = 0u;
    CHECK(rindx_d3d11_wait(&device, fence_value, UINT64_MAX) ==
          RIN_GPU_ERROR_BACKEND);
    CHECK(rindx_d3d11_wait(&device, fence_value, UINT64_MAX) == RIN_GPU_OK);
    CHECK(wait_calls == 2u);

    CHECK(rindx_d3d11_destroy_context(&immediate_context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_context(&deferred_context) == RIN_GPU_OK);
    CHECK(rindx_d3d11_destroy_device(&device) == RIN_GPU_OK);
    ringpu_software_backend_destroy(backend);
    return 0;
}
