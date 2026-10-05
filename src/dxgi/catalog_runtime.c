/* SPDX-License-Identifier: MIT */
#include <rindx/catalog.h>

#include <string.h>

static int catalog_runtime_overlap(const void* left, size_t left_size,
                                   const void* right, size_t right_size)
{
    uintptr_t left_address;
    uintptr_t right_address;

    if (!left || !right || left_size == 0u || right_size == 0u) return 0;
    left_address = (uintptr_t)left;
    right_address = (uintptr_t)right;
    if (left_address <= right_address)
        return right_address - left_address < left_size;
    return left_address - right_address < right_size;
}

static int catalog_runtime_get_adapter_info(
    void* context, const void* source, RinGpuAdapterInfoV1* info_out)
{
    (void)context;
    return ringpu_get_adapter_info((const RinGpuCore*)source, info_out);
}

static int catalog_runtime_get_display_count(
    void* context, const void* source, uint32_t* count_out)
{
    (void)context;
    return ringpu_get_display_count((const RinGpuCore*)source, count_out);
}

static int catalog_runtime_get_display_info(
    void* context, const void* source, uint32_t index,
    RinGpuDisplayInfoV1* info_out)
{
    (void)context;
    return ringpu_get_display_info((const RinGpuCore*)source, index, info_out);
}

int rin_gpu_dxgi_catalog_init(RinGpuDxgiCatalog* catalog,
                              const RinGpuCore* const* adapters,
                              uint32_t adapter_count, uint64_t generation,
                              uint64_t handle_secret)
{
    const void* source_handles[RIN_GPU_DXGI_MAX_ADAPTERS];
    RinGpuDxgiCatalogSourceOpsV1 source_ops;
    uint32_t index;

    if (!catalog || !adapters || adapter_count == 0u ||
        adapter_count > RIN_GPU_DXGI_MAX_ADAPTERS || generation == 0u ||
        handle_secret == 0u ||
        catalog_runtime_overlap(catalog, sizeof(*catalog), adapters,
                                (size_t)adapter_count * sizeof(adapters[0])))
        return RIN_GPU_DXGI_INVALID_ARGUMENT;
    memset(&source_ops, 0, sizeof(source_ops));
    source_ops.struct_size = sizeof(source_ops);
    source_ops.version = RIN_GPU_DXGI_CATALOG_SOURCE_OPS_V1_VERSION;
    source_ops.get_adapter_info = catalog_runtime_get_adapter_info;
    source_ops.get_display_count = catalog_runtime_get_display_count;
    source_ops.get_display_info = catalog_runtime_get_display_info;
    for (index = 0u; index < adapter_count; ++index)
        source_handles[index] = adapters[index];
    return rin_gpu_dxgi_catalog_init_from_sources(
        catalog, source_handles, adapter_count, generation, handle_secret,
        &source_ops);
}
