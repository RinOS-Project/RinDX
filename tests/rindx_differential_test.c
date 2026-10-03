/* SPDX-License-Identifier: MIT */
#include <rindx/d3d12.h>
#include <ringpu/runtime.h>
#include <ringpu/software.h>

#include "differential.h"

#include <stdio.h>
#include <string.h>

#define READBACK_BYTES 16u

typedef struct RinGpuAdapterContext {
    RinGpuRuntime* runtime;
    uint8_t readback[READBACK_BYTES];
} RinGpuAdapterContext;

typedef struct RinDxAdapterContext {
    RinDxD3d12Device* device;
    uint8_t readback[READBACK_BYTES];
} RinDxAdapterContext;

static int create_software_backend(RinGpuSoftwareBackend** backend_out)
{
    RinGpuSoftwareBackendDescV4 desc;
    memset(&desc, 0, sizeof(desc));
    desc.base.base.base.struct_size = sizeof(desc);
    desc.base.base.base.version = RIN_GPU_SOFTWARE_BACKEND_VERSION_4;
    desc.base.base.base.max_total_bytes = UINT64_C(4) * 1024u * 1024u;
    desc.base.base.base.flags = RIN_GPU_SOFTWARE_BACKEND_FLAG_HEADLESS;
    return ringpu_software_backend_create(&desc.base.base.base, backend_out);
}

static void make_runtime_desc(RinGpuRuntimeDescV1* desc,
                              const void* backend_ops,
                              RinGpuSoftwareBackend* backend,
                              uint64_t secret,
                              uint32_t backend_family)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_RUNTIME_VERSION;
    desc->device_generation = 1u;
    desc->handle_secret = secret;
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
    memcpy(desc->adapter.name, "differential-host", sizeof("differential-host"));
    desc->flags = RIN_GPU_RUNTIME_FLAG_HEADLESS;
    desc->backend_ops = backend_ops;
    desc->backend_context = backend;
    desc->backend_family = backend_family;
}

static int run_ringpu_workload(RinGpuRuntime* runtime,
                               const RinGpuDifferentialWorkloadV1* workload,
                               uint8_t output[READBACK_BYTES])
{
    RinGpuQueueDescV1 queue_desc;
    RinGpuCommandListDescV1 list_desc;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuBufferClearV1 clear;
    RinGpuSubmitInfoV1 submit;
    RinGpuHandle queue = 0u;
    RinGpuHandle list = 0u;
    RinGpuHandle fence = 0u;
    RinGpuHandle buffer = 0u;
    static const uint8_t zeroes[READBACK_BYTES] = {0u};
    int result;

    if (runtime == NULL || workload == NULL || output == NULL ||
        workload->command_stream == NULL ||
        workload->command_stream_bytes != sizeof(clear.pattern))
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.abi_version = RIN_GPU_ABI_VERSION;
    queue_desc.struct_size = sizeof(queue_desc);
    queue_desc.capabilities = RIN_GPU_QUEUE_COPY;
    result = ringpu_runtime_create_queue(runtime, &queue_desc, &queue);
    if (result != RIN_GPU_OK) return result;

    memset(&list_desc, 0, sizeof(list_desc));
    list_desc.abi_version = RIN_GPU_ABI_VERSION;
    list_desc.struct_size = sizeof(list_desc);
    list_desc.capabilities = RIN_GPU_QUEUE_COPY;
    result = ringpu_runtime_create_command_list(runtime, &list_desc, &list);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_create_fence(runtime, 0u, &fence);
    if (result != RIN_GPU_OK) return result;

    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = READBACK_BYTES;
    buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                        RIN_GPU_BUFFER_COPY_DESTINATION;
    buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    result = ringpu_runtime_create_buffer(runtime, &buffer_desc, &buffer);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_upload_buffer(runtime, buffer, 0u, zeroes,
                                          sizeof(zeroes));
    if (result != RIN_GPU_OK) return result;

    memset(&clear, 0, sizeof(clear));
    clear.abi_version = RIN_GPU_ABI_VERSION;
    clear.struct_size = sizeof(clear);
    clear.size_bytes = READBACK_BYTES;
    memcpy(&clear.pattern, workload->command_stream, sizeof(clear.pattern));
    result = ringpu_runtime_command_clear_buffer(runtime, list, buffer, &clear);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_command_list_close(runtime, list);
    if (result != RIN_GPU_OK) return result;

    memset(&submit, 0, sizeof(submit));
    submit.abi_version = RIN_GPU_ABI_VERSION;
    submit.struct_size = sizeof(submit);
    submit.command_list = list;
    submit.signal_fence = fence;
    submit.signal_value = 1u;
    result = ringpu_runtime_queue_submit(runtime, queue, &submit);
    if (result != RIN_GPU_OK) return result;
    result = ringpu_runtime_wait_fence(runtime, fence, 1u,
                                       RIN_GPU_TIMEOUT_INFINITE);
    if (result != RIN_GPU_OK) return result;
    return ringpu_runtime_readback_buffer(runtime, buffer, 0u, output,
                                          READBACK_BYTES);
}

static int run_d3d12_workload(RinDxD3d12Device* device,
                              const RinGpuDifferentialWorkloadV1* workload,
                              uint8_t output[READBACK_BYTES])
{
    RinDxD3d12CommandAllocator allocator;
    RinDxD3d12CommandList list;
    RinGpuBufferDescV1 buffer_desc;
    RinGpuBufferClearV1 clear;
    RinGpuHandle buffer = 0u;
    uint64_t fence_value = 0u;
    static const uint8_t zeroes[READBACK_BYTES] = {0u};
    int result;

    if (device == NULL || workload == NULL || output == NULL ||
        workload->command_stream == NULL ||
        workload->command_stream_bytes != sizeof(clear.pattern))
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    memset(&allocator, 0, sizeof(allocator));
    memset(&list, 0, sizeof(list));
    result = rindx_d3d12_create_command_allocator(device, &allocator);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_create_command_list(device, &allocator, &list);
    if (result != RIN_GPU_OK) return result;

    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = READBACK_BYTES;
    buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                        RIN_GPU_BUFFER_COPY_DESTINATION;
    buffer_desc.flags = RIN_GPU_BUFFER_CPU_VISIBLE;
    result = rindx_d3d12_create_buffer(device, &buffer_desc, &buffer);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_upload_buffer(device, buffer, 0u, zeroes,
                                       sizeof(zeroes));
    if (result != RIN_GPU_OK) return result;

    memset(&clear, 0, sizeof(clear));
    clear.abi_version = RIN_GPU_ABI_VERSION;
    clear.struct_size = sizeof(clear);
    clear.size_bytes = READBACK_BYTES;
    memcpy(&clear.pattern, workload->command_stream, sizeof(clear.pattern));
    result = rindx_d3d12_clear_buffer(&list, buffer, &clear);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_close_command_list(&list);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_execute_command_lists(device, &list, &fence_value);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_wait(device, fence_value, RIN_GPU_TIMEOUT_INFINITE);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_readback_buffer(device, buffer, 0u, output,
                                         READBACK_BYTES);
    if (result != RIN_GPU_OK) return result;

    result = rindx_d3d12_destroy_command_list(&list);
    if (result != RIN_GPU_OK) return result;
    result = rindx_d3d12_destroy_object(device, buffer);
    if (result != RIN_GPU_OK) return result;
    return rindx_d3d12_destroy_command_allocator(&allocator);
}

static void fill_readback_snapshot(
    const RinGpuDifferentialWorkloadV1* workload,
    const uint8_t* readback, RinGpuDifferentialSnapshotV1* snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->struct_size = sizeof(*snapshot);
    snapshot->version = RIN_GPU_DIFFERENTIAL_WORKLOAD_VERSION;
    snapshot->valid_fields = workload->expected_fields;
    snapshot->backend.struct_size = sizeof(snapshot->backend);
    snapshot->backend.version = RIN_GPU_DIFFERENTIAL_BACKEND_STATS_VERSION;
    snapshot->backend.valid_fields =
        RIN_GPU_DIFFERENTIAL_BACKEND_FIELD_DETERMINISTIC_SEED;
    snapshot->backend.deterministic_seed = workload->deterministic_seed;
    snapshot->readback = readback;
    snapshot->readback_bytes = READBACK_BYTES;
}

static int run_ringpu_adapter(
    void* context, const RinGpuDifferentialWorkloadV1* workload,
    RinGpuDifferentialSnapshotV1* snapshot_out)
{
    RinGpuAdapterContext* adapter = (RinGpuAdapterContext*)context;
    int result;
    if (adapter == NULL || snapshot_out == NULL)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    result = run_ringpu_workload(adapter->runtime, workload,
                                 adapter->readback);
    if (result != RIN_GPU_OK) return result;
    fill_readback_snapshot(workload, adapter->readback, snapshot_out);
    return RIN_GPU_OK;
}

static int run_rindx_adapter(
    void* context, const RinGpuDifferentialWorkloadV1* workload,
    RinGpuDifferentialSnapshotV1* snapshot_out)
{
    RinDxAdapterContext* adapter = (RinDxAdapterContext*)context;
    int result;
    if (adapter == NULL || snapshot_out == NULL)
        return RIN_GPU_ERROR_INVALID_ARGUMENT;
    result = run_d3d12_workload(adapter->device, workload, adapter->readback);
    if (result != RIN_GPU_OK) return result;
    fill_readback_snapshot(workload, adapter->readback, snapshot_out);
    return RIN_GPU_OK;
}

int main(void)
{
    RinGpuSoftwareBackend* rindx_backend = NULL;
    RinGpuRuntimeDescV1 runtime_desc;
    RinGpuRuntime* runtime = NULL;
    RinDxD3d12Device d3d12_device;
    RinGpuAdapterContext ringpu_context;
    RinDxAdapterContext rindx_context;
    RinGpuDifferentialBackendAdapterV1 ringpu_adapter;
    RinGpuDifferentialBackendAdapterV1 rindx_adapter;
    RinGpuDifferentialWorkloadV1 workload;
    RinGpuDifferentialPolicyV1 policy;
    RinGpuDifferentialReportV1 report;
    const uint32_t feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;
    const uint32_t clear_pattern = UINT32_C(0xa55a3cc3);
    uint8_t expected_pattern[sizeof(clear_pattern)];
    uint8_t expected_readback[READBACK_BYTES];
    uint32_t index;
    int result;

    memset(&d3d12_device, 0, sizeof(d3d12_device));
    if (create_software_backend(&rindx_backend) != RIN_GPU_OK) {
        fprintf(stderr, "software backend creation failed\n");
        if (rindx_backend) ringpu_software_backend_destroy(rindx_backend);
        return 1;
    }

    make_runtime_desc(&runtime_desc, NULL, NULL,
                      UINT64_C(0x43524f5353415049),
                      RIN_GPU_RUNTIME_BACKEND_FAMILY_UNKNOWN);
    result = ringpu_runtime_create(&runtime_desc, &runtime);
    if (result != RIN_GPU_OK) {
        fprintf(stderr, "RinGPU runtime creation failed: %d\n", result);
        ringpu_software_backend_destroy(rindx_backend);
        return 2;
    }

    make_runtime_desc(&runtime_desc, ringpu_software_backend_ops(),
                      rindx_backend, UINT64_C(0x52494e445843524f),
                      RIN_GPU_RUNTIME_BACKEND_FAMILY_VIRTIO);
    result = rindx_d3d12_create_device(&runtime_desc, &feature_level, 1u,
                                       &d3d12_device);
    if (result != RIN_GPU_OK) {
        fprintf(stderr, "RinDX D3D12 runtime creation failed: %d\n", result);
        ringpu_runtime_destroy(runtime);
        ringpu_software_backend_destroy(rindx_backend);
        return 3;
    }

    memset(&workload, 0, sizeof(workload));
    workload.struct_size = sizeof(workload);
    workload.version = RIN_GPU_DIFFERENTIAL_WORKLOAD_VERSION;
    memcpy(workload.id, "buffer-clear", sizeof("buffer-clear"));
    workload.command_stream = (const uint8_t*)&clear_pattern;
    workload.command_stream_bytes = sizeof(clear_pattern);
    workload.expected_fields =
        RIN_GPU_DIFFERENTIAL_SNAPSHOT_FIELD_READBACK;
    workload.deterministic_seed = UINT64_C(0x52494e4750553031);
    if (rin_gpu_differential_policy_init(&policy, 0u, 0.0f, 0.0f, 0.0f) != 0 ||
        rin_gpu_differential_report_init(&report) !=
            RIN_GPU_DIFFERENTIAL_MATCH) {
        fprintf(stderr, "differential configuration failed\n");
        result = 4;
        goto cleanup;
    }

    memset(&ringpu_context, 0, sizeof(ringpu_context));
    ringpu_context.runtime = runtime;
    memset(&rindx_context, 0, sizeof(rindx_context));
    rindx_context.device = &d3d12_device;
    memset(&ringpu_adapter, 0, sizeof(ringpu_adapter));
    ringpu_adapter.struct_size = sizeof(ringpu_adapter);
    ringpu_adapter.version = RIN_GPU_DIFFERENTIAL_WORKLOAD_VERSION;
    ringpu_adapter.run = run_ringpu_adapter;
    ringpu_adapter.context = &ringpu_context;
    memset(&rindx_adapter, 0, sizeof(rindx_adapter));
    rindx_adapter.struct_size = sizeof(rindx_adapter);
    rindx_adapter.version = RIN_GPU_DIFFERENTIAL_WORKLOAD_VERSION;
    rindx_adapter.run = run_rindx_adapter;
    rindx_adapter.context = &rindx_context;

    result = rin_gpu_differential_run_pair(
        &workload, &ringpu_adapter, &rindx_adapter, &policy, &report);
    memcpy(expected_pattern, &clear_pattern, sizeof(clear_pattern));
    for (index = 0u; index < READBACK_BYTES; ++index)
        expected_readback[index] = expected_pattern[
            index % sizeof(expected_pattern)];
    if (result != RIN_GPU_DIFFERENTIAL_MATCH ||
        report.mismatched_values != 0u ||
        memcmp(ringpu_context.readback, expected_readback,
               sizeof(expected_readback)) != 0 ||
        memcmp(rindx_context.readback, expected_readback,
               sizeof(expected_readback)) != 0) {
        fprintf(stderr, "cross-backend differential mismatch: result=%d flags=%u mismatches=%llu\n",
                result, report.mismatch_flags,
                (unsigned long long)report.mismatched_values);
        result = 5;
    } else {
        result = 0;
    }

cleanup:
    if (rindx_d3d12_destroy_device(&d3d12_device) != RIN_GPU_OK &&
        result == 0)
        result = 6;
    ringpu_runtime_destroy(runtime);
    ringpu_software_backend_destroy(rindx_backend);
    return result;
}
