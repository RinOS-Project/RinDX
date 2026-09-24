# RinDX

RinDX owns the DXGI COM ABI, factory/adapter/output catalog, private-data
runtime, and presentation-backed swapchain. It depends only on public
graphics-neutral RinGPU headers and links as `RinDX::RinDX` in the standalone
CMake project.

Physical enumeration is supplied by the OS-Core adapter in
`src/drivers/gpu/rin_gpu_dxgi_catalog_platform.c`. D3D11/D3D12/DXGI DLL export
resolution is an explicit `RinDxProviderV1` boundary; RinNT owns policy and
does not embed a DXGI implementation. Physical GPU drivers, IRQ/DMA, external
backends, and hardware evidence remain outside this software split.

Build:

```sh
cmake -S . -B build -DRINGPU_DIR=../RinGPU
cmake --build build
```

## Public API contract

| Requirement | Contract |
| --- | --- |
| Purpose | DXGI COM-shaped API, adapter/output catalog, provider seam, and presentation-backed swapchain for RinOS. |
| Supported API | Public headers: include/rindx/com.h, catalog.h, display_topology.h, provider.h, and swapchain.h. |
| Unsupported API | Not a complete Windows DXGI or D3D runtime. Physical enumeration, drivers, IRQ/DMA, and external backends remain OS-Core/provider responsibilities. |
| ownership | Caller owns buffers; returned COM-shaped interfaces follow their vtable AddRef/Release rules. Handles are not device addresses. |
| thread-safety | Synchronize shared mutable provider, catalog, and swapchain objects unless their interface states otherwise. |
| limits | Catalog capacity, record sizes, and swapchain bounds are defined by the public headers; provider features are explicit. |
| errors | Use declared HRESULT-like and provider status values; failed creation does not publish a usable object. |
| ABI stability | COM vtable order and versioned provider records are ABI. No full Windows binary compatibility is promised. |
| security | Public code has no private kernel dependency. Physical operations require the OS-Core provider boundary. |
| build | Standalone CMake target RinDX::RinDX; see the CMake command above and public RinGPU dependency. |
| test | Run registered CTest/Meson targets and repo CI. No tests/builds were run for this README update. |
