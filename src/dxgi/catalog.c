/* SPDX-License-Identifier: MIT */
#include <rindx/catalog.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RIN_GPU_DXGI_CATALOG_MAGIC UINT64_C(0x5247445843415431)

typedef struct RinGpuDxgiCatalogState {
    uint64_t magic;
    uint64_t generation;
    uint64_t handle_secret;
    uint32_t adapter_count;
    uint32_t output_count;
    const RinGpuCore* sources[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuPhysicalDriver* physical_sources[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiOutputTopologyProviderFn topology_provider;
    void* topology_provider_context;
    RinGpuAdapterInfoV1 source_adapters[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDisplayInfoV1 source_outputs[RIN_GPU_DXGI_MAX_OUTPUTS];
    RinGpuDxgiAdapterSnapshotV1 adapters[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiOutputSnapshotV1 outputs[RIN_GPU_DXGI_MAX_OUTPUTS];
    RinGpuDxgiHandle adapter_handles[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiHandle output_handles[RIN_GPU_DXGI_MAX_OUTPUTS];
    uint32_t adapter_output_offsets[RIN_GPU_DXGI_MAX_ADAPTERS];
    uint32_t adapter_output_counts[RIN_GPU_DXGI_MAX_ADAPTERS];
    uint32_t output_adapter_indices[RIN_GPU_DXGI_MAX_OUTPUTS];
    uint64_t guard_hash;
    volatile uint32_t api_lock;
    uint32_t reserved;
} RinGpuDxgiCatalogState;

_Static_assert(sizeof(RinGpuDxgiCatalogState) <= sizeof(RinGpuDxgiCatalog),
               "RinGPU DXGI catalog opaque state is too small");

static RinGpuDxgiCatalogState* catalog_state(RinGpuDxgiCatalog* catalog) {
    return (RinGpuDxgiCatalogState*)(void*)catalog;
}

static int catalog_overlap(const void* left, size_t left_size,
                           const void* right, size_t right_size) {
    uintptr_t left_address;
    uintptr_t right_address;

    if (!left || !right || left_size == 0u || right_size == 0u) return 0;
    left_address = (uintptr_t)left;
    right_address = (uintptr_t)right;
    if (left_address <= right_address)
        return right_address - left_address < left_size;
    return left_address - right_address < right_size;
}

static uint64_t catalog_hash(const RinGpuDxgiCatalogState* state) {
    const uint8_t* bytes = (const uint8_t*)(const void*)state;
    const size_t size = offsetof(RinGpuDxgiCatalogState, guard_hash);
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;

    for (index = 0u; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int catalog_lock(RinGpuDxgiCatalog* catalog,
                        RinGpuDxgiCatalogState** state_out) {
    RinGpuDxgiCatalogState* state;

    if (!catalog || !state_out) return RIN_GPU_DXGI_INVALID_ARGUMENT;
    state = catalog_state(catalog);
    if (state->magic != RIN_GPU_DXGI_CATALOG_MAGIC)
        return RIN_GPU_DXGI_STATE;
    if (__atomic_exchange_n(&state->api_lock, 1u, __ATOMIC_ACQUIRE) != 0u)
        return RIN_GPU_DXGI_BUSY;
    if (state->magic != RIN_GPU_DXGI_CATALOG_MAGIC ||
        state->reserved != 0u || state->guard_hash != catalog_hash(state)) {
        state->magic = 0u;
        __atomic_store_n(&state->api_lock, 0u, __ATOMIC_RELEASE);
        return RIN_GPU_DXGI_STATE;
    }
    *state_out = state;
    return RIN_GPU_DXGI_OK;
}

static void catalog_unlock(RinGpuDxgiCatalogState* state) {
    __atomic_store_n(&state->api_lock, 0u, __ATOMIC_RELEASE);
}

static uint64_t catalog_mix64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static uint64_t catalog_identity(const RinGpuDxgiCatalogState* state,
                                 uint32_t kind, uint32_t adapter_index,
                                 uint32_t output_index) {
    uint64_t identity = state->handle_secret ^
        catalog_mix64(state->generation + UINT64_C(0x9e3779b97f4a7c15));
    identity ^= (uint64_t)kind << 56u;
    identity ^= (uint64_t)(adapter_index + 1u) << 32u;
    identity ^= (uint64_t)(output_index + 1u) << 8u;
    return catalog_mix64(identity);
}

static int catalog_handle_unique(const RinGpuDxgiCatalogState* state,
                                 uint64_t handle) {
    uint32_t index;

    if (handle == 0u) return 0;
    for (index = 0u; index < state->adapter_count; ++index)
        if (state->adapter_handles[index] == handle) return 0;
    for (index = 0u; index < state->output_count; ++index)
        if (state->output_handles[index] == handle) return 0;
    return 1;
}

static int catalog_luid_unique(const RinGpuDxgiCatalogState* state,
                               uint64_t luid, uint32_t count) {
    uint32_t index;

    if (luid == 0u) return 0;
    for (index = 0u; index < count; ++index)
        if (state->adapters[index].adapter_luid == luid) return 0;
    return 1;
}

static int fixed_utf8_to_utf16(const char* input, size_t input_capacity,
                               uint16_t* output, size_t output_capacity) {
    size_t input_length = 0u;
    size_t input_index = 0u;
    size_t output_index = 0u;
    int terminated = 0;

    if (!input || !output || input_capacity == 0u || output_capacity == 0u)
        return 0;
    memset(output, 0, output_capacity * sizeof(*output));
    while (input_length < input_capacity) {
        unsigned byte = (unsigned char)input[input_length];
        if (byte == 0u) {
            terminated = 1;
            break;
        }
        ++input_length;
    }
    if (!terminated || input_length == 0u) return 0;
    for (size_t index = input_length + 1u; index < input_capacity; ++index)
        if (input[index] != '\0') return 0;

    while (input_index < input_length) {
        unsigned first = (unsigned char)input[input_index];
        unsigned code_point;
        size_t byte_count;

        if (first < 0x80u) {
            code_point = first;
            byte_count = 1u;
        } else if (first >= 0xC2u && first <= 0xDFu) {
            code_point = first & 0x1Fu;
            byte_count = 2u;
        } else if (first >= 0xE0u && first <= 0xEFu) {
            code_point = first & 0x0Fu;
            byte_count = 3u;
        } else if (first >= 0xF0u && first <= 0xF4u) {
            code_point = first & 0x07u;
            byte_count = 4u;
        } else {
            memset(output, 0, output_capacity * sizeof(*output));
            return 0;
        }
        if (byte_count > input_length - input_index) {
            memset(output, 0, output_capacity * sizeof(*output));
            return 0;
        }
        for (size_t offset = 1u; offset < byte_count; ++offset) {
            unsigned continuation =
                (unsigned char)input[input_index + offset];
            if ((continuation & 0xC0u) != 0x80u) {
                memset(output, 0, output_capacity * sizeof(*output));
                return 0;
            }
            code_point = (code_point << 6u) | (continuation & 0x3Fu);
        }
        if ((byte_count == 2u && code_point < 0x80u) ||
            (byte_count == 3u && code_point < 0x800u) ||
            (byte_count == 4u && code_point < 0x10000u) ||
            code_point > 0x10FFFFu ||
            (code_point >= 0xD800u && code_point <= 0xDFFFu) ||
            code_point < 0x20u || code_point == 0x7Fu) {
            memset(output, 0, output_capacity * sizeof(*output));
            return 0;
        }
        if (code_point < 0x10000u) {
            if (output_index + 1u >= output_capacity) {
                memset(output, 0, output_capacity * sizeof(*output));
                return 0;
            }
            output[output_index++] = (uint16_t)code_point;
        } else {
            unsigned value = code_point - 0x10000u;
            if (output_index + 2u >= output_capacity) {
                memset(output, 0, output_capacity * sizeof(*output));
                return 0;
            }
            output[output_index++] = (uint16_t)(0xD800u + (value >> 10u));
            output[output_index++] =
                (uint16_t)(0xDC00u + (value & 0x3FFu));
        }
        input_index += byte_count;
    }
    return output_index != 0u;
}

static uint32_t catalog_dxgi_format(uint32_t format) {
    switch (format) {
    case RIN_GPU_FORMAT_RGBA8_UNORM:
        return RIN_GPU_DXGI_FORMAT_R8G8B8A8_UNORM;
    case RIN_GPU_FORMAT_BGRA8_UNORM:
        return RIN_GPU_DXGI_FORMAT_B8G8R8A8_UNORM;
    default:
        return 0u;
    }
}

static int catalog_adapter_snapshot(
    const RinGpuAdapterInfoV1* source, uint64_t luid,
    RinGpuDxgiAdapterSnapshotV1* destination) {
    RinGpuDxgiAdapterSnapshotV1 snapshot;

    if (!source || !destination ||
        source->abi_version != RIN_GPU_ABI_VERSION ||
        source->struct_size != sizeof(*source) || source->vendor_id == 0u ||
        source->device_id == 0u || source->flags != 0u ||
        source->queue_capabilities == 0u ||
        (source->queue_capabilities & ~RIN_GPU_QUEUE_KNOWN_CAPABILITIES) != 0u ||
        (source->queue_capabilities & RIN_GPU_QUEUE_GRAPHICS) == 0u ||
        luid == 0u) {
        return 0;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.abi_version = RIN_GPU_DXGI_CATALOG_VERSION;
    snapshot.struct_size = sizeof(snapshot);
    snapshot.vendor_id = source->vendor_id;
    snapshot.device_id = source->device_id;
    snapshot.dedicated_video_memory_bytes = source->dedicated_memory_bytes;
    snapshot.shared_system_memory_bytes = source->shared_memory_bytes;
    snapshot.queue_capabilities = source->queue_capabilities;
    snapshot.adapter_luid = luid;
    if (!fixed_utf8_to_utf16(source->name, sizeof(source->name),
                             snapshot.description,
                             RIN_GPU_DXGI_ADAPTER_DESCRIPTION_UNITS)) {
        return 0;
    }
    *destination = snapshot;
    return 1;
}

static int catalog_output_snapshot(
    const RinGpuDisplayInfoV1* source, uint64_t luid,
    RinGpuDxgiOutputSnapshotV1* destination) {
    RinGpuDxgiOutputSnapshotV1 snapshot;
    uint32_t dxgi_format;

    if (!source || !destination ||
        source->abi_version != RIN_GPU_ABI_VERSION ||
        source->struct_size != sizeof(*source) || luid == 0u ||
        source->flags == 0u ||
        (source->flags & ~RIN_GPU_DISPLAY_KNOWN_FLAGS) != 0u ||
        (source->flags & RIN_GPU_DISPLAY_CONNECTED) == 0u ||
        source->width == 0u || source->height == 0u ||
        source->refresh_millihertz <
            RIN_GPU_DISPLAY_MIN_REFRESH_MILLIHERTZ ||
        source->refresh_millihertz >
            RIN_GPU_DISPLAY_MAX_REFRESH_MILLIHERTZ ||
        ((source->physical_width_mm == 0u) !=
         (source->physical_height_mm == 0u)) ||
        source->scale_milli < RIN_GPU_DISPLAY_MIN_SCALE_MILLI ||
        source->scale_milli > RIN_GPU_DISPLAY_MAX_SCALE_MILLI ||
        source->reserved0 != 0u) {
        return 0;
    }
    if ((source->display_id == RIN_GPU_PRIMARY_DISPLAY) !=
        ((source->flags & RIN_GPU_DISPLAY_PRIMARY) != 0u)) {
        return 0;
    }
    dxgi_format = catalog_dxgi_format(source->format);
    if (dxgi_format != RIN_GPU_DXGI_FORMAT_R8G8B8A8_UNORM &&
        dxgi_format != RIN_GPU_DXGI_FORMAT_B8G8R8A8_UNORM) {
        return 0;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.abi_version = RIN_GPU_DXGI_CATALOG_VERSION;
    snapshot.struct_size = sizeof(snapshot);
    snapshot.adapter_luid = luid;
    snapshot.display_id = source->display_id;
    snapshot.flags = source->flags;
    snapshot.width = source->width;
    snapshot.height = source->height;
    snapshot.refresh_numerator = source->refresh_millihertz;
    snapshot.refresh_denominator = 1000u;
    snapshot.dxgi_format = dxgi_format;
    snapshot.physical_width_mm = source->physical_width_mm;
    snapshot.physical_height_mm = source->physical_height_mm;
    snapshot.scale_milli = source->scale_milli;
    if (!fixed_utf8_to_utf16(source->name, sizeof(source->name),
                             snapshot.description,
                             RIN_GPU_DXGI_OUTPUT_DESCRIPTION_UNITS)) {
        return 0;
    }
    *destination = snapshot;
    return 1;
}

static int catalog_find_adapter(const RinGpuDxgiCatalogState* state,
                                RinGpuDxgiHandle handle,
                                uint32_t* index_out) {
    uint32_t index;

    if (handle == 0u) return 0;
    for (index = 0u; index < state->adapter_count; ++index) {
        if (state->adapter_handles[index] == handle) {
            if (index_out) *index_out = index;
            return 1;
        }
    }
    return 0;
}

static int catalog_find_output(const RinGpuDxgiCatalogState* state,
                               RinGpuDxgiHandle handle,
                               uint32_t* index_out) {
    uint32_t index;

    if (handle == 0u) return 0;
    for (index = 0u; index < state->output_count; ++index) {
        if (state->output_handles[index] == handle) {
            if (index_out) *index_out = index;
            return 1;
        }
    }
    return 0;
}

int rin_gpu_dxgi_catalog_init(RinGpuDxgiCatalog* catalog,
                              const RinGpuCore* const* adapters,
                              uint32_t adapter_count, uint64_t generation,
                              uint64_t handle_secret) {
    const RinGpuCore* sources[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiCatalogState* state;
    uint64_t prior_magic;
    uint32_t adapter_index;

    if (!catalog || !adapters || adapter_count == 0u ||
        adapter_count > RIN_GPU_DXGI_MAX_ADAPTERS || generation == 0u ||
        handle_secret == 0u ||
        catalog_overlap(catalog, sizeof(*catalog), adapters,
                        (size_t)adapter_count * sizeof(adapters[0]))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    for (adapter_index = 0u; adapter_index < adapter_count; ++adapter_index) {
        uint32_t prior;
        sources[adapter_index] = adapters[adapter_index];
        if (!sources[adapter_index] ||
            catalog_overlap(catalog, sizeof(*catalog),
                            sources[adapter_index], sizeof(uint8_t))) {
            return RIN_GPU_DXGI_INVALID_ARGUMENT;
        }
        for (prior = 0u; prior < adapter_index; ++prior)
            if (sources[prior] == sources[adapter_index])
                return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    memcpy(&prior_magic, catalog, sizeof(prior_magic));
    if (prior_magic == RIN_GPU_DXGI_CATALOG_MAGIC)
        return RIN_GPU_DXGI_STATE;

    memset(catalog, 0, sizeof(*catalog));
    state = catalog_state(catalog);
    state->generation = generation;
    state->handle_secret = handle_secret;
    state->adapter_count = adapter_count;

    for (adapter_index = 0u; adapter_index < adapter_count; ++adapter_index) {
        RinGpuAdapterInfoV1 adapter;
        uint32_t display_count = 0u;
        uint32_t display_index;
        uint32_t primary_count = 0u;
        uint64_t luid;
        uint64_t handle;

        state->sources[adapter_index] = sources[adapter_index];
        if (ringpu_get_adapter_info(sources[adapter_index], &adapter) !=
                RIN_GPU_OK ||
            ringpu_get_display_count(sources[adapter_index], &display_count) !=
                RIN_GPU_OK) {
            goto source_failed;
        }
        if (display_count == 0u || display_count > RIN_GPU_MAX_DISPLAYS ||
            display_count > RIN_GPU_DXGI_MAX_OUTPUTS - state->output_count) {
            goto protocol_failed;
        }
        luid = catalog_identity(state, 3u, adapter_index, 0u);
        if (!catalog_luid_unique(state, luid, adapter_index) ||
            !catalog_adapter_snapshot(&adapter, luid,
                                      &state->adapters[adapter_index])) {
            goto protocol_failed;
        }
        handle = catalog_identity(state, 1u, adapter_index, 0u);
        if (!catalog_handle_unique(state, handle)) goto protocol_failed;
        state->adapter_handles[adapter_index] = handle;
        state->source_adapters[adapter_index] = adapter;
        state->adapter_output_offsets[adapter_index] = state->output_count;
        state->adapter_output_counts[adapter_index] = display_count;

        for (display_index = 0u; display_index < display_count;
             ++display_index) {
            RinGpuDisplayInfoV1 display;
            uint32_t output_index = state->output_count;
            uint32_t prior;

            if (ringpu_get_display_info(sources[adapter_index], display_index,
                                        &display) != RIN_GPU_OK) {
                goto source_failed;
            }
            if (!catalog_output_snapshot(&display, luid,
                                         &state->outputs[output_index])) {
                goto protocol_failed;
            }
            for (prior = state->adapter_output_offsets[adapter_index];
                 prior < output_index; ++prior) {
                if (state->source_outputs[prior].display_id ==
                    display.display_id) {
                    goto protocol_failed;
                }
            }
            if ((display.flags & RIN_GPU_DISPLAY_PRIMARY) != 0u)
                ++primary_count;
            handle = catalog_identity(state, 2u, adapter_index,
                                      display_index);
            if (!catalog_handle_unique(state, handle)) goto protocol_failed;
            state->source_outputs[output_index] = display;
            state->output_handles[output_index] = handle;
            state->output_adapter_indices[output_index] = adapter_index;
            ++state->output_count;
        }
        if (primary_count != 1u) goto protocol_failed;
    }

    state->magic = RIN_GPU_DXGI_CATALOG_MAGIC;
    state->guard_hash = catalog_hash(state);
    return RIN_GPU_DXGI_OK;

source_failed:
    memset(catalog, 0, sizeof(*catalog));
    return RIN_GPU_DXGI_SOURCE_FAILED;
protocol_failed:
    memset(catalog, 0, sizeof(*catalog));
    return RIN_GPU_DXGI_PROTOCOL;
}

int rin_gpu_dxgi_catalog_bind_physical_sources(
    RinGpuDxgiCatalog* catalog, RinGpuPhysicalDriver* const* drivers,
    uint32_t driver_count, RinGpuDxgiOutputTopologyProviderFn provider,
    void* provider_context)
{
    RinGpuDxgiCatalogState* state;
    uint32_t index;
    int result;

    if (!catalog || !drivers || !provider || driver_count == 0u ||
        driver_count > RIN_GPU_DXGI_MAX_ADAPTERS ||
        catalog_overlap(catalog, sizeof(*catalog), drivers,
                        (size_t)driver_count * sizeof(drivers[0]))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (driver_count != state->adapter_count) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    for (index = 0u; index < driver_count; ++index) {
        uint32_t prior;
        if (!drivers[index]) {
            catalog_unlock(state);
            return RIN_GPU_DXGI_INVALID_ARGUMENT;
        }
        for (prior = 0u; prior < index; ++prior)
            if (drivers[prior] == drivers[index]) {
                catalog_unlock(state);
                return RIN_GPU_DXGI_INVALID_ARGUMENT;
            }
    }
    for (index = 0u; index < driver_count; ++index)
        state->physical_sources[index] = drivers[index];
    state->topology_provider = provider;
    state->topology_provider_context = provider_context;
    state->guard_hash = catalog_hash(state);
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_shutdown(RinGpuDxgiCatalog* catalog) {
    RinGpuDxgiCatalogState* state;
    int result = catalog_lock(catalog, &state);

    if (result != RIN_GPU_DXGI_OK) return result;
    memset(catalog, 0, sizeof(*catalog));
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_enumerate_adapter(
    RinGpuDxgiCatalog* catalog, uint32_t ordinal,
    RinGpuDxgiHandle* adapter_out, RinGpuDxgiAdapterSnapshotV1* snapshot_out) {
    RinGpuDxgiCatalogState* state;
    int result;

    if (!catalog || !adapter_out || !snapshot_out ||
        catalog_overlap(catalog, sizeof(*catalog), adapter_out,
                        sizeof(*adapter_out)) ||
        catalog_overlap(catalog, sizeof(*catalog), snapshot_out,
                        sizeof(*snapshot_out)) ||
        catalog_overlap(adapter_out, sizeof(*adapter_out), snapshot_out,
                        sizeof(*snapshot_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    *adapter_out = 0u;
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (ordinal >= state->adapter_count) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    *adapter_out = state->adapter_handles[ordinal];
    *snapshot_out = state->adapters[ordinal];
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_query_adapter(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiAdapterSnapshotV1* snapshot_out) {
    RinGpuDxgiCatalogState* state;
    uint32_t index;
    int result;

    if (!catalog || !snapshot_out ||
        catalog_overlap(catalog, sizeof(*catalog), snapshot_out,
                        sizeof(*snapshot_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (!catalog_find_adapter(state, adapter, &index)) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    *snapshot_out = state->adapters[index];
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_enumerate_output(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter, uint32_t ordinal,
    RinGpuDxgiHandle* output_out, RinGpuDxgiOutputSnapshotV1* snapshot_out) {
    RinGpuDxgiCatalogState* state;
    uint32_t adapter_index;
    uint32_t output_index;
    int result;

    if (!catalog || !output_out || !snapshot_out ||
        catalog_overlap(catalog, sizeof(*catalog), output_out,
                        sizeof(*output_out)) ||
        catalog_overlap(catalog, sizeof(*catalog), snapshot_out,
                        sizeof(*snapshot_out)) ||
        catalog_overlap(output_out, sizeof(*output_out), snapshot_out,
                        sizeof(*snapshot_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    *output_out = 0u;
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (!catalog_find_adapter(state, adapter, &adapter_index)) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    if (ordinal >= state->adapter_output_counts[adapter_index]) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    output_index = state->adapter_output_offsets[adapter_index] + ordinal;
    *output_out = state->output_handles[output_index];
    *snapshot_out = state->outputs[output_index];
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_query_output(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiHandle output, RinGpuDxgiOutputSnapshotV1* snapshot_out) {
    RinGpuDxgiCatalogState* state;
    uint32_t adapter_index;
    uint32_t output_index;
    int result;

    if (!catalog || !snapshot_out ||
        catalog_overlap(catalog, sizeof(*catalog), snapshot_out,
                        sizeof(*snapshot_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (!catalog_find_adapter(state, adapter, &adapter_index) ||
        !catalog_find_output(state, output, &output_index) ||
        state->output_adapter_indices[output_index] != adapter_index) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    *snapshot_out = state->outputs[output_index];
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_query_output_topology(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiHandle output, RinDxgiDisplayTopologyV1* topology_out,
    RinDxgiDisplayTargetV1* target_out)
{
    RinGpuDxgiCatalogState* state;
    RinDxgiDisplayTopologyV1 topology;
    RinGpuDisplayInfoV1 display;
    RinGpuDxgiOutputTopologyProviderFn provider;
    void* provider_context;
    RinGpuPhysicalDriver* driver;
    uint32_t adapter_index;
    uint32_t output_index;
    int result;

    if (!catalog || !topology_out || !target_out ||
        catalog_overlap(catalog, sizeof(*catalog), topology_out,
                        sizeof(*topology_out)) ||
        catalog_overlap(catalog, sizeof(*catalog), target_out,
                        sizeof(*target_out)) ||
        catalog_overlap(topology_out, sizeof(*topology_out), target_out,
                        sizeof(*target_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    memset(topology_out, 0, sizeof(*topology_out));
    memset(target_out, 0, sizeof(*target_out));
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (!catalog_find_adapter(state, adapter, &adapter_index) ||
        !catalog_find_output(state, output, &output_index) ||
        state->output_adapter_indices[output_index] != adapter_index) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_NOT_FOUND;
    }
    driver = state->physical_sources[adapter_index];
    provider = state->topology_provider;
    provider_context = state->topology_provider_context;
    if (!driver || !provider) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_UNSUPPORTED;
    }
    display = state->source_outputs[output_index];
    result = provider(provider_context, driver, &display, &topology,
                      target_out);
    if (result != RIN_GPU_DXGI_OK) {
        catalog_unlock(state);
        return result;
    }
    if (!rindx_display_topology_valid(&topology)) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_PROTOCOL;
    }
    if (!rindx_display_target_valid(target_out) ||
        target_out->generation != topology.generation) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_PROTOCOL;
    }
    *topology_out = topology;
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}

int rin_gpu_dxgi_catalog_is_current(RinGpuDxgiCatalog* catalog,
                                    uint64_t expected_generation,
                                    uint32_t* current_out) {
    RinGpuDxgiCatalogState* state;
    uint32_t adapter_index;
    int result;

    if (!catalog || !current_out ||
        catalog_overlap(catalog, sizeof(*catalog), current_out,
                        sizeof(*current_out))) {
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    }
    *current_out = 0u;
    result = catalog_lock(catalog, &state);
    if (result != RIN_GPU_DXGI_OK) return result;
    if (expected_generation != state->generation) {
        catalog_unlock(state);
        return RIN_GPU_DXGI_STALE;
    }
    for (adapter_index = 0u; adapter_index < state->adapter_count;
         ++adapter_index) {
        RinGpuAdapterInfoV1 adapter;
        uint32_t display_count = 0u;
        uint32_t display_index;

        if (ringpu_get_adapter_info(state->sources[adapter_index], &adapter) !=
                RIN_GPU_OK ||
            ringpu_get_display_count(state->sources[adapter_index],
                                     &display_count) != RIN_GPU_OK ||
            display_count != state->adapter_output_counts[adapter_index] ||
            memcmp(&adapter, &state->source_adapters[adapter_index],
                   sizeof(adapter)) != 0) {
            catalog_unlock(state);
            return RIN_GPU_DXGI_OK;
        }
        for (display_index = 0u; display_index < display_count;
             ++display_index) {
            RinGpuDisplayInfoV1 display;
            uint32_t output_index =
                state->adapter_output_offsets[adapter_index] + display_index;
            if (ringpu_get_display_info(state->sources[adapter_index],
                                        display_index, &display) != RIN_GPU_OK ||
                memcmp(&display, &state->source_outputs[output_index],
                       sizeof(display)) != 0) {
                catalog_unlock(state);
                return RIN_GPU_DXGI_OK;
            }
        }
    }
    *current_out = 1u;
    catalog_unlock(state);
    return RIN_GPU_DXGI_OK;
}
