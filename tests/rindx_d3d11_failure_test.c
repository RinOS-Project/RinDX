/* SPDX-License-Identifier: MIT */
#include <rindx/d3d11.h>

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
    if (result == RIN_GPU_OK) ++delegated_create_calls;
    return result;
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
    RinGpuBufferDescV1 buffer_desc;
    RinGpuHandle buffer = UINT64_C(0xdeadbeef);
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
    CHECK(delegated_ops != NULL && delegated_ops->create_buffer != NULL);
    ops = *delegated_ops;
    ops.create_buffer = injected_create_buffer;
    make_runtime_desc(&runtime_desc, &ops, backend);

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
    CHECK(rindx_d3d11_destroy_device(&device) == RIN_GPU_OK);
    ringpu_software_backend_destroy(backend);
    return 0;
}
