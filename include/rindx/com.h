/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_COM_H
#define RINDX_PUBLIC_COM_H

#include <stddef.h>
#include <stdint.h>

#include <rindx/catalog.h>

#define RIN_GPU_DXGI_COM_VERSION 1u
#define RIN_GPU_DXGI_COM_STATE_QWORDS 4096u
#define RIN_GPU_DXGI_PRIVATE_DATA_MAX_BYTES 128u

#if defined(_MSC_VER) && defined(_M_IX86)
#define RIN_DXGI_STDCALL __stdcall
#elif defined(__i386__)
#define RIN_DXGI_STDCALL __attribute__((stdcall))
#else
#define RIN_DXGI_STDCALL
#endif

typedef int32_t RinDxgiHresult;
typedef int32_t RinDxgiBool;

typedef struct RinDxgiGuid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} RinDxgiGuid;

typedef struct RinDxgiLuid {
    uint32_t low_part;
    int32_t high_part;
} RinDxgiLuid;

typedef struct RinDxgiRect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} RinDxgiRect;

typedef struct RinDxgiAdapterDesc {
    uint16_t description[128];
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t subsystem_id;
    uint32_t revision;
    uintptr_t dedicated_video_memory;
    uintptr_t dedicated_system_memory;
    uintptr_t shared_system_memory;
    RinDxgiLuid adapter_luid;
} RinDxgiAdapterDesc;

typedef struct RinDxgiAdapterDesc1 {
    uint16_t description[128];
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t subsystem_id;
    uint32_t revision;
    uintptr_t dedicated_video_memory;
    uintptr_t dedicated_system_memory;
    uintptr_t shared_system_memory;
    RinDxgiLuid adapter_luid;
    uint32_t flags;
} RinDxgiAdapterDesc1;

typedef struct RinDxgiOutputDesc {
    uint16_t device_name[32];
    RinDxgiRect desktop_coordinates;
    RinDxgiBool attached_to_desktop;
    uint32_t rotation;
    void* monitor;
} RinDxgiOutputDesc;

typedef struct RinDxgiRational {
    uint32_t numerator;
    uint32_t denominator;
} RinDxgiRational;

typedef struct RinDxgiModeDesc {
    uint32_t width;
    uint32_t height;
    RinDxgiRational refresh_rate;
    uint32_t format;
    uint32_t scanline_ordering;
    uint32_t scaling;
} RinDxgiModeDesc;

typedef struct RinDxgiUnknown RinDxgiUnknown;
typedef struct RinDxgiAdapter RinDxgiAdapter;
typedef struct RinDxgiAdapter1 RinDxgiAdapter1;
typedef struct RinDxgiOutput RinDxgiOutput;
typedef struct RinDxgiFactory RinDxgiFactory;
typedef struct RinDxgiFactory1 RinDxgiFactory1;

typedef struct RinDxgiUnknownVtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiUnknown* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiUnknown* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiUnknown* self);
} RinDxgiUnknownVtbl;

struct RinDxgiUnknown {
    const RinDxgiUnknownVtbl* vtable;
};

#define RIN_DXGI_OBJECT_METHODS(interface_type)                              \
    RinDxgiHresult(RIN_DXGI_STDCALL *set_private_data)(                     \
        interface_type* self, const RinDxgiGuid* name, uint32_t size,       \
        const void* data);                                                  \
    RinDxgiHresult(RIN_DXGI_STDCALL *set_private_data_interface)(           \
        interface_type* self, const RinDxgiGuid* name,                      \
        const RinDxgiUnknown* object);                                      \
    RinDxgiHresult(RIN_DXGI_STDCALL *get_private_data)(                     \
        interface_type* self, const RinDxgiGuid* name, uint32_t* size,      \
        void* data);                                                        \
    RinDxgiHresult(RIN_DXGI_STDCALL *get_parent)(                           \
        interface_type* self, const RinDxgiGuid* iid, void** parent_out)

typedef struct RinDxgiAdapterVtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiAdapter* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiAdapter* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiAdapter* self);
    RIN_DXGI_OBJECT_METHODS(RinDxgiAdapter);
    RinDxgiHresult(RIN_DXGI_STDCALL *enum_outputs)(
        RinDxgiAdapter* self, uint32_t ordinal, RinDxgiOutput** output_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_desc)(
        RinDxgiAdapter* self, RinDxgiAdapterDesc* descriptor_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *check_interface_support)(
        RinDxgiAdapter* self, const RinDxgiGuid* interface_name,
        int64_t* user_mode_driver_version_out);
} RinDxgiAdapterVtbl;

struct RinDxgiAdapter {
    const RinDxgiAdapterVtbl* vtable;
};

typedef struct RinDxgiAdapter1Vtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiAdapter1* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiAdapter1* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiAdapter1* self);
    RIN_DXGI_OBJECT_METHODS(RinDxgiAdapter1);
    RinDxgiHresult(RIN_DXGI_STDCALL *enum_outputs)(
        RinDxgiAdapter1* self, uint32_t ordinal, RinDxgiOutput** output_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_desc)(
        RinDxgiAdapter1* self, RinDxgiAdapterDesc* descriptor_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *check_interface_support)(
        RinDxgiAdapter1* self, const RinDxgiGuid* interface_name,
        int64_t* user_mode_driver_version_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_desc1)(
        RinDxgiAdapter1* self, RinDxgiAdapterDesc1* descriptor_out);
} RinDxgiAdapter1Vtbl;

struct RinDxgiAdapter1 {
    const RinDxgiAdapter1Vtbl* vtable;
};

typedef struct RinDxgiOutputVtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiOutput* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiOutput* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiOutput* self);
    RIN_DXGI_OBJECT_METHODS(RinDxgiOutput);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_desc)(
        RinDxgiOutput* self, RinDxgiOutputDesc* descriptor_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_display_mode_list)(
        RinDxgiOutput* self, uint32_t format, uint32_t flags,
        uint32_t* mode_count, RinDxgiModeDesc* mode_descriptors);
    RinDxgiHresult(RIN_DXGI_STDCALL *find_closest_matching_mode)(
        RinDxgiOutput* self, const RinDxgiModeDesc* requested_mode,
        RinDxgiModeDesc* closest_mode, RinDxgiUnknown* concerned_device);
    RinDxgiHresult(RIN_DXGI_STDCALL *wait_for_vblank)(RinDxgiOutput* self);
    RinDxgiHresult(RIN_DXGI_STDCALL *take_ownership)(
        RinDxgiOutput* self, RinDxgiUnknown* device, RinDxgiBool exclusive);
    void(RIN_DXGI_STDCALL *release_ownership)(RinDxgiOutput* self);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_gamma_control_capabilities)(
        RinDxgiOutput* self, void* capabilities_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *set_gamma_control)(
        RinDxgiOutput* self, const void* gamma_control);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_gamma_control)(
        RinDxgiOutput* self, void* gamma_control_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *set_display_surface)(
        RinDxgiOutput* self, RinDxgiUnknown* surface);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_display_surface_data)(
        RinDxgiOutput* self, RinDxgiUnknown* destination);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_frame_statistics)(
        RinDxgiOutput* self, void* statistics_out);
} RinDxgiOutputVtbl;

struct RinDxgiOutput {
    const RinDxgiOutputVtbl* vtable;
};

typedef struct RinDxgiFactoryVtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiFactory* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiFactory* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiFactory* self);
    RIN_DXGI_OBJECT_METHODS(RinDxgiFactory);
    RinDxgiHresult(RIN_DXGI_STDCALL *enum_adapters)(
        RinDxgiFactory* self, uint32_t ordinal, RinDxgiAdapter** adapter_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *make_window_association)(
        RinDxgiFactory* self, void* window, uint32_t flags);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_window_association)(
        RinDxgiFactory* self, void** window_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *create_swap_chain)(
        RinDxgiFactory* self, RinDxgiUnknown* device,
        const void* descriptor, void** swap_chain_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *create_software_adapter)(
        RinDxgiFactory* self, void* module, RinDxgiAdapter** adapter_out);
} RinDxgiFactoryVtbl;

struct RinDxgiFactory {
    const RinDxgiFactoryVtbl* vtable;
};

typedef struct RinDxgiFactory1Vtbl {
    RinDxgiHresult(RIN_DXGI_STDCALL *query_interface)(
        RinDxgiFactory1* self, const RinDxgiGuid* iid, void** object_out);
    uint32_t(RIN_DXGI_STDCALL *add_ref)(RinDxgiFactory1* self);
    uint32_t(RIN_DXGI_STDCALL *release)(RinDxgiFactory1* self);
    RIN_DXGI_OBJECT_METHODS(RinDxgiFactory1);
    RinDxgiHresult(RIN_DXGI_STDCALL *enum_adapters)(
        RinDxgiFactory1* self, uint32_t ordinal, RinDxgiAdapter** adapter_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *make_window_association)(
        RinDxgiFactory1* self, void* window, uint32_t flags);
    RinDxgiHresult(RIN_DXGI_STDCALL *get_window_association)(
        RinDxgiFactory1* self, void** window_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *create_swap_chain)(
        RinDxgiFactory1* self, RinDxgiUnknown* device,
        const void* descriptor, void** swap_chain_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *create_software_adapter)(
        RinDxgiFactory1* self, void* module, RinDxgiAdapter** adapter_out);
    RinDxgiHresult(RIN_DXGI_STDCALL *enum_adapters1)(
        RinDxgiFactory1* self, uint32_t ordinal,
        RinDxgiAdapter1** adapter_out);
    RinDxgiBool(RIN_DXGI_STDCALL *is_current)(RinDxgiFactory1* self);
} RinDxgiFactory1Vtbl;

struct RinDxgiFactory1 {
    const RinDxgiFactory1Vtbl* vtable;
};

#undef RIN_DXGI_OBJECT_METHODS

#define RIN_DXGI_S_OK ((RinDxgiHresult)0)
#define RIN_DXGI_E_NOTIMPL ((RinDxgiHresult)INT32_C(-2147467263))
#define RIN_DXGI_E_NOINTERFACE ((RinDxgiHresult)INT32_C(-2147467262))
#define RIN_DXGI_E_POINTER ((RinDxgiHresult)INT32_C(-2147467261))
#define RIN_DXGI_E_FAIL ((RinDxgiHresult)INT32_C(-2147467259))
#define RIN_DXGI_E_INVALIDARG ((RinDxgiHresult)INT32_C(-2147024809))
#define RIN_DXGI_ERROR_INVALID_CALL ((RinDxgiHresult)INT32_C(-2005270527))
#define RIN_DXGI_ERROR_NOT_FOUND ((RinDxgiHresult)INT32_C(-2005270526))
#define RIN_DXGI_ERROR_MORE_DATA ((RinDxgiHresult)INT32_C(-2005270525))
#define RIN_DXGI_ERROR_UNSUPPORTED ((RinDxgiHresult)INT32_C(-2005270524))
#define RIN_DXGI_ERROR_DEVICE_REMOVED ((RinDxgiHresult)INT32_C(-2005270523))
#define RIN_DXGI_MWA_NO_WINDOW_CHANGES UINT32_C(0x00000001)
#define RIN_DXGI_MWA_NO_ALT_ENTER UINT32_C(0x00000002)
#define RIN_DXGI_MWA_NO_PRINT_SCREEN UINT32_C(0x00000004)
#define RIN_DXGI_MWA_KNOWN_FLAGS UINT32_C(0x00000007)
#define RIN_DXGI_ERROR_WAS_STILL_DRAWING \
    ((RinDxgiHresult)INT32_C(-2005270518))

#define RIN_DXGI_ADAPTER_FLAG_NONE 0u
#define RIN_DXGI_MODE_ROTATION_UNSPECIFIED 0u
#define RIN_DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED 0u
#define RIN_DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE 1u
#define RIN_DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST 2u
#define RIN_DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST 3u
#define RIN_DXGI_MODE_SCALING_UNSPECIFIED 0u
#define RIN_DXGI_MODE_SCALING_CENTERED 1u
#define RIN_DXGI_MODE_SCALING_STRETCHED 2u

#define RIN_DXGI_ENUM_MODES_INTERLACED UINT32_C(0x00000001)
#define RIN_DXGI_ENUM_MODES_SCALING UINT32_C(0x00000002)
#define RIN_DXGI_ENUM_MODES_STEREO UINT32_C(0x00000004)
#define RIN_DXGI_ENUM_MODES_DISABLED_STEREO UINT32_C(0x00000008)
#define RIN_DXGI_ENUM_MODES_KNOWN_FLAGS UINT32_C(0x0000000f)

extern const RinDxgiGuid RIN_DXGI_IID_IUNKNOWN;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_OBJECT;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_ADAPTER;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_ADAPTER1;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_OUTPUT;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_FACTORY;
extern const RinDxgiGuid RIN_DXGI_IID_IDXGI_FACTORY1;

typedef struct RinGpuDxgiComRuntime {
    uint64_t opaque[RIN_GPU_DXGI_COM_STATE_QWORDS];
} RinGpuDxgiComRuntime;

int rin_gpu_dxgi_com_runtime_init(RinGpuDxgiComRuntime* runtime,
                                  RinGpuDxgiCatalog* catalog,
                                  uint64_t catalog_generation,
                                  uint64_t state_secret);
int rin_gpu_dxgi_com_runtime_bind(RinGpuDxgiComRuntime* runtime);
int rin_gpu_dxgi_com_runtime_unbind(RinGpuDxgiComRuntime* runtime);
int rin_gpu_dxgi_com_runtime_shutdown(RinGpuDxgiComRuntime* runtime);

RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory(const RinDxgiGuid* iid, void** factory_out);
RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory1(const RinDxgiGuid* iid, void** factory_out);
RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory2(uint32_t flags, const RinDxgiGuid* iid,
                   void** factory_out);

#if defined(__cplusplus)
static_assert(sizeof(RinDxgiGuid) == 16u, "DXGI GUID ABI drift");
static_assert(sizeof(RinDxgiLuid) == 8u, "DXGI LUID ABI drift");
static_assert(sizeof(RinDxgiAdapterDesc) ==
                  (sizeof(uintptr_t) == 8u ? 304u : 292u),
              "DXGI adapter descriptor ABI drift");
static_assert(sizeof(RinDxgiAdapterDesc1) ==
                  (sizeof(uintptr_t) == 8u ? 312u : 296u),
              "DXGI adapter1 descriptor ABI drift");
static_assert(sizeof(RinDxgiOutputDesc) ==
                  (sizeof(uintptr_t) == 8u ? 96u : 92u),
              "DXGI output descriptor ABI drift");
static_assert(sizeof(RinDxgiRational) == 8u,
              "DXGI rational ABI drift");
static_assert(sizeof(RinDxgiModeDesc) == 28u,
              "DXGI mode descriptor ABI drift");
static_assert(offsetof(RinDxgiModeDesc, refresh_rate) == 8u &&
                  offsetof(RinDxgiModeDesc, scaling) == 24u,
              "DXGI mode descriptor field order drift");
#else
_Static_assert(sizeof(RinDxgiGuid) == 16u, "DXGI GUID ABI drift");
_Static_assert(sizeof(RinDxgiLuid) == 8u, "DXGI LUID ABI drift");
_Static_assert(sizeof(RinDxgiAdapterDesc) ==
                   (sizeof(uintptr_t) == 8u ? 304u : 292u),
               "DXGI adapter descriptor ABI drift");
_Static_assert(sizeof(RinDxgiAdapterDesc1) ==
                   (sizeof(uintptr_t) == 8u ? 312u : 296u),
               "DXGI adapter1 descriptor ABI drift");
_Static_assert(sizeof(RinDxgiOutputDesc) ==
                   (sizeof(uintptr_t) == 8u ? 96u : 92u),
               "DXGI output descriptor ABI drift");
_Static_assert(sizeof(RinDxgiRational) == 8u,
               "DXGI rational ABI drift");
_Static_assert(sizeof(RinDxgiModeDesc) == 28u,
               "DXGI mode descriptor ABI drift");
_Static_assert(offsetof(RinDxgiModeDesc, refresh_rate) == 8u &&
                   offsetof(RinDxgiModeDesc, scaling) == 24u,
               "DXGI mode descriptor field order drift");
#endif

#endif /* RINDX_PUBLIC_COM_H */
