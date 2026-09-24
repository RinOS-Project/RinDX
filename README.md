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
