# RinDX D3D11 software owner

`include/rindx/d3d11.h` and `src/d3d11/d3d11.c` provide the host software
execution contract for a D3D11 adapter. Device and context creation, feature
level 11_0 negotiation, RinGPU-backed buffer/2D texture/shader/pipeline
objects, explicit image transitions, render-pass/depth ownership, raster
state, draw/draw-indexed/instanced draw, dispatch, queue/fence submission,
and image readback are real operations over the RinGPU software executor.

The contract accepts validated RSH1 modules and versioned RinGPU descriptors;
it does not treat arbitrary DXBC/DXIL or a Windows COM pointer as executable
shader input. A future Windows DLL/COM adapter must lower those native ABIs
into this owner and propagate failures. Full Windows binary compatibility,
DXBC/DXIL frontend parity, external backend, physical GPU, IRQ/DMA, and
hardware evidence remain separate gates.
