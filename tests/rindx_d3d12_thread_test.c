/* SPDX-License-Identifier: MIT */
#include <rindx/d3d12.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define THREAD_COUNT 4u
#define THREAD_ITERATIONS 2000u

typedef struct WorkerContext {
    RinDxD3d12Device* device;
    const RinGpuBufferDescV1* buffer_desc;
    RinGpuHandle pending_buffer;
    RinDxD3d12CommandAllocator allocator;
    RinDxD3d12CommandList pending_list;
    uint32_t completed;
    int failed;
} WorkerContext;

static void worker_run(WorkerContext* context)
{
    for (uint32_t iteration = 0u; iteration < THREAD_ITERATIONS; ++iteration) {
        int result;
        context->pending_buffer = 0u;
        result = rindx_d3d12_create_buffer(context->device,
                                           context->buffer_desc,
                                           &context->pending_buffer);
        if (result != RIN_GPU_OK || context->pending_buffer == 0u) {
            context->failed = 1;
            return;
        }
        result = rindx_d3d12_destroy_object(context->device,
                                            context->pending_buffer);
        if (result != RIN_GPU_OK) {
            context->failed = 1;
            return;
        }
        context->pending_buffer = 0u;

        result = rindx_d3d12_create_command_list(
            context->device, &context->allocator, &context->pending_list);
        if (result != RIN_GPU_OK) {
            context->failed = 1;
            return;
        }
        result = rindx_d3d12_close_command_list(&context->pending_list);
        if (result == RIN_GPU_OK)
            result = rindx_d3d12_reset_command_list(&context->pending_list);
        if (result == RIN_GPU_OK)
            result = rindx_d3d12_destroy_command_list(&context->pending_list);
        if (result != RIN_GPU_OK) {
            context->failed = 1;
            return;
        }
        ++context->completed;
    }
}

#if defined(_WIN32)
static DWORD WINAPI worker_entry(LPVOID opaque)
{
    worker_run((WorkerContext*)opaque);
    return 0u;
}
#else
static void* worker_entry(void* opaque)
{
    worker_run((WorkerContext*)opaque);
    return NULL;
}
#endif

static void make_runtime_desc(RinGpuRuntimeDescV1* desc)
{
    memset(desc, 0, sizeof(*desc));
    desc->struct_size = sizeof(*desc);
    desc->version = RIN_GPU_RUNTIME_VERSION;
    desc->device_generation = 1u;
    desc->handle_secret = UINT64_C(0x4458313254485244);
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
    memcpy(desc->adapter.name, "rindx-d3d12-thread", 19u);
    desc->flags = RIN_GPU_RUNTIME_FLAG_HEADLESS;
}

int main(void)
{
    RinGpuRuntimeDescV1 runtime_desc;
    RinGpuBufferDescV1 buffer_desc;
    RinDxD3d12Device device;
    WorkerContext contexts[THREAD_COUNT];
    uint32_t created = 0u;
    int success = 1;
    const uint32_t feature_level = RIN_DX_D3D12_FEATURE_LEVEL_12_0;

    make_runtime_desc(&runtime_desc);
    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.abi_version = RIN_GPU_ABI_VERSION;
    buffer_desc.struct_size = sizeof(buffer_desc);
    buffer_desc.size_bytes = 256u;
    buffer_desc.usage = RIN_GPU_BUFFER_COPY_SOURCE |
                        RIN_GPU_BUFFER_COPY_DESTINATION;
    memset(contexts, 0, sizeof(contexts));
    CHECK(rindx_d3d12_create_device(&runtime_desc, &feature_level, 1u,
                                    &device) == RIN_GPU_OK);
    for (uint32_t index = 0u; index < THREAD_COUNT; ++index) {
        contexts[index].device = &device;
        contexts[index].buffer_desc = &buffer_desc;
        CHECK(rindx_d3d12_create_command_allocator(
                  &device, &contexts[index].allocator) == RIN_GPU_OK);
    }

#if defined(_WIN32)
    {
        HANDLE threads[THREAD_COUNT] = {NULL};
        for (uint32_t index = 0u; index < THREAD_COUNT; ++index) {
            threads[index] = CreateThread(NULL, 0u, worker_entry,
                                          &contexts[index], 0u, NULL);
            if (!threads[index]) {
                success = 0;
                break;
            }
            ++created;
        }
        for (uint32_t index = 0u; index < created; ++index) {
            if (WaitForSingleObject(threads[index], INFINITE) != WAIT_OBJECT_0)
                success = 0;
            CloseHandle(threads[index]);
        }
    }
#else
    {
        pthread_t threads[THREAD_COUNT];
        for (uint32_t index = 0u; index < THREAD_COUNT; ++index) {
            if (pthread_create(&threads[index], NULL, worker_entry,
                               &contexts[index]) != 0) {
                success = 0;
                break;
            }
            ++created;
        }
        for (uint32_t index = 0u; index < created; ++index)
            if (pthread_join(threads[index], NULL) != 0) success = 0;
    }
#endif

    if (created != THREAD_COUNT) success = 0;
    for (uint32_t index = 0u; index < created; ++index) {
        if (contexts[index].failed ||
            contexts[index].completed != THREAD_ITERATIONS)
            success = 0;
        if (contexts[index].pending_buffer != 0u &&
            rindx_d3d12_destroy_object(&device,
                                       contexts[index].pending_buffer) !=
                RIN_GPU_OK)
            success = 0;
        if (contexts[index].pending_list.struct_size != 0u &&
            rindx_d3d12_destroy_command_list(
                &contexts[index].pending_list) != RIN_GPU_OK)
            success = 0;
        if (contexts[index].allocator.struct_size != 0u &&
            rindx_d3d12_destroy_command_allocator(
                &contexts[index].allocator) != RIN_GPU_OK)
            success = 0;
    }
    if (rindx_d3d12_destroy_device(&device) != RIN_GPU_OK) success = 0;
    CHECK(success);
    return 0;
}
