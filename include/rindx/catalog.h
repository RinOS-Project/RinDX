/* SPDX-License-Identifier: MIT */
#ifndef RINDX_PUBLIC_CATALOG_H
#define RINDX_PUBLIC_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#include <ringpu/ringpu.h>

#include <rindx/display_topology.h>

typedef struct RinGpuPhysicalDriver RinGpuPhysicalDriver;

typedef int (*RinGpuDxgiOutputTopologyProviderFn)(
    void* context, RinGpuPhysicalDriver* driver,
    const RinGpuDisplayInfoV1* display,
    RinDxgiDisplayTopologyV1* topology_out,
    RinDxgiDisplayTargetV1* target_out);

#define RIN_GPU_DXGI_CATALOG_VERSION 1u
#define RIN_GPU_DXGI_MAX_ADAPTERS 8u
#define RIN_GPU_DXGI_MAX_OUTPUTS \
    (RIN_GPU_DXGI_MAX_ADAPTERS * RIN_GPU_MAX_DISPLAYS)
#define RIN_GPU_DXGI_ADAPTER_DESCRIPTION_UNITS 128u
#define RIN_GPU_DXGI_OUTPUT_DESCRIPTION_UNITS 64u
#define RIN_GPU_DXGI_CATALOG_STATE_QWORDS 8192u

typedef uint64_t RinGpuDxgiHandle;

typedef enum RinGpuDxgiCatalogResult {
    RIN_GPU_DXGI_OK = 0,
    RIN_GPU_DXGI_INVALID_ARGUMENT = -1,
    RIN_GPU_DXGI_STATE = -2,
    RIN_GPU_DXGI_BUSY = -3,
    RIN_GPU_DXGI_STALE = -4,
    RIN_GPU_DXGI_NOT_FOUND = -5,
    RIN_GPU_DXGI_SOURCE_FAILED = -6,
    RIN_GPU_DXGI_PROTOCOL = -7,
    RIN_GPU_DXGI_LIMIT = -8,
    RIN_GPU_DXGI_UNSUPPORTED = -9
} RinGpuDxgiCatalogResult;

/* Values are the public DXGI_FORMAT numeric values used by Windows clients. */
#define RIN_GPU_DXGI_FORMAT_R8G8B8A8_UNORM 28u
#define RIN_GPU_DXGI_FORMAT_B8G8R8A8_UNORM 87u

#define RIN_GPU_DXGI_OUTPUT_CONNECTED UINT32_C(0x00000001)
#define RIN_GPU_DXGI_OUTPUT_PRIMARY UINT32_C(0x00000002)
#define RIN_GPU_DXGI_OUTPUT_KNOWN_FLAGS UINT32_C(0x00000003)

/* Pointer-free, fixed-width data for a later IDXGIAdapter1 ABI adapter. */
typedef struct RinGpuDxgiAdapterSnapshotV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t vendor_id;
    uint32_t device_id;
    uint64_t dedicated_video_memory_bytes;
    uint64_t shared_system_memory_bytes;
    uint32_t queue_capabilities;
    uint32_t flags;
    uint64_t adapter_luid;
    uint16_t description[RIN_GPU_DXGI_ADAPTER_DESCRIPTION_UNITS];
} RinGpuDxgiAdapterSnapshotV1;

/* RinGPU does not yet own desktop coordinates, rotation, or HMONITOR.  This
 * normalized descriptor publishes only fields backed by the display owner. */
typedef struct RinGpuDxgiOutputSnapshotV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t adapter_luid;
    uint32_t display_id;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t refresh_numerator;
    uint32_t refresh_denominator;
    uint32_t dxgi_format;
    uint32_t physical_width_mm;
    uint32_t physical_height_mm;
    uint32_t scale_milli;
    uint32_t reserved0;
    uint32_t reserved1;
    uint16_t description[RIN_GPU_DXGI_OUTPUT_DESCRIPTION_UNITS];
} RinGpuDxgiOutputSnapshotV1;

#define RIN_GPU_DXGI_MEMORY_BUDGET_LOCAL_VALID UINT32_C(0x00000001)
#define RIN_GPU_DXGI_MEMORY_BUDGET_SYSTEM_VALID UINT32_C(0x00000002)
#define RIN_GPU_DXGI_MEMORY_BUDGET_KNOWN_FLAGS \
    (RIN_GPU_DXGI_MEMORY_BUDGET_LOCAL_VALID | \
     RIN_GPU_DXGI_MEMORY_BUDGET_SYSTEM_VALID)

/* Live heap accounting for a physical adapter.  This is the data source for
 * a later DXGI_QUERY_VIDEO_MEMORY_INFO projection; it is not a promise that
 * the compositor or another process has reserved any untracked memory. */
typedef struct RinGpuDxgiMemoryBudgetV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t flags;
    uint32_t reserved0;
    uint64_t local_budget_bytes;
    uint64_t local_usage_bytes;
    uint64_t local_available_bytes;
    uint64_t system_budget_bytes;
    uint64_t system_usage_bytes;
    uint64_t system_available_bytes;
    uint64_t device_epoch;
    uint64_t iommu_map_generation;
    uint32_t active_allocation_count;
    uint32_t cleanup_pending_count;
    uint64_t reserved[2];
} RinGpuDxgiMemoryBudgetV1;

typedef struct RinGpuDxgiCatalog {
    uint64_t opaque[RIN_GPU_DXGI_CATALOG_STATE_QWORDS];
} RinGpuDxgiCatalog;

/* The catalog snapshots public RinGPU identity/display data and retains the
 * caller-owned core pointers only for is_current().  Every core must therefore
 * outlive catalog shutdown.  generation and handle_secret are nonzero and must
 * change when a process publishes a replacement adapter catalog. */
int rin_gpu_dxgi_catalog_init(RinGpuDxgiCatalog* catalog,
                              const RinGpuCore* const* adapters,
                              uint32_t adapter_count, uint64_t generation,
                              uint64_t handle_secret);
/* Builds the catalog directly from active physical-driver owners.  The
 * drivers and their cores remain caller-owned and must outlive the catalog;
 * a lost, detached, or invalid driver is never represented as an adapter. */
int rin_gpu_dxgi_catalog_init_from_physical_drivers(
    RinGpuDxgiCatalog* catalog, RinGpuPhysicalDriver* const* drivers,
    uint32_t driver_count, uint64_t generation, uint64_t handle_secret);
/* Internal completion of the physical-driver initializer.  The catalog keeps
 * these non-owning pointers only so IDXGIOutput::GetDesc can read the same
 * generation-bound compositor topology as the physical display owner. */
int rin_gpu_dxgi_catalog_bind_physical_sources(
    RinGpuDxgiCatalog* catalog, RinGpuPhysicalDriver* const* drivers,
    uint32_t driver_count, RinGpuDxgiOutputTopologyProviderFn provider,
    void* provider_context);
int rin_gpu_dxgi_query_memory_budget_from_physical_driver(
    const RinGpuPhysicalDriver* driver, RinGpuDxgiMemoryBudgetV1* budget_out);
int rin_gpu_dxgi_catalog_shutdown(RinGpuDxgiCatalog* catalog);

int rin_gpu_dxgi_catalog_enumerate_adapter(
    RinGpuDxgiCatalog* catalog, uint32_t ordinal,
    RinGpuDxgiHandle* adapter_out, RinGpuDxgiAdapterSnapshotV1* snapshot_out);
int rin_gpu_dxgi_catalog_query_adapter(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiAdapterSnapshotV1* snapshot_out);
int rin_gpu_dxgi_catalog_enumerate_output(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter, uint32_t ordinal,
    RinGpuDxgiHandle* output_out, RinGpuDxgiOutputSnapshotV1* snapshot_out);
int rin_gpu_dxgi_catalog_query_output(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiHandle output, RinGpuDxgiOutputSnapshotV1* snapshot_out);
/* Return the compositor target backing one physical DXGI output.  Both
 * structures are copied from the live owner and must have the same
 * generation; an unbound physical catalog reports UNSUPPORTED instead of
 * manufacturing desktop coordinates. */
int rin_gpu_dxgi_catalog_query_output_topology(
    RinGpuDxgiCatalog* catalog, RinGpuDxgiHandle adapter,
    RinGpuDxgiHandle output, RinDxgiDisplayTopologyV1* topology_out,
    RinDxgiDisplayTargetV1* target_out);

/* Returns OK with current_out=0 for hot-unplug, device loss, or any public
 * descriptor drift.  A generation mismatch is STALE and also returns zero. */
int rin_gpu_dxgi_catalog_is_current(RinGpuDxgiCatalog* catalog,
                                    uint64_t expected_generation,
                                    uint32_t* current_out);

#if defined(__cplusplus)
static_assert(sizeof(RinGpuDxgiAdapterSnapshotV1) == 304u,
              "RinGPU DXGI adapter snapshot drift");
static_assert(sizeof(RinGpuDxgiOutputSnapshotV1) == 192u,
              "RinGPU DXGI output snapshot drift");
#else
_Static_assert(sizeof(RinGpuDxgiAdapterSnapshotV1) == 304u,
               "RinGPU DXGI adapter snapshot drift");
_Static_assert(sizeof(RinGpuDxgiOutputSnapshotV1) == 192u,
               "RinGPU DXGI output snapshot drift");
#endif

#endif /* RINDX_PUBLIC_CATALOG_H */
