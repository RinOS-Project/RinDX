/* SPDX-License-Identifier: MIT */
#include <rindx/d3d12.h>

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
static uint32_t submit_calls;
static uint32_t fail_next_submit;

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

static void make_runtime_desc(RinGpuRuntimeDescV1* desc,
                              const RinGpuBackendOpsV1* ops,
                              RinGpuSoftwareBackend* backend)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_RUNTIME_VERSION;
    desc->device_generation = 1u;
    desc->handle_secret = UINT64_C(0x445831324641494c);
    desc->max_buffer_size = 1024u * 1024u;
    desc->max_image_size = 1024u * 1024u;
    desc->max_total_allocation_size = 4u * 1024u * 1024u;
    desc->max_image_dimension = 64u;
    desc->max_image_layers = 1u;
    desc->max_image_mip_levels = 1u;
    desc->max_image_sample_count = 1u;
    desc->adapter.abi_version = RIN_GPU_ABI_VERSION;
    desc->adapter.struct_size = sizeof(desc->adapter);
    desc->adapter.queue_capabilities = RIN_GPU_QUEUE_COPY |
                                       RIN_GPU_QUEUE_COMPUTE |
                                       RIN_GPU_QUEUE_GRAPHICS;
    memcpy(desc->adapter.name, "rindx-d3d12-fault", 18u);
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
    RinDxD3d12Device device;
    RinDxD3d12CommandAllocator allocator;
    RinDxD3d12CommandList list;
    const uint32_t feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;
    const uint64_t untouched_fence_value = UINT64_C(0xfeedface);
    uint64_t fence_value = untouched_fence_value;
    uint64_t completed_fence_value = UINT64_MAX;

    memset(&backend_desc, 0, sizeof(backend_desc));
    backend_desc.base.base.base.struct_size = sizeof(backend_desc);
    backend_desc.base.base.base.version =
        RIN_GPU_SOFTWARE_BACKEND_VERSION_4;
    backend_desc.base.base.base.max_total_bytes = 4u * 1024u * 1024u;
    backend_desc.base.base.base.flags = RIN_GPU_SOFTWARE_BACKEND_FLAG_HEADLESS;
    CHECK(ringpu_software_backend_create(&backend_desc.base.base.base,
                                         &backend) == RIN_GPU_OK);

    delegated_ops = ringpu_software_backend_ops();
    CHECK(delegated_ops != NULL && delegated_ops->submit_commands != NULL);
    ops = *delegated_ops;
    ops.submit_commands = injected_submit;
    make_runtime_desc(&runtime_desc, &ops, backend);
    fail_next_submit = 1u;
    submit_calls = 0u;

    CHECK(rindx_d3d12_create_device(&runtime_desc, &feature_level, 1u,
                                   &device) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_command_allocator(&device, &allocator) ==
          RIN_GPU_OK);
    CHECK(rindx_d3d12_create_command_list(&device, &allocator, &list) ==
          RIN_GPU_OK);

    CHECK(rindx_d3d12_execute_command_lists(&device, &list, &fence_value) ==
          RIN_GPU_ERROR_BACKEND);
    CHECK(fence_value == untouched_fence_value);
    CHECK(device.submission_value == 0u);
    CHECK(ringpu_runtime_get_fence_value(device.runtime, device.fence,
                                         &completed_fence_value) == RIN_GPU_OK);
    CHECK(completed_fence_value == 0u);

    CHECK(rindx_d3d12_reset_command_list(&list) == RIN_GPU_OK);
    fence_value = 0u;
    CHECK(rindx_d3d12_execute_command_lists(&device, &list, &fence_value) ==
          RIN_GPU_OK);
    CHECK(fence_value == 1u && device.submission_value == 1u);
    CHECK(submit_calls == 2u);
    CHECK(rindx_d3d12_wait(&device, fence_value, UINT64_MAX) == RIN_GPU_OK);

    CHECK(rindx_d3d12_destroy_command_list(&list) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_command_allocator(&allocator) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_device(&device) == RIN_GPU_OK);
    ringpu_software_backend_destroy(backend);
    return 0;
}
