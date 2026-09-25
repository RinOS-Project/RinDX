# RinDX

RinDX owns the DXGI COM ABI, factory/adapter/output catalog, private-data
runtime, presentation-backed swapchain, and bounded D3D11/D3D12 software
owners in `include/rindx/d3d11.h` and `include/rindx/d3d12.h`. These owners
execute validated RinGPU/RSH1 workloads through explicit host contracts; they
do not claim Windows DLL/COM binary compatibility or accept unchecked
DXBC/DXIL.

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
| Supported API | Public headers: include/rindx/com.h, catalog.h, display_topology.h, provider.h, swapchain.h, d3d11.h, and d3d12.h. |
| Unsupported API | Not a complete Windows DXGI/D3D binary runtime. Full D3D11/D3D12 COM/DLL, DXBC/DXIL/root-signature translation, physical enumeration, drivers, IRQ/DMA, and external backends remain separate boundaries. |
| ownership | Caller owns buffers; returned COM-shaped interfaces follow their vtable AddRef/Release rules. Handles are not device addresses. |
| thread-safety | Synchronize shared mutable provider, catalog, and swapchain objects unless their interface states otherwise. |
| limits | Catalog capacity, record sizes, and swapchain bounds are defined by the public headers; provider features are explicit. |
| errors | Use declared HRESULT-like and provider status values; failed creation does not publish a usable object. |
| ABI stability | COM vtable order and versioned provider records are ABI. No full Windows binary compatibility is promised. |
| security | Public code has no private kernel dependency. Physical operations require the OS-Core provider boundary. |
| build | Standalone CMake target RinDX::RinDX; see the CMake command above and public RinGPU dependency. |
| test | Run registered CTest/Meson targets. The bounded D3D11 and D3D12 owners are covered by `rindx-d3d11-runtime-test` and `rindx-d3d12-runtime-test`; physical and Windows loader evidence remains separate. |
