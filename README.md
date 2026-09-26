# RinDX

RinDX owns the DXGI COM ABI, factory/adapter catalog, private-data runtime,
presentation-backed swapchain, and bounded D3D11/D3D12 software owners in
`include/rindx/d3d11.h` and `include/rindx/d3d12.h`. On Windows,
`RinDXNative.dll` exports the host software `D3D11Create*`, `D3D12CreateDevice`,
and `CreateDXGIFactory*` entry points. The native COM objects lower accepted
resource, command, fence, descriptor, PSO, queue, back-buffer, Present, and
ResizeBuffers operations to validated RinGPU/RSH1 execution.

Physical enumeration is supplied by the OS-Core adapter in
`src/drivers/gpu/rin_gpu_dxgi_catalog_platform.c`. D3D11/D3D12/DXGI DLL export
resolution is an explicit `RinDxProviderV1` boundary; RinNT owns policy and
does not embed a DXGI implementation. Physical GPU drivers, IRQ/DMA, external
backends, and hardware evidence remain outside this software split.

The native entry points are a host software ABI profile, not a claim that an
unmodified Windows application has full driver-level compatibility. DXBC/DXIL
and root-signature translation, physical adapter/driver execution, IRQ/DMA,
external backends, and OS display-plane ownership remain separate boundaries.
Unsupported native shapes are rejected before publication or submission;
accepted paths execute real RinGPU data and completion operations.

D3D11 additionally exposes bounded buffer/Texture2D UpdateSubresource and
GenerateMips. GenerateMips requires a fully upload-ready 2D mip chain and
lowers each adjacent level through RinGPU linear blit with explicit per-mip
state transitions; unsupported formats, dimensions, and incomplete chains are
rejected rather than synthesized. D3D11 depth/stencil clear is limited to the
validated D32_FLOAT_S8_UINT software path and requires explicit copy-state
transitions.

D3D11 also exposes a typed graphics bind-group owner. Its storage-image path is
exercised by a real RSH1 fragment shader and R8 storage image; binding kind,
access, resource usage, lifetime, and subresource validation remain in RinGPU,
and the draw writes/readbacks the storage image. Invalid or unsupported view
forms remain errors rather than fabricated native COM views.

The bounded D3D11/D3D12 pipeline owners also pass validated blend factors and
operations to RinGPU; the runtime tests cover a real enabled blend state and
reject an invalid factor before pipeline creation.

They also pass bounded depth format/compare/write state into the RinGPU native
pipeline owner and reject an invalid compare operation before allocation.

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
| Supported Windows profile | `RinDXNative.dll` exports D3D11/D3D12/DXGI creation and implements the tested host software COM profile, including native resource/view objects, command recording, queue/fence completion, PSO creation, DXGI factory/adapter private data, D3D11 and D3D12 swapchain buffers, Present, readback, GDI output, and ResizeBuffers. |
| Remaining boundary | Full Windows driver compatibility and every optional COM method, DXBC/DXIL/root-signature translation, physical enumeration/driver execution, IRQ/DMA, and external backends remain separate boundaries. |
| ownership | Caller owns buffers; returned COM-shaped interfaces follow their vtable AddRef/Release rules. Handles are not device addresses. |
| thread-safety | Synchronize shared mutable provider, catalog, and swapchain objects unless their interface states otherwise. |
| limits | Catalog capacity, record sizes, and swapchain bounds are defined by the public headers; provider features are explicit. |
| errors | Use declared HRESULT-like and provider status values; failed creation does not publish a usable object. |
| ABI stability | COM vtable order and versioned provider records are ABI. No full Windows binary compatibility is promised. |
| security | Public code has no private kernel dependency. Physical operations require the OS-Core provider boundary. |
| build | Standalone CMake target RinDX::RinDX; see the CMake command above and public RinGPU dependency. |
| test | Run registered CTest/Meson targets. Native MSVC coverage is `rindx-native-d3d-runtime`; portable owner coverage is `rindx-d3d11-runtime-test` and `rindx-d3d12-runtime-test`. Physical/QEMU/CTS/soak evidence remains separate. |
