/* SPDX-License-Identifier: MIT */
#if defined(_WIN32)

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>
#include <d3d12.h>

#include <stdio.h>
#include <string.h>

static int check_hr(HRESULT result, const char* expression, int line) {
    if (FAILED(result)) {
        fprintf(stderr, "native check failed at %d: %s (0x%08lx)\n",
                line, expression, (unsigned long)result);
        return 0;
    }
    return 1;
}

static LRESULT CALLBACK test_window_proc(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcA(window, message, wparam, lparam);
}

#define CHECK_HR(expression) \
    do { if (!check_hr((expression), #expression, __LINE__)) return 1; } while (0)
#define CHECK(expression) \
    do { if (!(expression)) { fprintf(stderr, "native check failed at %d: %s\n", \
                                      __LINE__, #expression); return 1; } } while (0)

int main(void) {
    ID3D11Device* d3d11_device = NULL;
    ID3D11DeviceContext* d3d11_context = NULL;
    ID3D11Buffer* buffer = NULL;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    D3D11_BUFFER_DESC buffer_desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D12Device* d3d12_device = NULL;
    ID3D12CommandQueue* d3d12_queue = NULL;
    ID3D12CommandAllocator* d3d12_allocator = NULL;
    ID3D12GraphicsCommandList* d3d12_list = NULL;
    ID3D12Fence* d3d12_fence = NULL;
    ID3D12CommandAllocator* resource_allocator = NULL;
    ID3D12GraphicsCommandList* resource_list = NULL;
    ID3D12Resource* upload_resource = NULL;
    ID3D12Resource* color_resource = NULL;
    ID3D12DescriptorHeap* rtv_heap = NULL;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE root_signature;
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    D3D12_HEAP_PROPERTIES heap_properties;
    D3D12_RESOURCE_DESC resource_desc;
    D3D12_CLEAR_VALUE clear_value;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    void* mapped_resource = NULL;
    HANDLE fence_event = NULL;
    WNDCLASSA window_class;
    HWND window;
    DXGI_SWAP_CHAIN_DESC swap_desc;
    IDXGISwapChain* swapchain = NULL;
    IDXGIFactory1* factory = NULL;
    IDXGIAdapter1* adapter = NULL;
    IDXGISwapChain* factory_swapchain = NULL;
    IDXGISwapChain* d3d12_swapchain = NULL;
    ID3D11Texture2D* backbuffer = NULL;
    ID3D11RenderTargetView* render_target = NULL;
    ID3D11Device* swap_device = NULL;
    ID3D11DeviceContext* swap_context = NULL;
    ID3D12Resource* d3d12_backbuffer = NULL;
    ID3D12DescriptorHeap* d3d12_swap_rtv_heap = NULL;
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12_swap_rtv_handle;
    static const GUID private_guid =
        {0x6a5b4c3d, 0x2e1f, 0x4a59, {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11}};
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = test_window_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "RinDXNativeSwapchainTest";
    CHECK(RegisterClassA(&window_class) != 0u);
    window = CreateWindowExA(0u, window_class.lpszClassName, "RinDX",
                             WS_OVERLAPPEDWINDOW, 0, 0, 2, 2, NULL, NULL,
                             window_class.hInstance, NULL);
    CHECK(window != NULL);

    CHECK_HR(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0u,
                               &level, 1u, D3D11_SDK_VERSION,
                               &d3d11_device, &level, &d3d11_context));
    CHECK(d3d11_device != NULL && d3d11_context != NULL);
    CHECK(ID3D11Device_GetFeatureLevel(d3d11_device) == D3D_FEATURE_LEVEL_11_0);
    memset(&buffer_desc, 0, sizeof(buffer_desc));
    buffer_desc.ByteWidth = 64u;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECK_HR(ID3D11Device_CreateBuffer(d3d11_device, &buffer_desc, NULL,
                                       &buffer));
    memset(&mapped, 0, sizeof(mapped));
    CHECK_HR(ID3D11DeviceContext_Map(d3d11_context, (ID3D11Resource*)buffer,
                                     0u, D3D11_MAP_WRITE, 0u, &mapped));
    CHECK(mapped.pData != NULL && mapped.RowPitch == 64u);
    memset(mapped.pData, 0x5a, 64u);
    ID3D11DeviceContext_Unmap(d3d11_context, (ID3D11Resource*)buffer, 0u);
    ID3D11DeviceContext_Flush(d3d11_context);
    ID3D11Buffer_Release(buffer);
    ID3D11DeviceContext_Release(d3d11_context);
    ID3D11Device_Release(d3d11_device);

    memset(&swap_desc, 0, sizeof(swap_desc));
    swap_desc.BufferDesc.Width = 2u;
    swap_desc.BufferDesc.Height = 2u;
    swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.BufferDesc.RefreshRate.Numerator = 60u;
    swap_desc.BufferDesc.RefreshRate.Denominator = 1u;
    swap_desc.SampleDesc.Count = 1u;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2u;
    swap_desc.OutputWindow = window;
    swap_desc.Windowed = TRUE;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    CHECK_HR(D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0u, NULL, 0u,
        D3D11_SDK_VERSION, &swap_desc, &swapchain, &swap_device, &level,
        &swap_context));
    CHECK_HR(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory));
    {
        BYTE private_value = 0x5au;
        BYTE private_readback = 0u;
        UINT private_size = sizeof(private_readback);
        CHECK_HR(IDXGIFactory1_SetPrivateData(
            factory, &private_guid, sizeof(private_value), &private_value));
        CHECK_HR(IDXGIFactory1_GetPrivateData(
            factory, &private_guid, &private_size, &private_readback));
        CHECK(private_size == sizeof(private_value) &&
              private_readback == private_value);
    }
    CHECK_HR(IDXGIFactory1_EnumAdapters1(factory, 0u, &adapter));
    {
        DXGI_ADAPTER_DESC1 adapter_desc;
        BYTE private_value = 0x33u;
        BYTE private_readback = 0u;
        UINT private_size = sizeof(private_readback);
        CHECK_HR(IDXGIAdapter1_GetDesc1(adapter, &adapter_desc));
        CHECK((adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u);
        CHECK_HR(IDXGIAdapter1_SetPrivateData(
            adapter, &private_guid, sizeof(private_value), &private_value));
        CHECK_HR(IDXGIAdapter1_GetPrivateData(
            adapter, &private_guid, &private_size, &private_readback));
        CHECK(private_size == sizeof(private_value) &&
              private_readback == private_value);
    }
    CHECK_HR(IDXGIFactory1_CreateSwapChain(
        factory, (IUnknown*)swap_device, &swap_desc, &factory_swapchain));
    IDXGISwapChain_Release(factory_swapchain);
    IDXGIAdapter1_Release(adapter);
    IDXGIFactory1_Release(factory);
    factory_swapchain = NULL;
    adapter = NULL;
    factory = NULL;
    CHECK_HR(IDXGISwapChain_GetBuffer(
        swapchain, 0u, &IID_ID3D11Texture2D, (void**)&backbuffer));
    CHECK_HR(ID3D11Device_CreateRenderTargetView(
        swap_device, (ID3D11Resource*)backbuffer, NULL, &render_target));
    ID3D11DeviceContext_OMSetRenderTargets(swap_context, 1u, &render_target,
                                            NULL);
    {
        const FLOAT color[4] = {0.1f, 0.2f, 0.3f, 1.0f};
        ID3D11DeviceContext_ClearRenderTargetView(swap_context, render_target,
                                                  color);
    }
    ID3D11DeviceContext_Flush(swap_context);
    CHECK_HR(IDXGISwapChain_Present(swapchain, 1u, 0u));
    CHECK_HR(IDXGISwapChain_ResizeBuffers(swapchain, 2u, 2u, 2u,
                                          DXGI_FORMAT_R8G8B8A8_UNORM, 0u));
    ID3D11RenderTargetView_Release(render_target);
    ID3D11Texture2D_Release(backbuffer);
    IDXGISwapChain_Release(swapchain);
    ID3D11DeviceContext_Release(swap_context);
    ID3D11Device_Release(swap_device);
    CHECK_HR(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_0,
                               &IID_ID3D12Device, (void**)&d3d12_device));
    CHECK(d3d12_device != NULL);
    CHECK(ID3D12Device_GetNodeCount(d3d12_device) == 1u);
    memset(&root_signature, 0, sizeof(root_signature));
    CHECK_HR(ID3D12Device_CheckFeatureSupport(
        d3d12_device, D3D12_FEATURE_ROOT_SIGNATURE, &root_signature,
        sizeof(root_signature)));
    CHECK(root_signature.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_0);
    CHECK_HR(ID3D12Device_GetDeviceRemovedReason(d3d12_device));
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 1u;
    CHECK_HR(ID3D12Device_CreateCommandQueue(
        d3d12_device, &queue_desc, &IID_ID3D12CommandQueue,
        (void**)&d3d12_queue));
    CHECK_HR(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory));
    CHECK_HR(IDXGIFactory1_CreateSwapChain(
        factory, (IUnknown*)d3d12_queue, &swap_desc, &d3d12_swapchain));
    CHECK_HR(IDXGISwapChain_GetBuffer(
        d3d12_swapchain, 0u, &IID_ID3D12Resource,
        (void**)&d3d12_backbuffer));
    memset(&heap_desc, 0, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = 1u;
    heap_desc.NodeMask = 1u;
    CHECK_HR(ID3D12Device_CreateDescriptorHeap(
        d3d12_device, &heap_desc, &IID_ID3D12DescriptorHeap,
        (void**)&d3d12_swap_rtv_heap));
    CHECK(ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
              d3d12_swap_rtv_heap, &d3d12_swap_rtv_handle) != NULL);
    ID3D12Device_CreateRenderTargetView(
        d3d12_device, d3d12_backbuffer, NULL, d3d12_swap_rtv_handle);
    CHECK_HR(ID3D12Device_CreateCommandAllocator(
        d3d12_device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator, (void**)&resource_allocator));
    CHECK_HR(ID3D12Device_CreateCommandList(
        d3d12_device, 1u, D3D12_COMMAND_LIST_TYPE_DIRECT,
        resource_allocator, NULL, &IID_ID3D12GraphicsCommandList,
        (void**)&resource_list));
    ID3D12GraphicsCommandList_OMSetRenderTargets(
        resource_list, 1u, &d3d12_swap_rtv_handle, FALSE, NULL);
    {
        const FLOAT swap_color[4] = {0.05f, 0.15f, 0.25f, 1.0f};
        ID3D12GraphicsCommandList_ClearRenderTargetView(
            resource_list, d3d12_swap_rtv_handle, swap_color, 0u, NULL);
    }
    CHECK_HR(ID3D12GraphicsCommandList_Close(resource_list));
    ID3D12CommandQueue_ExecuteCommandLists(
        d3d12_queue, 1u, (ID3D12CommandList* const*)&resource_list);
    CHECK_HR(IDXGISwapChain_Present(d3d12_swapchain, 1u, 0u));
    ID3D12GraphicsCommandList_Release(resource_list);
    ID3D12CommandAllocator_Release(resource_allocator);
    ID3D12DescriptorHeap_Release(d3d12_swap_rtv_heap);
    ID3D12Resource_Release(d3d12_backbuffer);
    d3d12_backbuffer = NULL;
    CHECK_HR(IDXGISwapChain_ResizeBuffers(
        d3d12_swapchain, 2u, 2u, 2u, DXGI_FORMAT_R8G8B8A8_UNORM, 0u));
    IDXGISwapChain_Release(d3d12_swapchain);
    IDXGIFactory1_Release(factory);
    resource_list = NULL;
    resource_allocator = NULL;
    d3d12_swap_rtv_heap = NULL;
    d3d12_swapchain = NULL;
    factory = NULL;
    CHECK_HR(ID3D12Device_CreateCommandAllocator(
        d3d12_device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator, (void**)&d3d12_allocator));
    CHECK_HR(ID3D12Device_CreateCommandList(
        d3d12_device, 1u, D3D12_COMMAND_LIST_TYPE_DIRECT,
        d3d12_allocator, NULL, &IID_ID3D12GraphicsCommandList,
        (void**)&d3d12_list));
    CHECK_HR(ID3D12GraphicsCommandList_Close(d3d12_list));
    ID3D12CommandQueue_ExecuteCommandLists(
        d3d12_queue, 1u, (ID3D12CommandList* const*)&d3d12_list);
    CHECK_HR(ID3D12Device_CreateFence(
        d3d12_device, 0u, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence,
        (void**)&d3d12_fence));
    CHECK_HR(ID3D12CommandQueue_Signal(d3d12_queue, d3d12_fence, 1u));
    CHECK(ID3D12Fence_GetCompletedValue(d3d12_fence) == 1u);
    CHECK_HR(ID3D12CommandQueue_Wait(d3d12_queue, d3d12_fence, 1u));
    fence_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(fence_event != NULL);
    CHECK_HR(ID3D12Fence_SetEventOnCompletion(d3d12_fence, 1u,
                                               fence_event));
    CHECK(WaitForSingleObject(fence_event, 0u) == WAIT_OBJECT_0);
    CloseHandle(fence_event);
    memset(&heap_properties, 0, sizeof(heap_properties));
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_properties.CreationNodeMask = 1u;
    heap_properties.VisibleNodeMask = 1u;
    memset(&resource_desc, 0, sizeof(resource_desc));
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = 64u;
    resource_desc.Height = 1u;
    resource_desc.DepthOrArraySize = 1u;
    resource_desc.MipLevels = 1u;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1u;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    CHECK_HR(ID3D12Device_CreateCommittedResource(
        d3d12_device, &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
        (void**)&upload_resource));
    CHECK_HR(ID3D12Resource_Map(upload_resource, 0u, NULL, &mapped_resource));
    CHECK(mapped_resource != NULL);
    memset(mapped_resource, 0x5a, 64u);
    ID3D12Resource_Unmap(upload_resource, 0u, NULL);
    ID3D12Resource_Release(upload_resource);
    upload_resource = NULL;

    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    memset(&resource_desc, 0, sizeof(resource_desc));
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Width = 2u;
    resource_desc.Height = 2u;
    resource_desc.DepthOrArraySize = 1u;
    resource_desc.MipLevels = 1u;
    resource_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resource_desc.SampleDesc.Count = 1u;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clear_value.Color[3] = 1.0f;
    CHECK_HR(ID3D12Device_CreateCommittedResource(
        d3d12_device, &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_COMMON, &clear_value, &IID_ID3D12Resource,
        (void**)&color_resource));
    memset(&heap_desc, 0, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = 1u;
    heap_desc.NodeMask = 1u;
    CHECK_HR(ID3D12Device_CreateDescriptorHeap(
        d3d12_device, &heap_desc, &IID_ID3D12DescriptorHeap,
        (void**)&rtv_heap));
    CHECK(ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
              rtv_heap, &rtv_handle) != NULL);
    ID3D12Device_CreateRenderTargetView(
        d3d12_device, color_resource, NULL, rtv_handle);
    CHECK_HR(ID3D12Device_CreateCommandAllocator(
        d3d12_device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        &IID_ID3D12CommandAllocator, (void**)&resource_allocator));
    CHECK_HR(ID3D12Device_CreateCommandList(
        d3d12_device, 1u, D3D12_COMMAND_LIST_TYPE_DIRECT,
        resource_allocator, NULL, &IID_ID3D12GraphicsCommandList,
        (void**)&resource_list));
    ID3D12GraphicsCommandList_OMSetRenderTargets(
        resource_list, 1u, &rtv_handle, FALSE, NULL);
    {
        const FLOAT clear_color[4] = {0.25f, 0.5f, 0.75f, 1.0f};
        ID3D12GraphicsCommandList_ClearRenderTargetView(
            resource_list, rtv_handle, clear_color, 0u, NULL);
    }
    CHECK_HR(ID3D12GraphicsCommandList_Close(resource_list));
    ID3D12CommandQueue_ExecuteCommandLists(
        d3d12_queue, 1u, (ID3D12CommandList* const*)&resource_list);
    ID3D12GraphicsCommandList_Release(resource_list);
    ID3D12CommandAllocator_Release(resource_allocator);
    ID3D12DescriptorHeap_Release(rtv_heap);
    ID3D12Resource_Release(color_resource);
    resource_list = NULL;
    resource_allocator = NULL;
    rtv_heap = NULL;
    color_resource = NULL;
    ID3D12GraphicsCommandList_Release(d3d12_list);
    ID3D12CommandAllocator_Release(d3d12_allocator);
    ID3D12CommandQueue_Release(d3d12_queue);
    ID3D12Fence_Release(d3d12_fence);
    ID3D12Device_Release(d3d12_device);
    DestroyWindow(window);
    UnregisterClassA(window_class.lpszClassName, window_class.hInstance);
    puts("rindx-native-d3d-runtime PASS");
    return 0;
}

#endif /* _WIN32 */
