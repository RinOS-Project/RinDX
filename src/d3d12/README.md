# RinDX D3D12 owner

`include/rindx/d3d12.h` and `d3d12.c` provide a bounded host software owner
for the D3D12-style queue, allocator/list, resource state, render-pass,
descriptor-heap, draw/dispatch, fence and readback path. Every operation
lowers to validated RinGPU runtime calls and failed operations do not publish
usable state.

The owner is deliberately not a Windows binary runtime: COM/DLL export
compatibility, root-signature/DXIL translation, complete D3D12 semantics,
physical adapters, drivers, IRQ/DMA, external backends, and hardware evidence
remain separate incomplete boundaries. The CTest target
`rindx-d3d12-runtime` covers the host software path.
