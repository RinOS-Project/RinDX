# RinDX ownership

RinDX owns the DXGI COM ABI, adapter/output catalog, private-data state, and
swapchain translation.  Its only graphics dependency is the public,
graphics-neutral RinGPU API.

OS-Core owns physical adapter enumeration and translates its runtime display
topology into `RinDxgiDisplayTopologyV1`.  RinDX does not include OS-Core
private headers and does not implement a hidden fallback when a physical
provider is missing.
