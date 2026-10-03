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

#define LIFETIME_CYCLES 4096u

typedef enum TrackedType {
    TRACKED_BUFFER = 0,
    TRACKED_IMAGE = 1,
    TRACKED_SAMPLER = 2,
    TRACKED_TYPE_COUNT = 3
} TrackedType;

typedef struct LifetimeTelemetry {
    uint32_t created[TRACKED_TYPE_COUNT];
    uint32_t destroyed[TRACKED_TYPE_COUNT];
    uint32_t active[TRACKED_TYPE_COUNT];
    uint32_t active_total;
    uint32_t peak_active;
    uint32_t invalid_retirement;
} LifetimeTelemetry;

static LifetimeTelemetry telemetry;
static const RinGpuBackendOpsV1* delegate_ops;

static void record_create(TrackedType type)
{
    ++telemetry.created[type];
    ++telemetry.active[type];
    ++telemetry.active_total;
    if (telemetry.active_total > telemetry.peak_active)
        telemetry.peak_active = telemetry.active_total;
}

static void record_destroy(TrackedType type)
{
    ++telemetry.destroyed[type];
    if (telemetry.active[type] == 0u || telemetry.active_total == 0u) {
        telemetry.invalid_retirement = 1u;
        return;
    }
    --telemetry.active[type];
    --telemetry.active_total;
}

static int tracked_create_buffer(void* context,
                                 const RinGpuBufferDescV1* desc,
                                 uint64_t* cookie)
{
    int result = delegate_ops->create_buffer(context, desc, cookie);
    if (result == RIN_GPU_OK) record_create(TRACKED_BUFFER);
    return result;
}

static void tracked_destroy_buffer(void* context, uint64_t cookie)
{
    delegate_ops->destroy_buffer(context, cookie);
    record_destroy(TRACKED_BUFFER);
}

static int tracked_create_image(void* context, const RinGpuImageDescV1* desc,
                                uint64_t allocation_bytes, uint64_t* cookie)
{
    int result = delegate_ops->create_image(context, desc, allocation_bytes,
                                            cookie);
    if (result == RIN_GPU_OK) record_create(TRACKED_IMAGE);
    return result;
}

static void tracked_destroy_image(void* context, uint64_t cookie)
{
    delegate_ops->destroy_image(context, cookie);
    record_destroy(TRACKED_IMAGE);
}

static int tracked_create_sampler(void* context,
                                  const RinGpuSamplerDescV1* desc,
                                  uint64_t* cookie)
{
    int result = delegate_ops->create_sampler(context, desc, cookie);
    if (result == RIN_GPU_OK) record_create(TRACKED_SAMPLER);
    return result;
}

static void tracked_destroy_sampler(void* context, uint64_t cookie)
{
    delegate_ops->destroy_sampler(context, cookie);
    record_destroy(TRACKED_SAMPLER);
}

static void make_runtime_desc(RinGpuRuntimeDescV1* desc,
                              const RinGpuBackendOpsV1* ops,
                              RinGpuSoftwareBackend* backend)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_RUNTIME_VERSION;
    desc->device_generation = 1u;
    desc->handle_secret = UINT64_C(0x445831324c494645);
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
    memcpy(desc->adapter.name, "rindx-d3d12-life", 17u);
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
    RinGpuBufferDescV1 buffer_desc;
    RinGpuImageDescV1 image_desc;
    RinGpuSamplerDescV1 sampler_desc;
    const uint32_t feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;
    uint32_t active_handles = 0u;
    uint32_t peak_handles = 0u;

    memset(&backend_desc, 0, sizeof(backend_desc));
    backend_desc.base.base.base.struct_size = sizeof(backend_desc);
    backend_desc.base.base.base.version =
        RIN_GPU_SOFTWARE_BACKEND_VERSION_4;
    backend_desc.base.base.base.max_total_bytes = 4u * 1024u * 1024u;
    backend_desc.base.base.base.flags = RIN_GPU_SOFTWARE_BACKEND_FLAG_HEADLESS;
    CHECK(ringpu_software_backend_create(&backend_desc.base.base.base,
                                         &backend) == RIN_GPU_OK);
    delegate_ops = ringpu_software_backend_ops();
    CHECK(delegate_ops != NULL && delegate_ops->create_buffer != NULL &&
          delegate_ops->create_image != NULL &&
          delegate_ops->create_sampler != NULL);
    ops = *delegate_ops;
    ops.create_buffer = tracked_create_buffer;
    ops.destroy_buffer = tracked_destroy_buffer;
    ops.create_image = tracked_create_image;
    ops.destroy_image = tracked_destroy_image;
    ops.create_sampler = tracked_create_sampler;
    ops.destroy_sampler = tracked_destroy_sampler;
    make_runtime_desc(&runtime_desc, &ops, backend);
    CHECK(rindx_d3d12_create_device(&runtime_desc, &feature_level, 1u,
                                   &device) == RIN_GPU_OK);
    CHECK(rindx_d3d12_create_command_allocator(&device, &allocator) ==
          RIN_GPU_OK);

    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = 16u;
    buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                        RIN_GPU_BUFFER_COPY_DESTINATION;
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

    for (uint32_t iteration = 0u; iteration < LIFETIME_CYCLES; ++iteration) {
        RinGpuHandle buffer = 0u;
        RinGpuHandle image = 0u;
        RinGpuHandle sampler = 0u;
        RinDxD3d12CommandList list;

        CHECK(rindx_d3d12_create_buffer(&device, &buffer_desc, &buffer) ==
              RIN_GPU_OK && buffer != 0u);
        ++active_handles;
        CHECK(rindx_d3d12_create_texture2d(&device, &image_desc, &image) ==
              RIN_GPU_OK && image != 0u);
        ++active_handles;
        CHECK(rindx_d3d12_create_sampler(&device, &sampler_desc, &sampler) ==
              RIN_GPU_OK && sampler != 0u);
        ++active_handles;
        CHECK(rindx_d3d12_create_command_list(&device, &allocator, &list) ==
              RIN_GPU_OK);
        ++active_handles;
        if (active_handles > peak_handles) peak_handles = active_handles;
        CHECK(rindx_d3d12_close_command_list(&list) == RIN_GPU_OK);
        CHECK(rindx_d3d12_destroy_command_list(&list) == RIN_GPU_OK);
        --active_handles;
        CHECK(rindx_d3d12_destroy_object(&device, sampler) == RIN_GPU_OK);
        --active_handles;
        CHECK(rindx_d3d12_destroy_object(&device, image) == RIN_GPU_OK);
        --active_handles;
        CHECK(rindx_d3d12_destroy_object(&device, buffer) == RIN_GPU_OK);
        --active_handles;
    }

    CHECK(active_handles == 0u && peak_handles == 4u);
    CHECK(telemetry.invalid_retirement == 0u && telemetry.active_total == 0u &&
          telemetry.peak_active == 3u);
    for (uint32_t type = 0u; type < TRACKED_TYPE_COUNT; ++type) {
        CHECK(telemetry.created[type] == LIFETIME_CYCLES);
        CHECK(telemetry.destroyed[type] == LIFETIME_CYCLES);
        CHECK(telemetry.active[type] == 0u);
    }
    CHECK(rindx_d3d12_destroy_command_allocator(&allocator) == RIN_GPU_OK);
    CHECK(rindx_d3d12_destroy_device(&device) == RIN_GPU_OK);
    ringpu_software_backend_destroy(backend);
    return 0;
}
