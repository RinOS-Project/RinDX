/* SPDX-License-Identifier: MIT */
#include <rindx/com.h>

#include <limits.h>
#include <string.h>

typedef struct RinGpuDxgiComState RinGpuDxgiComState;

#if defined(_MSC_VER)
#include <intrin.h>
typedef volatile long RinDxAtomicU32;
typedef RinGpuDxgiComState* volatile RinDxAtomicStatePtr;

static void rin_atomic_u32_init(RinDxAtomicU32* value, uint32_t initial) {
    *value = (long)initial;
}
static uint32_t rin_atomic_u32_load(const RinDxAtomicU32* value) {
    return (uint32_t)_InterlockedCompareExchange(
        (volatile long*)(void*)value, 0L, 0L);
}
static void rin_atomic_u32_store(RinDxAtomicU32* value, uint32_t replacement) {
    (void)_InterlockedExchange(value, (long)replacement);
}
static uint32_t rin_atomic_u32_exchange(RinDxAtomicU32* value,
                                         uint32_t replacement) {
    return (uint32_t)_InterlockedExchange(value, (long)replacement);
}
static int rin_atomic_u32_cas(RinDxAtomicU32* value, uint32_t* expected,
                              uint32_t replacement) {
    long previous = _InterlockedCompareExchange(
        value, (long)replacement, (long)*expected);
    if ((uint32_t)previous == *expected) return 1;
    *expected = (uint32_t)previous;
    return 0;
}
static RinGpuDxgiComState* rin_atomic_state_load(
    const RinDxAtomicStatePtr* value) {
    return (RinGpuDxgiComState*)_InterlockedCompareExchangePointer(
        (void* volatile*)(void*)value, NULL, NULL);
}
static int rin_atomic_state_cas(RinDxAtomicStatePtr* value,
                                RinGpuDxgiComState** expected,
                                RinGpuDxgiComState* replacement) {
    RinGpuDxgiComState* previous =
        (RinGpuDxgiComState*)_InterlockedCompareExchangePointer(
            (void* volatile*)(void*)value, replacement, *expected);
    if (previous == *expected) return 1;
    *expected = previous;
    return 0;
}
#else
#include <stdatomic.h>
typedef _Atomic uint32_t RinDxAtomicU32;
typedef _Atomic(RinGpuDxgiComState*) RinDxAtomicStatePtr;

static void rin_atomic_u32_init(RinDxAtomicU32* value, uint32_t initial) {
    atomic_init(value, initial);
}
static uint32_t rin_atomic_u32_load(const RinDxAtomicU32* value) {
    return atomic_load_explicit(value, memory_order_acquire);
}
static void rin_atomic_u32_store(RinDxAtomicU32* value, uint32_t replacement) {
    atomic_store_explicit(value, replacement, memory_order_release);
}
static uint32_t rin_atomic_u32_exchange(RinDxAtomicU32* value,
                                         uint32_t replacement) {
    return atomic_exchange_explicit(value, replacement, memory_order_acq_rel);
}
static int rin_atomic_u32_cas(RinDxAtomicU32* value, uint32_t* expected,
                              uint32_t replacement) {
    return atomic_compare_exchange_weak_explicit(
        value, expected, replacement, memory_order_acq_rel,
        memory_order_acquire);
}
static RinGpuDxgiComState* rin_atomic_state_load(
    const RinDxAtomicStatePtr* value) {
    return atomic_load_explicit(value, memory_order_acquire);
}
static int rin_atomic_state_cas(RinDxAtomicStatePtr* value,
                                RinGpuDxgiComState** expected,
                                RinGpuDxgiComState* replacement) {
    return atomic_compare_exchange_strong_explicit(
        value, expected, replacement, memory_order_acq_rel,
        memory_order_acquire);
}
#endif

#define RIN_DXGI_COM_MAGIC UINT64_C(0x314d4f4349584452)
#define RIN_DXGI_COM_CALL_CLOSING UINT32_C(0x80000000)
#define RIN_DXGI_COM_CALL_COUNT_MASK UINT32_C(0x7fffffff)
#define RIN_DXGI_PRIVATE_DATA_KIND_BLOB 1u
#define RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE 2u

const RinDxgiGuid RIN_DXGI_IID_IUNKNOWN = {
    0x00000000u, 0x0000u, 0x0000u,
    {0xc0u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x46u}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_OBJECT = {
    0xaec22fb8u, 0x76f3u, 0x4639u,
    {0x9bu, 0xe0u, 0x28u, 0xebu, 0x43u, 0xa6u, 0x7au, 0x2eu}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_ADAPTER = {
    0x2411e7e1u, 0x12acu, 0x4ccfu,
    {0xbdu, 0x14u, 0x97u, 0x98u, 0xe8u, 0x53u, 0x4du, 0xc0u}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_ADAPTER1 = {
    0x29038f61u, 0x3839u, 0x4626u,
    {0x91u, 0xfdu, 0x08u, 0x68u, 0x79u, 0x01u, 0x1au, 0x05u}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_OUTPUT = {
    0xae02eedbu, 0xc735u, 0x4690u,
    {0x8du, 0x52u, 0x5au, 0x8du, 0xc2u, 0x02u, 0x13u, 0xaau}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_FACTORY = {
    0x7b7166ecu, 0x21c7u, 0x44aeu,
    {0xb2u, 0x1au, 0xc9u, 0xaeu, 0x32u, 0x1au, 0xe3u, 0x69u}};
const RinDxgiGuid RIN_DXGI_IID_IDXGI_FACTORY1 = {
    0x770aae78u, 0xf26fu, 0x4dbau,
    {0xa8u, 0x29u, 0x25u, 0x3cu, 0x83u, 0xd1u, 0xb3u, 0x87u}};

typedef struct RinGpuDxgiPrivateData {
    RinDxgiGuid name;
    uint32_t size;
    uint32_t active;
    uint32_t kind;
    RinDxgiUnknown* interface_value;
    uint8_t bytes[RIN_GPU_DXGI_PRIVATE_DATA_MAX_BYTES];
} RinGpuDxgiPrivateData;

typedef struct RinGpuDxgiComFactoryObject {
    RinDxgiFactory1 interface_value;
    RinGpuDxgiComState* state;
    RinDxAtomicU32 references;
    RinGpuDxgiPrivateData private_data;
    void* window_association;
    uint32_t window_association_flags;
} RinGpuDxgiComFactoryObject;

typedef struct RinGpuDxgiComAdapterObject {
    RinDxgiAdapter1 interface_value;
    RinGpuDxgiComState* state;
    RinGpuDxgiHandle handle;
    uint32_t ordinal;
    RinDxAtomicU32 references;
    RinGpuDxgiPrivateData private_data;
} RinGpuDxgiComAdapterObject;

typedef struct RinGpuDxgiComOutputObject {
    RinDxgiOutput interface_value;
    RinGpuDxgiComState* state;
    RinGpuDxgiHandle handle;
    uint32_t parent_ordinal;
    uint32_t ordinal;
    RinDxAtomicU32 references;
    RinGpuDxgiPrivateData private_data;
} RinGpuDxgiComOutputObject;

struct RinGpuDxgiComState {
    uint64_t magic;
    uint64_t state_hash;
    RinGpuDxgiCatalog* catalog;
    uint64_t catalog_generation;
    uint64_t state_secret;
    RinDxAtomicU32 call_gate;
    RinDxAtomicU32 private_data_lock;
    uint32_t initialized;
    uint32_t adapter_count;
    uint32_t output_count;
    uint32_t first_output[RIN_GPU_DXGI_MAX_ADAPTERS];
    uint32_t adapter_output_count[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiComFactoryObject factory;
    RinGpuDxgiComAdapterObject adapters[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiComOutputObject outputs[RIN_GPU_DXGI_MAX_OUTPUTS];
};

static RinDxgiHresult RIN_DXGI_STDCALL factory_query_interface(
    RinDxgiFactory1*, const RinDxgiGuid*, void**);
static uint32_t RIN_DXGI_STDCALL factory_add_ref(RinDxgiFactory1*);
static uint32_t RIN_DXGI_STDCALL factory_release(RinDxgiFactory1*);
static RinDxgiHresult RIN_DXGI_STDCALL factory_set_private_data(
    RinDxgiFactory1*, const RinDxgiGuid*, uint32_t, const void*);
static RinDxgiHresult RIN_DXGI_STDCALL factory_set_private_interface(
    RinDxgiFactory1*, const RinDxgiGuid*, const RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL factory_get_private_data(
    RinDxgiFactory1*, const RinDxgiGuid*, uint32_t*, void*);
static RinDxgiHresult RIN_DXGI_STDCALL factory_get_parent(
    RinDxgiFactory1*, const RinDxgiGuid*, void**);
static RinDxgiHresult RIN_DXGI_STDCALL factory_enum_adapters(
    RinDxgiFactory1*, uint32_t, RinDxgiAdapter**);
static RinDxgiHresult RIN_DXGI_STDCALL factory_make_window_association(
    RinDxgiFactory1*, void*, uint32_t);
static RinDxgiHresult RIN_DXGI_STDCALL factory_get_window_association(
    RinDxgiFactory1*, void**);
static RinDxgiHresult RIN_DXGI_STDCALL factory_create_swap_chain(
    RinDxgiFactory1*, RinDxgiUnknown*, const void*, void**);
static RinDxgiHresult RIN_DXGI_STDCALL factory_create_software_adapter(
    RinDxgiFactory1*, void*, RinDxgiAdapter**);
static RinDxgiHresult RIN_DXGI_STDCALL factory_enum_adapters1(
    RinDxgiFactory1*, uint32_t, RinDxgiAdapter1**);
static RinDxgiBool RIN_DXGI_STDCALL factory_is_current(RinDxgiFactory1*);

static RinDxgiHresult RIN_DXGI_STDCALL adapter_query_interface(
    RinDxgiAdapter1*, const RinDxgiGuid*, void**);
static uint32_t RIN_DXGI_STDCALL adapter_add_ref(RinDxgiAdapter1*);
static uint32_t RIN_DXGI_STDCALL adapter_release(RinDxgiAdapter1*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_set_private_data(
    RinDxgiAdapter1*, const RinDxgiGuid*, uint32_t, const void*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_set_private_interface(
    RinDxgiAdapter1*, const RinDxgiGuid*, const RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_private_data(
    RinDxgiAdapter1*, const RinDxgiGuid*, uint32_t*, void*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_parent(
    RinDxgiAdapter1*, const RinDxgiGuid*, void**);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_enum_outputs(
    RinDxgiAdapter1*, uint32_t, RinDxgiOutput**);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_desc(
    RinDxgiAdapter1*, RinDxgiAdapterDesc*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_check_interface_support(
    RinDxgiAdapter1*, const RinDxgiGuid*, int64_t*);
static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_desc1(
    RinDxgiAdapter1*, RinDxgiAdapterDesc1*);

static RinDxgiHresult RIN_DXGI_STDCALL output_query_interface(
    RinDxgiOutput*, const RinDxgiGuid*, void**);
static uint32_t RIN_DXGI_STDCALL output_add_ref(RinDxgiOutput*);
static uint32_t RIN_DXGI_STDCALL output_release(RinDxgiOutput*);
static RinDxgiHresult RIN_DXGI_STDCALL output_set_private_data(
    RinDxgiOutput*, const RinDxgiGuid*, uint32_t, const void*);
static RinDxgiHresult RIN_DXGI_STDCALL output_set_private_interface(
    RinDxgiOutput*, const RinDxgiGuid*, const RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_private_data(
    RinDxgiOutput*, const RinDxgiGuid*, uint32_t*, void*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_parent(
    RinDxgiOutput*, const RinDxgiGuid*, void**);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_desc(
    RinDxgiOutput*, RinDxgiOutputDesc*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_mode_list(
    RinDxgiOutput*, uint32_t, uint32_t, uint32_t*, RinDxgiModeDesc*);
static RinDxgiHresult RIN_DXGI_STDCALL output_find_mode(
    RinDxgiOutput*, const RinDxgiModeDesc*, RinDxgiModeDesc*,
    RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL output_wait_vblank(RinDxgiOutput*);
static RinDxgiHresult RIN_DXGI_STDCALL output_take_ownership(
    RinDxgiOutput*, RinDxgiUnknown*, RinDxgiBool);
static void RIN_DXGI_STDCALL output_release_ownership(RinDxgiOutput*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_gamma_caps(
    RinDxgiOutput*, void*);
static RinDxgiHresult RIN_DXGI_STDCALL output_set_gamma(
    RinDxgiOutput*, const void*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_gamma(
    RinDxgiOutput*, void*);
static RinDxgiHresult RIN_DXGI_STDCALL output_set_surface(
    RinDxgiOutput*, RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_surface_data(
    RinDxgiOutput*, RinDxgiUnknown*);
static RinDxgiHresult RIN_DXGI_STDCALL output_get_frame_statistics(
    RinDxgiOutput*, void*);

static const RinDxgiFactory1Vtbl factory_vtable = {
    factory_query_interface,
    factory_add_ref,
    factory_release,
    factory_set_private_data,
    factory_set_private_interface,
    factory_get_private_data,
    factory_get_parent,
    factory_enum_adapters,
    factory_make_window_association,
    factory_get_window_association,
    factory_create_swap_chain,
    factory_create_software_adapter,
    factory_enum_adapters1,
    factory_is_current,
};

static const RinDxgiAdapter1Vtbl adapter_vtable = {
    adapter_query_interface,
    adapter_add_ref,
    adapter_release,
    adapter_set_private_data,
    adapter_set_private_interface,
    adapter_get_private_data,
    adapter_get_parent,
    adapter_enum_outputs,
    adapter_get_desc,
    adapter_check_interface_support,
    adapter_get_desc1,
};

static const RinDxgiOutputVtbl output_vtable = {
    output_query_interface,
    output_add_ref,
    output_release,
    output_set_private_data,
    output_set_private_interface,
    output_get_private_data,
    output_get_parent,
    output_get_desc,
    output_get_mode_list,
    output_find_mode,
    output_wait_vblank,
    output_take_ownership,
    output_release_ownership,
    output_get_gamma_caps,
    output_set_gamma,
    output_get_gamma,
    output_set_surface,
    output_get_surface_data,
    output_get_frame_statistics,
};

static RinDxAtomicStatePtr bound_state;

_Static_assert(sizeof(RinGpuDxgiComState) <= sizeof(RinGpuDxgiComRuntime),
               "DXGI COM runtime state exceeds public storage");

static RinGpuDxgiComState* runtime_state(RinGpuDxgiComRuntime* runtime) {
    return runtime ? (RinGpuDxgiComState*)(void*)runtime->opaque : NULL;
}

static int ranges_overlap(const void* left, size_t left_size,
                          const void* right, size_t right_size) {
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    if (!left || !right || left_size == 0u || right_size == 0u) return 0;
    if (left_start > UINTPTR_MAX - left_size ||
        right_start > UINTPTR_MAX - right_size) {
        return 1;
    }
    return left_start < right_start + right_size &&
           right_start < left_start + left_size;
}

static int state_output_alias(const RinGpuDxgiComState* state,
                              const void* output, size_t output_size) {
    return state && ranges_overlap(state, sizeof(*state), output, output_size);
}

static int guid_equal(const RinDxgiGuid* left, const RinDxgiGuid* right);

static void private_data_lock(RinGpuDxgiComState* state) {
    while (rin_atomic_u32_exchange(&state->private_data_lock, 1u) != 0u) {
    }
}

static void private_data_unlock(RinGpuDxgiComState* state) {
    rin_atomic_u32_store(&state->private_data_lock, 0u);
}

static int private_interface_valid(const RinDxgiUnknown* value) {
    return value && value->vtable && value->vtable->query_interface &&
           value->vtable->add_ref && value->vtable->release;
}

static int private_data_shape_valid(const RinGpuDxgiPrivateData* data) {
    if (!data || data->active > 1u || data->kind >
                             RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE) {
        return 0;
    }
    if (!data->active) {
        return data->size == 0u && data->kind == 0u &&
               data->interface_value == NULL;
    }
    if (data->kind == RIN_DXGI_PRIVATE_DATA_KIND_BLOB) {
        return data->size != 0u &&
               data->size <= RIN_GPU_DXGI_PRIVATE_DATA_MAX_BYTES &&
               data->interface_value == NULL;
    }
    return data->kind == RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE &&
           data->size == sizeof(RinDxgiUnknown*) &&
           data->interface_value != NULL;
}

static RinDxgiUnknown* private_data_old_interface(
    const RinGpuDxgiPrivateData* data) {
    return data && data->active &&
                   data->kind == RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE
               ? data->interface_value
               : NULL;
}

static void private_data_release_interface(RinDxgiUnknown* value) {
    if (value && value->vtable && value->vtable->release) {
        (void)value->vtable->release(value);
    }
}

static RinDxgiHresult private_data_set(
    RinGpuDxgiComState* state, RinGpuDxgiPrivateData* destination,
    const RinDxgiGuid* name, uint32_t size, const void* bytes) {
    RinGpuDxgiPrivateData candidate;
    RinGpuDxgiPrivateData existing;
    if (!state || !destination || !name || size > RIN_GPU_DXGI_PRIVATE_DATA_MAX_BYTES ||
        (size != 0u && !bytes) || state_output_alias(state, name, sizeof(*name)) ||
        (bytes && state_output_alias(state, bytes, size))) {
        return RIN_DXGI_E_INVALIDARG;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (size != 0u) {
        candidate.name = *name;
        candidate.size = size;
        candidate.active = 1u;
        candidate.kind = RIN_DXGI_PRIVATE_DATA_KIND_BLOB;
        memcpy(candidate.bytes, bytes, size);
    }
    private_data_lock(state);
    existing = *destination;
    if (size == 0u && existing.active && !guid_equal(&existing.name, name)) {
        private_data_unlock(state);
        return RIN_DXGI_ERROR_NOT_FOUND;
    }
    *destination = candidate;
    private_data_unlock(state);
    private_data_release_interface(private_data_old_interface(&existing));
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult private_data_set_interface(
    RinGpuDxgiComState* state, RinGpuDxgiPrivateData* destination,
    const RinDxgiGuid* name, const RinDxgiUnknown* value) {
    RinGpuDxgiPrivateData candidate;
    RinGpuDxgiPrivateData existing;
    RinDxgiUnknown* old_interface;
    RinDxgiUnknown* retained = (RinDxgiUnknown*)(uintptr_t)value;
    if (!state || !destination || !name ||
        state_output_alias(state, name, sizeof(*name)) ||
        (value &&
         (state_output_alias(state, value, sizeof(*value)) ||
          !private_interface_valid(value)))) {
        return RIN_DXGI_E_INVALIDARG;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (retained) {
        (void)retained->vtable->add_ref(retained);
        candidate.name = *name;
        candidate.size = sizeof(RinDxgiUnknown*);
        candidate.active = 1u;
        candidate.kind = RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE;
        candidate.interface_value = retained;
    }
    private_data_lock(state);
    existing = *destination;
    if (!retained && existing.active && !guid_equal(&existing.name, name)) {
        private_data_unlock(state);
        return RIN_DXGI_ERROR_NOT_FOUND;
    }
    *destination = candidate;
    private_data_unlock(state);
    old_interface = private_data_old_interface(&existing);
    private_data_release_interface(old_interface);
    return RIN_DXGI_S_OK;
}

static void private_data_dispose(RinGpuDxgiComState* state,
                                  RinGpuDxgiPrivateData* destination) {
    RinGpuDxgiPrivateData existing;
    if (!state || !destination) return;
    private_data_lock(state);
    existing = *destination;
    memset(destination, 0, sizeof(*destination));
    private_data_unlock(state);
    private_data_release_interface(private_data_old_interface(&existing));
}

static RinDxgiHresult private_data_get(
    RinGpuDxgiComState* state, const RinGpuDxgiPrivateData* source,
    const RinDxgiGuid* name, uint32_t* size, void* bytes) {
    RinGpuDxgiPrivateData candidate;
    uint32_t capacity;
    if (!size) return RIN_DXGI_E_POINTER;
    if (!state || !source || !name ||
        state_output_alias(state, name, sizeof(*name)) ||
        state_output_alias(state, size, sizeof(*size)) ||
        (bytes && state_output_alias(state, bytes, RIN_GPU_DXGI_PRIVATE_DATA_MAX_BYTES))) {
        return RIN_DXGI_E_INVALIDARG;
    }
    capacity = *size;
    private_data_lock(state);
    candidate = *source;
    if (!candidate.active || !guid_equal(&candidate.name, name)) {
        private_data_unlock(state);
        *size = 0u;
        return RIN_DXGI_ERROR_NOT_FOUND;
    }
    if (candidate.kind == RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE &&
        !private_interface_valid(candidate.interface_value)) {
        private_data_unlock(state);
        *size = 0u;
        return RIN_DXGI_E_FAIL;
    }
    if (capacity < candidate.size || (candidate.size != 0u && !bytes)) {
        *size = candidate.size;
        private_data_unlock(state);
        return RIN_DXGI_ERROR_MORE_DATA;
    }
    if (candidate.kind == RIN_DXGI_PRIVATE_DATA_KIND_INTERFACE) {
        RinDxgiUnknown* retained = candidate.interface_value;
        (void)retained->vtable->add_ref(retained);
        memcpy(bytes, &retained, sizeof(retained));
    } else {
        memcpy(bytes, candidate.bytes, candidate.size);
    }
    *size = candidate.size;
    private_data_unlock(state);
    return RIN_DXGI_S_OK;
}

static int guid_equal(const RinDxgiGuid* left, const RinDxgiGuid* right) {
    return left && right && memcmp(left, right, sizeof(*left)) == 0;
}

static uint64_t hash_word(uint64_t hash, uint64_t value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
    hash ^= hash >> 32u;
    return hash;
}

static uint64_t calculate_state_hash(const RinGpuDxgiComState* state) {
    uint64_t hash;
    uint32_t index;
    if (!state || state->adapter_count > RIN_GPU_DXGI_MAX_ADAPTERS ||
        state->output_count > RIN_GPU_DXGI_MAX_OUTPUTS) {
        return 0u;
    }
    hash = UINT64_C(1469598103934665603) ^ state->state_secret;
    hash = hash_word(hash, state->magic);
    hash = hash_word(hash, (uint64_t)(uintptr_t)state->catalog);
    hash = hash_word(hash, state->catalog_generation);
    hash = hash_word(hash, state->adapter_count);
    hash = hash_word(hash, state->output_count);
    for (index = 0u; index < state->adapter_count; ++index) {
        hash = hash_word(hash, state->first_output[index]);
        hash = hash_word(hash, state->adapter_output_count[index]);
        hash = hash_word(hash, state->adapters[index].handle);
        hash = hash_word(hash, state->adapters[index].ordinal);
    }
    for (index = 0u; index < state->output_count; ++index) {
        hash = hash_word(hash, state->outputs[index].handle);
        hash = hash_word(hash, state->outputs[index].parent_ordinal);
        hash = hash_word(hash, state->outputs[index].ordinal);
    }
    return hash ? hash : UINT64_C(1);
}

static int state_shape_valid(const RinGpuDxgiComState* state) {
    uint32_t adapter;
    uint32_t output;
    if (!state || state->magic != RIN_DXGI_COM_MAGIC ||
        state->initialized != RIN_GPU_DXGI_COM_VERSION || !state->catalog ||
        state->catalog_generation == 0u || state->state_secret == 0u ||
        state->adapter_count == 0u ||
        state->adapter_count > RIN_GPU_DXGI_MAX_ADAPTERS ||
        state->output_count > RIN_GPU_DXGI_MAX_OUTPUTS ||
        state->factory.interface_value.vtable != &factory_vtable ||
        state->factory.state != state) {
        return 0;
    }
    if (!private_data_shape_valid(&state->factory.private_data) ||
        (state->factory.window_association_flags &
         ~RIN_DXGI_MWA_KNOWN_FLAGS) != 0u) {
        return 0;
    }
    for (adapter = 0u; adapter < state->adapter_count; ++adapter) {
        const RinGpuDxgiComAdapterObject* object = &state->adapters[adapter];
        if (object->interface_value.vtable != &adapter_vtable ||
            object->state != state || object->handle == 0u ||
            object->ordinal != adapter ||
            !private_data_shape_valid(&object->private_data) ||
            state->first_output[adapter] > state->output_count ||
            state->adapter_output_count[adapter] >
                state->output_count - state->first_output[adapter]) {
            return 0;
        }
    }
    for (output = 0u; output < state->output_count; ++output) {
        const RinGpuDxgiComOutputObject* object = &state->outputs[output];
        if (object->interface_value.vtable != &output_vtable ||
            object->state != state || object->handle == 0u ||
            !private_data_shape_valid(&object->private_data) ||
            object->parent_ordinal >= state->adapter_count ||
            object->ordinal >=
                state->adapter_output_count[object->parent_ordinal] ||
            state->first_output[object->parent_ordinal] + object->ordinal !=
                output) {
            return 0;
        }
    }
    return state->state_hash == calculate_state_hash(state);
}

static int state_enter(RinGpuDxgiComState* state) {
    uint32_t observed;
    if (!state) return 0;
    observed = rin_atomic_u32_load(&state->call_gate);
    for (;;) {
        if ((observed & RIN_DXGI_COM_CALL_CLOSING) != 0u ||
            (observed & RIN_DXGI_COM_CALL_COUNT_MASK) ==
                RIN_DXGI_COM_CALL_COUNT_MASK) {
            return 0;
        }
        if (rin_atomic_u32_cas(&state->call_gate, &observed,
                               observed + 1u)) {
            return 1;
        }
    }
}

static void state_leave(RinGpuDxgiComState* state) {
    uint32_t observed = rin_atomic_u32_load(&state->call_gate);
    while (observed != 0u && !rin_atomic_u32_cas(
               &state->call_gate, &observed, observed - 1u)) {
    }
}

static uint32_t add_reference(RinDxAtomicU32* references) {
    uint32_t observed = rin_atomic_u32_load(references);
    for (;;) {
        if (observed == UINT32_MAX) return UINT32_MAX;
        if (rin_atomic_u32_cas(references, &observed, observed + 1u)) {
            return observed + 1u;
        }
    }
}

static uint32_t release_reference(RinDxAtomicU32* references) {
    uint32_t observed = rin_atomic_u32_load(references);
    for (;;) {
        if (observed == 0u) return 0u;
        if (rin_atomic_u32_cas(references, &observed, observed - 1u)) {
            return observed - 1u;
        }
    }
}

static RinDxgiHresult catalog_result(int result) {
    if (result == RIN_GPU_DXGI_OK) return RIN_DXGI_S_OK;
    if (result == RIN_GPU_DXGI_NOT_FOUND) return RIN_DXGI_ERROR_NOT_FOUND;
    if (result == RIN_GPU_DXGI_BUSY) return RIN_DXGI_ERROR_WAS_STILL_DRAWING;
    if (result == RIN_GPU_DXGI_STALE ||
        result == RIN_GPU_DXGI_SOURCE_FAILED) {
        return RIN_DXGI_ERROR_DEVICE_REMOVED;
    }
    if (result == RIN_GPU_DXGI_INVALID_ARGUMENT) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (result == RIN_GPU_DXGI_UNSUPPORTED)
        return RIN_DXGI_ERROR_UNSUPPORTED;
    return RIN_DXGI_E_FAIL;
}

static uintptr_t bounded_size(uint64_t value) {
    if (sizeof(uintptr_t) == sizeof(uint32_t) && value > UINT32_MAX) {
        return (uintptr_t)UINT32_MAX;
    }
    return (uintptr_t)value;
}

static void adapter_desc1_from_snapshot(
    const RinGpuDxgiAdapterSnapshotV1* snapshot,
    RinDxgiAdapterDesc1* descriptor) {
    memset(descriptor, 0, sizeof(*descriptor));
    memcpy(descriptor->description, snapshot->description,
           sizeof(descriptor->description));
    descriptor->vendor_id = snapshot->vendor_id;
    descriptor->device_id = snapshot->device_id;
    descriptor->dedicated_video_memory =
        bounded_size(snapshot->dedicated_video_memory_bytes);
    descriptor->shared_system_memory =
        bounded_size(snapshot->shared_system_memory_bytes);
    descriptor->adapter_luid.low_part = (uint32_t)snapshot->adapter_luid;
    descriptor->adapter_luid.high_part =
        (int32_t)(uint32_t)(snapshot->adapter_luid >> 32u);
    descriptor->flags = RIN_DXGI_ADAPTER_FLAG_NONE;
}

static void mode_desc_from_snapshot(
    const RinGpuDxgiOutputSnapshotV1* snapshot,
    RinDxgiModeDesc* descriptor) {
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->width = snapshot->width;
    descriptor->height = snapshot->height;
    descriptor->refresh_rate.numerator = snapshot->refresh_numerator;
    descriptor->refresh_rate.denominator = snapshot->refresh_denominator;
    descriptor->format = snapshot->dxgi_format;
    descriptor->scanline_ordering =
        RIN_DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    descriptor->scaling = RIN_DXGI_MODE_SCALING_UNSPECIFIED;
}

static int factory_object_valid(const RinGpuDxgiComFactoryObject* object) {
    return object && object->state &&
           &object->state->factory == object && state_shape_valid(object->state);
}

static int adapter_object_valid(const RinGpuDxgiComAdapterObject* object) {
    return object && object->state &&
           object->ordinal < object->state->adapter_count &&
           &object->state->adapters[object->ordinal] == object &&
           state_shape_valid(object->state);
}

static int output_object_valid(const RinGpuDxgiComOutputObject* object) {
    uint32_t index;
    if (!object || !object->state ||
        object->parent_ordinal >= object->state->adapter_count) {
        return 0;
    }
    index = object->state->first_output[object->parent_ordinal] +
            object->ordinal;
    return index < object->state->output_count &&
           &object->state->outputs[index] == object &&
           state_shape_valid(object->state);
}

static RinDxgiHresult unsupported_factory(RinDxgiFactory1* self) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    state_leave(object->state);
    return RIN_DXGI_ERROR_UNSUPPORTED;
}

static RinDxgiHresult unsupported_adapter(RinDxgiAdapter1* self) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    state_leave(object->state);
    return RIN_DXGI_ERROR_UNSUPPORTED;
}

static RinDxgiHresult unsupported_output(RinDxgiOutput* self) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    state_leave(object->state);
    return RIN_DXGI_ERROR_UNSUPPORTED;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_query_interface(
    RinDxgiFactory1* self, const RinDxgiGuid* iid, void** object_out) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    if (!object_out) return RIN_DXGI_E_POINTER;
    if (object && state_output_alias(object->state, object_out,
                                     sizeof(*object_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *object_out = NULL;
    if (!iid || !object || !state_enter(object->state)) {
        return !iid ? RIN_DXGI_E_INVALIDARG : RIN_DXGI_E_FAIL;
    }
    if (!factory_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    if (!guid_equal(iid, &RIN_DXGI_IID_IUNKNOWN) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_OBJECT) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_FACTORY) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_FACTORY1)) {
        state_leave(object->state);
        return RIN_DXGI_E_NOINTERFACE;
    }
    (void)add_reference(&object->references);
    *object_out = &object->interface_value;
    state_leave(object->state);
    return RIN_DXGI_S_OK;
}

static uint32_t RIN_DXGI_STDCALL factory_add_ref(RinDxgiFactory1* self) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (factory_object_valid(object)) result = add_reference(&object->references);
    state_leave(object->state);
    return result;
}

static uint32_t RIN_DXGI_STDCALL factory_release(RinDxgiFactory1* self) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (factory_object_valid(object)) {
        result = release_reference(&object->references);
    }
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_set_private_data(
    RinDxgiFactory1* self, const RinDxgiGuid* name, uint32_t size,
    const void* data) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_set_private_interface(
    RinDxgiFactory1* self, const RinDxgiGuid* name,
    const RinDxgiUnknown* value) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set_interface(object->state, &object->private_data,
                                        name, value);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_get_private_data(
    RinDxgiFactory1* self, const RinDxgiGuid* name, uint32_t* size,
    void* data) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_get(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_get_parent(
    RinDxgiFactory1* self, const RinDxgiGuid* iid, void** parent_out) {
    if (!parent_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    parent_out, sizeof(*parent_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *parent_out = NULL;
    (void)iid;
    return unsupported_factory(self);
}

static RinDxgiHresult factory_enumerate_common(
    RinDxgiFactory1* self, uint32_t ordinal, void** adapter_out) {
    RinGpuDxgiComFactoryObject* factory =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    RinGpuDxgiComAdapterObject* adapter;
    RinGpuDxgiAdapterSnapshotV1 snapshot;
    int query_result;
    if (!adapter_out) return RIN_DXGI_E_POINTER;
    if (factory && state_output_alias(factory->state, adapter_out,
                                      sizeof(*adapter_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *adapter_out = NULL;
    if (!factory || !state_enter(factory->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(factory)) {
        state_leave(factory->state);
        return RIN_DXGI_E_FAIL;
    }
    if (ordinal >= factory->state->adapter_count) {
        state_leave(factory->state);
        return RIN_DXGI_ERROR_NOT_FOUND;
    }
    adapter = &factory->state->adapters[ordinal];
    query_result = rin_gpu_dxgi_catalog_query_adapter(
        factory->state->catalog, adapter->handle, &snapshot);
    if (query_result != RIN_GPU_DXGI_OK) {
        state_leave(factory->state);
        return catalog_result(query_result);
    }
    (void)add_reference(&adapter->references);
    *adapter_out = &adapter->interface_value;
    state_leave(factory->state);
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_enum_adapters(
    RinDxgiFactory1* self, uint32_t ordinal, RinDxgiAdapter** adapter_out) {
    void* adapter = NULL;
    RinDxgiHresult result;
    if (!adapter_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    adapter_out, sizeof(*adapter_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *adapter_out = NULL;
    result = factory_enumerate_common(self, ordinal, &adapter);
    if (result == RIN_DXGI_S_OK) {
        *adapter_out = (RinDxgiAdapter*)adapter;
    }
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_enum_adapters1(
    RinDxgiFactory1* self, uint32_t ordinal, RinDxgiAdapter1** adapter_out) {
    void* adapter = NULL;
    RinDxgiHresult result;
    if (!adapter_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    adapter_out, sizeof(*adapter_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *adapter_out = NULL;
    result = factory_enumerate_common(self, ordinal, &adapter);
    if (result == RIN_DXGI_S_OK) {
        *adapter_out = (RinDxgiAdapter1*)adapter;
    }
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_make_window_association(
    RinDxgiFactory1* self, void* window, uint32_t flags) {
    RinGpuDxgiComFactoryObject* object =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!factory_object_valid(object) ||
        (flags & ~RIN_DXGI_MWA_KNOWN_FLAGS) != 0u ||
        (window && state_output_alias(object->state, window,
                                      sizeof(uintptr_t)))) {
        state_leave(object->state);
        return factory_object_valid(object) ? RIN_DXGI_E_INVALIDARG
                                            : RIN_DXGI_E_FAIL;
    }
    private_data_lock(object->state);
    object->window_association = window;
    object->window_association_flags = flags;
    private_data_unlock(object->state);
    state_leave(object->state);
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_get_window_association(
    RinDxgiFactory1* self, void** window_out) {
    if (!window_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    window_out, sizeof(*window_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *window_out = NULL;
    {
        RinGpuDxgiComFactoryObject* object =
            (RinGpuDxgiComFactoryObject*)(void*)self;
        if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
        if (!factory_object_valid(object)) {
            state_leave(object->state);
            return RIN_DXGI_E_FAIL;
        }
        private_data_lock(object->state);
        *window_out = object->window_association;
        private_data_unlock(object->state);
        state_leave(object->state);
    }
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_create_swap_chain(
    RinDxgiFactory1* self, RinDxgiUnknown* device, const void* descriptor,
    void** swap_chain_out) {
    (void)device;
    (void)descriptor;
    if (!swap_chain_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    swap_chain_out, sizeof(*swap_chain_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *swap_chain_out = NULL;
    return unsupported_factory(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL factory_create_software_adapter(
    RinDxgiFactory1* self, void* module, RinDxgiAdapter** adapter_out) {
    (void)module;
    if (!adapter_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComFactoryObject*)(void*)self)->state,
                    adapter_out, sizeof(*adapter_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *adapter_out = NULL;
    return unsupported_factory(self);
}

static RinDxgiBool RIN_DXGI_STDCALL factory_is_current(
    RinDxgiFactory1* self) {
    RinGpuDxgiComFactoryObject* factory =
        (RinGpuDxgiComFactoryObject*)(void*)self;
    uint32_t current = 0u;
    int result;
    if (!factory || !state_enter(factory->state)) return 0;
    if (!factory_object_valid(factory)) {
        state_leave(factory->state);
        return 0;
    }
    result = rin_gpu_dxgi_catalog_is_current(
        factory->state->catalog, factory->state->catalog_generation, &current);
    state_leave(factory->state);
    return result == RIN_GPU_DXGI_OK && current == 1u ? 1 : 0;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_query_interface(
    RinDxgiAdapter1* self, const RinDxgiGuid* iid, void** object_out) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    if (!object_out) return RIN_DXGI_E_POINTER;
    if (object && state_output_alias(object->state, object_out,
                                     sizeof(*object_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *object_out = NULL;
    if (!iid || !object || !state_enter(object->state)) {
        return !iid ? RIN_DXGI_E_INVALIDARG : RIN_DXGI_E_FAIL;
    }
    if (!adapter_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    if (!guid_equal(iid, &RIN_DXGI_IID_IUNKNOWN) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_OBJECT) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_ADAPTER) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_ADAPTER1)) {
        state_leave(object->state);
        return RIN_DXGI_E_NOINTERFACE;
    }
    (void)add_reference(&object->references);
    *object_out = &object->interface_value;
    state_leave(object->state);
    return RIN_DXGI_S_OK;
}

static uint32_t RIN_DXGI_STDCALL adapter_add_ref(RinDxgiAdapter1* self) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (adapter_object_valid(object)) result = add_reference(&object->references);
    state_leave(object->state);
    return result;
}

static uint32_t RIN_DXGI_STDCALL adapter_release(RinDxgiAdapter1* self) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (adapter_object_valid(object)) {
        result = release_reference(&object->references);
    }
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_set_private_data(
    RinDxgiAdapter1* self, const RinDxgiGuid* name, uint32_t size,
    const void* data) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_set_private_interface(
    RinDxgiAdapter1* self, const RinDxgiGuid* name,
    const RinDxgiUnknown* value) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set_interface(object->state, &object->private_data,
                                        name, value);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_private_data(
    RinDxgiAdapter1* self, const RinDxgiGuid* name, uint32_t* size,
    void* data) {
    RinGpuDxgiComAdapterObject* object =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_get(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_parent(
    RinDxgiAdapter1* self, const RinDxgiGuid* iid, void** parent_out) {
    RinGpuDxgiComAdapterObject* adapter =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinGpuDxgiComFactoryObject* factory;
    RinDxgiHresult result;
    if (!parent_out) return RIN_DXGI_E_POINTER;
    if (adapter && state_output_alias(adapter->state, parent_out,
                                      sizeof(*parent_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *parent_out = NULL;
    if (!iid || !adapter || !state_enter(adapter->state)) {
        return !iid ? RIN_DXGI_E_INVALIDARG : RIN_DXGI_E_FAIL;
    }
    if (!adapter_object_valid(adapter)) {
        state_leave(adapter->state);
        return RIN_DXGI_E_FAIL;
    }
    factory = &adapter->state->factory;
    result = factory_query_interface(&factory->interface_value, iid,
                                     parent_out);
    state_leave(adapter->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_enum_outputs(
    RinDxgiAdapter1* self, uint32_t ordinal, RinDxgiOutput** output_out) {
    RinGpuDxgiComAdapterObject* adapter =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinGpuDxgiComOutputObject* output;
    RinGpuDxgiOutputSnapshotV1 snapshot;
    int query_result;
    uint32_t index;
    if (!output_out) return RIN_DXGI_E_POINTER;
    if (adapter && state_output_alias(adapter->state, output_out,
                                      sizeof(*output_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *output_out = NULL;
    if (!adapter || !state_enter(adapter->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(adapter)) {
        state_leave(adapter->state);
        return RIN_DXGI_E_FAIL;
    }
    if (ordinal >= adapter->state->adapter_output_count[adapter->ordinal]) {
        state_leave(adapter->state);
        return RIN_DXGI_ERROR_NOT_FOUND;
    }
    index = adapter->state->first_output[adapter->ordinal] + ordinal;
    output = &adapter->state->outputs[index];
    query_result = rin_gpu_dxgi_catalog_query_output(
        adapter->state->catalog, adapter->handle, output->handle, &snapshot);
    if (query_result != RIN_GPU_DXGI_OK) {
        state_leave(adapter->state);
        return catalog_result(query_result);
    }
    (void)add_reference(&output->references);
    *output_out = &output->interface_value;
    state_leave(adapter->state);
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_desc1(
    RinDxgiAdapter1* self, RinDxgiAdapterDesc1* descriptor_out) {
    RinGpuDxgiComAdapterObject* adapter =
        (RinGpuDxgiComAdapterObject*)(void*)self;
    RinGpuDxgiAdapterSnapshotV1 snapshot;
    int query_result;
    if (!descriptor_out) return RIN_DXGI_E_POINTER;
    if (adapter && state_output_alias(adapter->state, descriptor_out,
                                      sizeof(*descriptor_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    memset(descriptor_out, 0, sizeof(*descriptor_out));
    if (!adapter || !state_enter(adapter->state)) return RIN_DXGI_E_FAIL;
    if (!adapter_object_valid(adapter)) {
        state_leave(adapter->state);
        return RIN_DXGI_E_FAIL;
    }
    query_result = rin_gpu_dxgi_catalog_query_adapter(
        adapter->state->catalog, adapter->handle, &snapshot);
    if (query_result == RIN_GPU_DXGI_OK) {
        adapter_desc1_from_snapshot(&snapshot, descriptor_out);
    }
    state_leave(adapter->state);
    return catalog_result(query_result);
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_get_desc(
    RinDxgiAdapter1* self, RinDxgiAdapterDesc* descriptor_out) {
    RinDxgiAdapterDesc1 descriptor1;
    RinDxgiHresult result;
    if (!descriptor_out) return RIN_DXGI_E_POINTER;
    if (self && state_output_alias(
                    ((RinGpuDxgiComAdapterObject*)(void*)self)->state,
                    descriptor_out, sizeof(*descriptor_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    memset(descriptor_out, 0, sizeof(*descriptor_out));
    result = adapter_get_desc1(self, &descriptor1);
    if (result == RIN_DXGI_S_OK) {
        memcpy(descriptor_out, &descriptor1, sizeof(*descriptor_out));
    }
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL adapter_check_interface_support(
    RinDxgiAdapter1* self, const RinDxgiGuid* interface_name,
    int64_t* version_out) {
    (void)interface_name;
    if (self && version_out &&
        state_output_alias(
            ((RinGpuDxgiComAdapterObject*)(void*)self)->state, version_out,
            sizeof(*version_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (version_out) *version_out = 0;
    return unsupported_adapter(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_query_interface(
    RinDxgiOutput* self, const RinDxgiGuid* iid, void** object_out) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    if (!object_out) return RIN_DXGI_E_POINTER;
    if (object && state_output_alias(object->state, object_out,
                                     sizeof(*object_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *object_out = NULL;
    if (!iid || !object || !state_enter(object->state)) {
        return !iid ? RIN_DXGI_E_INVALIDARG : RIN_DXGI_E_FAIL;
    }
    if (!output_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    if (!guid_equal(iid, &RIN_DXGI_IID_IUNKNOWN) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_OBJECT) &&
        !guid_equal(iid, &RIN_DXGI_IID_IDXGI_OUTPUT)) {
        state_leave(object->state);
        return RIN_DXGI_E_NOINTERFACE;
    }
    (void)add_reference(&object->references);
    *object_out = &object->interface_value;
    state_leave(object->state);
    return RIN_DXGI_S_OK;
}

static uint32_t RIN_DXGI_STDCALL output_add_ref(RinDxgiOutput* self) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (output_object_valid(object)) result = add_reference(&object->references);
    state_leave(object->state);
    return result;
}

static uint32_t RIN_DXGI_STDCALL output_release(RinDxgiOutput* self) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    uint32_t result = 0u;
    if (!object || !state_enter(object->state)) return 0u;
    if (output_object_valid(object)) {
        result = release_reference(&object->references);
    }
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_set_private_data(
    RinDxgiOutput* self, const RinDxgiGuid* name, uint32_t size,
    const void* data) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_set_private_interface(
    RinDxgiOutput* self, const RinDxgiGuid* name,
    const RinDxgiUnknown* value) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_set_interface(object->state, &object->private_data,
                                        name, value);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_private_data(
    RinDxgiOutput* self, const RinDxgiGuid* name, uint32_t* size,
    void* data) {
    RinGpuDxgiComOutputObject* object =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinDxgiHresult result;
    if (!object || !state_enter(object->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(object)) {
        state_leave(object->state);
        return RIN_DXGI_E_FAIL;
    }
    result = private_data_get(object->state, &object->private_data, name,
                              size, data);
    state_leave(object->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_parent(
    RinDxgiOutput* self, const RinDxgiGuid* iid, void** parent_out) {
    RinGpuDxgiComOutputObject* output =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinGpuDxgiComAdapterObject* adapter;
    RinDxgiHresult result;
    if (!parent_out) return RIN_DXGI_E_POINTER;
    if (output && state_output_alias(output->state, parent_out,
                                     sizeof(*parent_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *parent_out = NULL;
    if (!iid || !output || !state_enter(output->state)) {
        return !iid ? RIN_DXGI_E_INVALIDARG : RIN_DXGI_E_FAIL;
    }
    if (!output_object_valid(output)) {
        state_leave(output->state);
        return RIN_DXGI_E_FAIL;
    }
    adapter = &output->state->adapters[output->parent_ordinal];
    result = adapter_query_interface(&adapter->interface_value, iid,
                                     parent_out);
    state_leave(output->state);
    return result;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_desc(
    RinDxgiOutput* self, RinDxgiOutputDesc* descriptor_out) {
    RinGpuDxgiComOutputObject* output =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinGpuDxgiComAdapterObject* adapter;
    RinGpuDxgiOutputSnapshotV1 snapshot;
    RinDxgiDisplayTopologyV1 topology;
    RinDxgiDisplayTargetV1 target;
    uint32_t index;
    int query_result;
    int topology_result;
    if (!descriptor_out) return RIN_DXGI_E_POINTER;
    if (output && state_output_alias(output->state, descriptor_out,
                                     sizeof(*descriptor_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    memset(descriptor_out, 0, sizeof(*descriptor_out));
    if (!output || !state_enter(output->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(output)) {
        state_leave(output->state);
        return RIN_DXGI_E_FAIL;
    }
    adapter = &output->state->adapters[output->parent_ordinal];
    query_result = rin_gpu_dxgi_catalog_query_output(
        output->state->catalog, adapter->handle, output->handle, &snapshot);
    if (query_result == RIN_GPU_DXGI_OK) {
        for (index = 0u; index + 1u < 32u &&
                         snapshot.description[index] != 0u;
             ++index) {
            descriptor_out->device_name[index] = snapshot.description[index];
        }
        topology_result = rin_gpu_dxgi_catalog_query_output_topology(
            output->state->catalog, adapter->handle, output->handle,
            &topology, &target);
        if (topology_result == RIN_GPU_DXGI_OK) {
            int64_t right = (int64_t)target.x + (int64_t)target.width;
            int64_t bottom = (int64_t)target.y + (int64_t)target.height;
            descriptor_out->desktop_coordinates.left = target.x;
            descriptor_out->desktop_coordinates.top = target.y;
            descriptor_out->desktop_coordinates.right = (int32_t)right;
            descriptor_out->desktop_coordinates.bottom = (int32_t)bottom;
            descriptor_out->attached_to_desktop =
                (target.flags & RINDX_DISPLAY_FLAG_ENABLED) != 0u;
        } else if (topology_result != RIN_GPU_DXGI_UNSUPPORTED) {
            state_leave(output->state);
            return catalog_result(topology_result);
        }
        descriptor_out->rotation = RIN_DXGI_MODE_ROTATION_UNSPECIFIED;
    }
    state_leave(output->state);
    return catalog_result(query_result);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_mode_list(
    RinDxgiOutput* self, uint32_t format, uint32_t flags,
    uint32_t* mode_count, RinDxgiModeDesc* descriptors) {
    RinGpuDxgiComOutputObject* output =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinGpuDxgiComAdapterObject* adapter;
    RinGpuDxgiOutputSnapshotV1 snapshot;
    RinDxgiModeDesc descriptor;
    RinDxgiHresult result;
    uint32_t capacity;
    int query_result;

    if (!mode_count) return RIN_DXGI_E_POINTER;
    if (output && state_output_alias(output->state, mode_count,
                                     sizeof(*mode_count))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (descriptors &&
        ranges_overlap(mode_count, sizeof(*mode_count), descriptors,
                       sizeof(*descriptors))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    capacity = descriptors ? *mode_count : 0u;
    *mode_count = 0u;
    if (descriptors && output &&
        state_output_alias(output->state, descriptors,
                           sizeof(*descriptors))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if ((flags & ~RIN_DXGI_ENUM_MODES_KNOWN_FLAGS) != 0u || format == 0u) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (!output || !state_enter(output->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(output)) {
        state_leave(output->state);
        return RIN_DXGI_E_FAIL;
    }
    adapter = &output->state->adapters[output->parent_ordinal];
    query_result = rin_gpu_dxgi_catalog_query_output(
        output->state->catalog, adapter->handle, output->handle, &snapshot);
    result = catalog_result(query_result);
    if (query_result != RIN_GPU_DXGI_OK) {
        state_leave(output->state);
        return result;
    }
    if (snapshot.dxgi_format != format) {
        state_leave(output->state);
        return RIN_DXGI_S_OK;
    }
    *mode_count = 1u;
    if (!descriptors) {
        state_leave(output->state);
        return RIN_DXGI_S_OK;
    }
    if (capacity < 1u) {
        state_leave(output->state);
        return RIN_DXGI_ERROR_MORE_DATA;
    }
    mode_desc_from_snapshot(&snapshot, &descriptor);
    *descriptors = descriptor;
    state_leave(output->state);
    return RIN_DXGI_S_OK;
}

static RinDxgiHresult RIN_DXGI_STDCALL output_find_mode(
    RinDxgiOutput* self, const RinDxgiModeDesc* requested,
    RinDxgiModeDesc* closest, RinDxgiUnknown* device) {
    RinGpuDxgiComOutputObject* output =
        (RinGpuDxgiComOutputObject*)(void*)self;
    RinGpuDxgiComAdapterObject* adapter;
    RinGpuDxgiOutputSnapshotV1 snapshot;
    RinDxgiModeDesc request;
    int query_result;

    if (!requested || !closest) return RIN_DXGI_E_POINTER;
    if (output && state_output_alias(output->state, closest,
                                     sizeof(*closest))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (output && state_output_alias(output->state, requested,
                                     sizeof(*requested))) {
        memset(closest, 0, sizeof(*closest));
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    request = *requested;
    memset(closest, 0, sizeof(*closest));
    if (device) return unsupported_output(self);
    if ((request.width == 0u) != (request.height == 0u) ||
        (request.refresh_rate.numerator == 0u) !=
            (request.refresh_rate.denominator == 0u) ||
        request.format == 0u ||
        request.scanline_ordering >
            RIN_DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST ||
        request.scaling > RIN_DXGI_MODE_SCALING_STRETCHED) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    if (!output || !state_enter(output->state)) return RIN_DXGI_E_FAIL;
    if (!output_object_valid(output)) {
        state_leave(output->state);
        return RIN_DXGI_E_FAIL;
    }
    adapter = &output->state->adapters[output->parent_ordinal];
    query_result = rin_gpu_dxgi_catalog_query_output(
        output->state->catalog, adapter->handle, output->handle, &snapshot);
    if (query_result == RIN_GPU_DXGI_OK &&
        request.format != snapshot.dxgi_format) {
        query_result = RIN_GPU_DXGI_NOT_FOUND;
    }
    if (query_result == RIN_GPU_DXGI_OK) {
        mode_desc_from_snapshot(&snapshot, closest);
    }
    state_leave(output->state);
    return catalog_result(query_result);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_wait_vblank(
    RinDxgiOutput* self) {
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_take_ownership(
    RinDxgiOutput* self, RinDxgiUnknown* device, RinDxgiBool exclusive) {
    (void)device;
    (void)exclusive;
    return unsupported_output(self);
}

static void RIN_DXGI_STDCALL output_release_ownership(RinDxgiOutput* self) {
    (void)unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_gamma_caps(
    RinDxgiOutput* self, void* value) {
    (void)value;
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_set_gamma(
    RinDxgiOutput* self, const void* value) {
    (void)value;
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_gamma(
    RinDxgiOutput* self, void* value) {
    (void)value;
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_set_surface(
    RinDxgiOutput* self, RinDxgiUnknown* surface) {
    (void)surface;
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_surface_data(
    RinDxgiOutput* self, RinDxgiUnknown* destination) {
    (void)destination;
    return unsupported_output(self);
}

static RinDxgiHresult RIN_DXGI_STDCALL output_get_frame_statistics(
    RinDxgiOutput* self, void* value) {
    (void)value;
    return unsupported_output(self);
}

int rin_gpu_dxgi_com_runtime_init(RinGpuDxgiComRuntime* runtime,
                                  RinGpuDxgiCatalog* catalog,
                                  uint64_t catalog_generation,
                                  uint64_t state_secret) {
    RinGpuDxgiComState* state = runtime_state(runtime);
    uint32_t current = 0u;
    uint32_t adapter;
    int result;
    if (!runtime || !catalog || catalog_generation == 0u ||
        state_secret == 0u ||
        ranges_overlap(runtime, sizeof(*runtime), catalog,
                       sizeof(*catalog))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    if (state->magic == RIN_DXGI_COM_MAGIC && state->initialized != 0u) {
        return RIN_GPU_DXGI_STATE;
    }
    result = rin_gpu_dxgi_catalog_is_current(catalog, catalog_generation,
                                             &current);
    if (result != RIN_GPU_DXGI_OK || current != 1u) {
        return result != RIN_GPU_DXGI_OK ? result : RIN_GPU_DXGI_STALE;
    }
    memset(runtime, 0, sizeof(*runtime));
    state->magic = RIN_DXGI_COM_MAGIC;
    state->catalog = catalog;
    state->catalog_generation = catalog_generation;
    state->state_secret = state_secret;
    rin_atomic_u32_init(&state->call_gate, 0u);
    rin_atomic_u32_init(&state->private_data_lock, 0u);
    state->factory.interface_value.vtable = &factory_vtable;
    state->factory.state = state;
    rin_atomic_u32_init(&state->factory.references, 0u);
    for (adapter = 0u; adapter < RIN_GPU_DXGI_MAX_ADAPTERS; ++adapter) {
        RinGpuDxgiAdapterSnapshotV1 snapshot;
        RinGpuDxgiHandle adapter_handle = 0u;
        uint32_t output_ordinal;
        result = rin_gpu_dxgi_catalog_enumerate_adapter(
            catalog, adapter, &adapter_handle, &snapshot);
        if (result == RIN_GPU_DXGI_NOT_FOUND) break;
        if (result != RIN_GPU_DXGI_OK || adapter_handle == 0u) goto fail;
        state->first_output[adapter] = state->output_count;
        state->adapters[adapter].interface_value.vtable = &adapter_vtable;
        state->adapters[adapter].state = state;
        state->adapters[adapter].handle = adapter_handle;
        state->adapters[adapter].ordinal = adapter;
        rin_atomic_u32_init(&state->adapters[adapter].references, 0u);
        for (output_ordinal = 0u;
             output_ordinal < RIN_GPU_MAX_DISPLAYS; ++output_ordinal) {
            RinGpuDxgiOutputSnapshotV1 output_snapshot;
            RinGpuDxgiHandle output_handle = 0u;
            RinGpuDxgiComOutputObject* output;
            result = rin_gpu_dxgi_catalog_enumerate_output(
                catalog, adapter_handle, output_ordinal, &output_handle,
                &output_snapshot);
            if (result == RIN_GPU_DXGI_NOT_FOUND) break;
            if (result != RIN_GPU_DXGI_OK || output_handle == 0u ||
                state->output_count >= RIN_GPU_DXGI_MAX_OUTPUTS) {
                result = result == RIN_GPU_DXGI_OK ? RIN_GPU_DXGI_LIMIT
                                                  : result;
                goto fail;
            }
            output = &state->outputs[state->output_count++];
            output->interface_value.vtable = &output_vtable;
            output->state = state;
            output->handle = output_handle;
            output->parent_ordinal = adapter;
            output->ordinal = output_ordinal;
            rin_atomic_u32_init(&output->references, 0u);
        }
        state->adapter_output_count[adapter] = output_ordinal;
        state->adapter_count++;
    }
    if (state->adapter_count == 0u) {
        result = RIN_GPU_DXGI_NOT_FOUND;
        goto fail;
    }
    state->initialized = RIN_GPU_DXGI_COM_VERSION;
    state->state_hash = calculate_state_hash(state);
    if (!state_shape_valid(state)) {
        result = RIN_GPU_DXGI_STATE;
        goto fail;
    }
    return RIN_GPU_DXGI_OK;

fail:
    memset(runtime, 0, sizeof(*runtime));
    return result;
}

int rin_gpu_dxgi_com_runtime_bind(RinGpuDxgiComRuntime* runtime) {
    RinGpuDxgiComState* state = runtime_state(runtime);
    RinGpuDxgiComState* expected = NULL;
    uint32_t current = 0u;
    int result;
    if (!state || !state_enter(state)) return RIN_GPU_DXGI_INVALID_ARGUMENT;
    if (!state_shape_valid(state)) {
        state_leave(state);
        return RIN_GPU_DXGI_STATE;
    }
    result = rin_gpu_dxgi_catalog_is_current(
        state->catalog, state->catalog_generation, &current);
    if (result != RIN_GPU_DXGI_OK || current != 1u) {
        state_leave(state);
        return result != RIN_GPU_DXGI_OK ? result : RIN_GPU_DXGI_STALE;
    }
    if (!rin_atomic_state_cas(&bound_state, &expected, state)) {
        state_leave(state);
        return expected == state ? RIN_GPU_DXGI_STATE : RIN_GPU_DXGI_BUSY;
    }
    state_leave(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_com_runtime_unbind(RinGpuDxgiComRuntime* runtime) {
    RinGpuDxgiComState* state = runtime_state(runtime);
    RinGpuDxgiComState* expected = state;
    if (!state || !state_enter(state)) return RIN_GPU_DXGI_INVALID_ARGUMENT;
    if (!state_shape_valid(state)) {
        state_leave(state);
        return RIN_GPU_DXGI_STATE;
    }
    if (!rin_atomic_state_cas(&bound_state, &expected, NULL)) {
        state_leave(state);
        return RIN_GPU_DXGI_STATE;
    }
    state_leave(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_com_runtime_shutdown(RinGpuDxgiComRuntime* runtime) {
    RinGpuDxgiComState* state = runtime_state(runtime);
    uint32_t expected = 0u;
    uint32_t index;
    if (!state) return RIN_GPU_DXGI_INVALID_ARGUMENT;
    if (!rin_atomic_u32_cas(&state->call_gate, &expected,
                            RIN_DXGI_COM_CALL_CLOSING)) {
        return RIN_GPU_DXGI_BUSY;
    }
    if (!state_shape_valid(state) ||
        rin_atomic_state_load(&bound_state) == state) {
        rin_atomic_u32_store(&state->call_gate, 0u);
        return state_shape_valid(state) ? RIN_GPU_DXGI_BUSY
                                        : RIN_GPU_DXGI_STATE;
    }
    if (rin_atomic_u32_load(&state->factory.references) != 0u) {
        rin_atomic_u32_store(&state->call_gate, 0u);
        return RIN_GPU_DXGI_BUSY;
    }
    for (index = 0u; index < state->adapter_count; ++index) {
        if (rin_atomic_u32_load(&state->adapters[index].references) != 0u) {
            rin_atomic_u32_store(&state->call_gate, 0u);
            return RIN_GPU_DXGI_BUSY;
        }
    }
    for (index = 0u; index < state->output_count; ++index) {
        if (rin_atomic_u32_load(&state->outputs[index].references) != 0u) {
            rin_atomic_u32_store(&state->call_gate, 0u);
            return RIN_GPU_DXGI_BUSY;
        }
    }
    private_data_dispose(state, &state->factory.private_data);
    for (index = 0u; index < state->adapter_count; ++index) {
        private_data_dispose(state, &state->adapters[index].private_data);
    }
    for (index = 0u; index < state->output_count; ++index) {
        private_data_dispose(state, &state->outputs[index].private_data);
    }
    memset(runtime, 0, sizeof(*runtime));
    return RIN_GPU_DXGI_OK;
}

#if defined(_WIN32)
extern int32_t rindx_native_dxgi_create_factory(const void* iid, void** out);
extern int32_t rindx_native_dxgi_create_factory2(uint32_t flags, const void* iid,
                                                 void** out);
#endif

static RinDxgiHresult create_factory_common(const RinDxgiGuid* iid,
                                            void** factory_out) {
    RinGpuDxgiComState* state;
    RinDxgiHresult result;
    if (!factory_out) return RIN_DXGI_E_POINTER;
    state = rin_atomic_state_load(&bound_state);
    if (state && state_output_alias(state, factory_out,
                                    sizeof(*factory_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *factory_out = NULL;
    if (!iid) return RIN_DXGI_E_INVALIDARG;
    if (!state || !state_enter(state)) return RIN_DXGI_E_FAIL;
    if (!state_shape_valid(state)) {
        state_leave(state);
        return RIN_DXGI_E_FAIL;
    }
    result = factory_query_interface(&state->factory.interface_value, iid,
                                     factory_out);
    state_leave(state);
    return result;
}

#if defined(_WIN32) && defined(RINDX_BUILD_DLL)
#define RINDX_DXGI_NATIVE_EXPORT __declspec(dllexport)
#else
#define RINDX_DXGI_NATIVE_EXPORT
#endif

RINDX_DXGI_NATIVE_EXPORT RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory(const RinDxgiGuid* iid, void** factory_out) {
#if defined(_WIN32)
    return (RinDxgiHresult)rindx_native_dxgi_create_factory(iid, factory_out);
#else
    return create_factory_common(iid, factory_out);
#endif
}

RINDX_DXGI_NATIVE_EXPORT RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory1(const RinDxgiGuid* iid, void** factory_out) {
#if defined(_WIN32)
    return (RinDxgiHresult)rindx_native_dxgi_create_factory(iid, factory_out);
#else
    return create_factory_common(iid, factory_out);
#endif
}

RINDX_DXGI_NATIVE_EXPORT RinDxgiHresult RIN_DXGI_STDCALL
CreateDXGIFactory2(uint32_t flags, const RinDxgiGuid* iid,
                   void** factory_out) {
#if defined(_WIN32)
    return (RinDxgiHresult)rindx_native_dxgi_create_factory2(flags, iid,
                                                              factory_out);
#else
    RinGpuDxgiComState* state;
    if (!factory_out) return RIN_DXGI_E_POINTER;
    state = rin_atomic_state_load(&bound_state);
    if (state && state_output_alias(state, factory_out,
                                    sizeof(*factory_out))) {
        return RIN_DXGI_ERROR_INVALID_CALL;
    }
    *factory_out = NULL;
    if (flags != 0u) return RIN_DXGI_ERROR_UNSUPPORTED;
    return create_factory_common(iid, factory_out);
#endif
}
