/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_SWAPCHAIN_H
#define RINDX_PUBLIC_SWAPCHAIN_H

#include <ringpu/presentation.h>

#include <stdint.h>

#define RIN_GPU_DXGI_SWAPCHAIN_VERSION 1u
#define RIN_GPU_DXGI_SWAPCHAIN_RUNTIME_STATE_QWORDS 4608u
#define RIN_GPU_DXGI_SWAPCHAIN_MIN_BUFFERS 2u
#define RIN_GPU_DXGI_SWAPCHAIN_MAX_BUFFERS 8u

#define RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING UINT32_C(0x00000001)
#define RIN_GPU_DXGI_SWAPCHAIN_FULLSCREEN UINT32_C(0x00000002)
#define RIN_GPU_DXGI_SWAPCHAIN_KNOWN_FLAGS \
    (RIN_GPU_DXGI_SWAPCHAIN_ALLOW_TEARING | RIN_GPU_DXGI_SWAPCHAIN_FULLSCREEN)

typedef enum RinGpuDxgiSwapchainResult {
    RIN_GPU_DXGI_SWAPCHAIN_OK = 0,
    RIN_GPU_DXGI_SWAPCHAIN_INVALID_ARGUMENT = -1,
    RIN_GPU_DXGI_SWAPCHAIN_NOT_READY = -2,
    RIN_GPU_DXGI_SWAPCHAIN_OUT_OF_DATE = -3,
    RIN_GPU_DXGI_SWAPCHAIN_SUBOPTIMAL = -4,
    RIN_GPU_DXGI_SWAPCHAIN_WAS_STILL_DRAWING = -5,
    RIN_GPU_DXGI_SWAPCHAIN_BACKEND = -6,
    RIN_GPU_DXGI_SWAPCHAIN_DEVICE_LOST = -7,
    RIN_GPU_DXGI_SWAPCHAIN_UNSUPPORTED = -8,
    RIN_GPU_DXGI_SWAPCHAIN_LIMIT = -9
} RinGpuDxgiSwapchainResult;

typedef int (*RinGpuDxgiWindowRetainFn)(void* context, void* native_window);
typedef void (*RinGpuDxgiWindowReleaseFn)(void* context, void* native_window);
typedef int (*RinGpuDxgiWindowValidateFn)(void* context, void* native_window,
                                          uint32_t display_id);

typedef struct RinGpuDxgiWindowOwnerV1 {
    uint32_t struct_size;
    uint32_t version;
    void* context;
    void* native_window;
    RinGpuDxgiWindowRetainFn retain;
    RinGpuDxgiWindowReleaseFn release;
    RinGpuDxgiWindowValidateFn validate;
    uint64_t reserved[2];
} RinGpuDxgiWindowOwnerV1;

typedef struct RinGpuDxgiSwapchainDescV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t buffer_count;
    uint32_t flags;
    RinGpuPresentationOutputV1 output;
    uint64_t reserved[2];
} RinGpuDxgiSwapchainDescV1;

typedef struct RinGpuDxgiSwapchainRuntime {
    uint64_t opaque[RIN_GPU_DXGI_SWAPCHAIN_RUNTIME_STATE_QWORDS];
} RinGpuDxgiSwapchainRuntime;

typedef struct RinGpuDxgiSwapchainStatusV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t buffer_count;
    uint32_t flags;
    uint32_t output_changed;
    uint32_t tearing_supported;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t submitted_count;
    uint64_t completed_count;
    uint64_t reserved[1];
} RinGpuDxgiSwapchainStatusV1;

typedef struct RinGpuDxgiSwapchainBufferV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t image_token;
    uint32_t display_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t output_generation;
    uint64_t device_generation;
    uint64_t resource_handle;
    uint64_t reserved[1];
} RinGpuDxgiSwapchainBufferV1;

#ifdef __cplusplus
extern "C" {
#endif

int rin_gpu_dxgi_swapchain_runtime_init(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuDxgiSwapchainDescV1* descriptor,
    const RinGpuDxgiWindowOwnerV1* window_owner,
    const RinGpuPresentationBackendV1* presentation_backend);
int rin_gpu_dxgi_swapchain_runtime_shutdown(
    RinGpuDxgiSwapchainRuntime* runtime);
int rin_gpu_dxgi_swapchain_acquire(
    RinGpuDxgiSwapchainRuntime* runtime,
    RinGpuPresentationAcquireV1* acquire_out);
int rin_gpu_dxgi_swapchain_get_buffer(
    RinGpuDxgiSwapchainRuntime* runtime, uint32_t index,
    RinGpuDxgiSwapchainBufferV1* buffer_out);
int rin_gpu_dxgi_swapchain_bind_buffer(
    RinGpuDxgiSwapchainRuntime* runtime, uint64_t image_token,
    uint64_t resource_handle);
int rin_gpu_dxgi_swapchain_unbind_buffer(
    RinGpuDxgiSwapchainRuntime* runtime, uint64_t image_token,
    uint64_t resource_handle);
int rin_gpu_dxgi_swapchain_present(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationSubmitV1* submit, uint64_t* fence_value_out);
int rin_gpu_dxgi_swapchain_complete(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationCompletionV1* completion);
int rin_gpu_dxgi_swapchain_resize_buffers(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationOutputV1* output, uint32_t buffer_count);
int rin_gpu_dxgi_swapchain_set_fullscreen(
    RinGpuDxgiSwapchainRuntime* runtime, uint32_t fullscreen,
    const RinGpuPresentationOutputV1* output);
int rin_gpu_dxgi_swapchain_notify_output_change(
    RinGpuDxgiSwapchainRuntime* runtime,
    const RinGpuPresentationOutputV1* observed_output);
int rin_gpu_dxgi_swapchain_device_lost(RinGpuDxgiSwapchainRuntime* runtime);
int rin_gpu_dxgi_swapchain_device_reset(
    RinGpuDxgiSwapchainRuntime* runtime, uint64_t next_device_generation);
int rin_gpu_dxgi_swapchain_get_status(
    RinGpuDxgiSwapchainRuntime* runtime,
    RinGpuDxgiSwapchainStatusV1* status_out);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(RinGpuDxgiWindowOwnerV1) ==
                  (sizeof(void*) == 8u ? 64u : 44u),
              "RinGPU DXGI window owner drift");
static_assert(sizeof(RinGpuDxgiSwapchainDescV1) == 96u,
              "RinGPU DXGI swapchain descriptor drift");
static_assert(sizeof(RinGpuDxgiSwapchainStatusV1) == 64u,
              "RinGPU DXGI swapchain status drift");
static_assert(sizeof(RinGpuDxgiSwapchainBufferV1) == 64u,
              "RinGPU DXGI swapchain buffer drift");
#else
_Static_assert(sizeof(RinGpuDxgiWindowOwnerV1) ==
                   (sizeof(void*) == 8u ? 64u : 44u),
               "RinGPU DXGI window owner drift");
_Static_assert(sizeof(RinGpuDxgiSwapchainDescV1) == 96u,
               "RinGPU DXGI swapchain descriptor drift");
_Static_assert(sizeof(RinGpuDxgiSwapchainStatusV1) == 64u,
               "RinGPU DXGI swapchain status drift");
_Static_assert(sizeof(RinGpuDxgiSwapchainBufferV1) == 64u,
               "RinGPU DXGI swapchain buffer drift");
#endif

#endif /* RINDX_PUBLIC_SWAPCHAIN_H */
