// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

bool srContextInitialized = false;
bool weaverInitialized = false;
SR::PredictingDX12Weaver* weaver = nullptr;
SR::SRContext* srContext = nullptr;
reshade::api::device* d3d12device = nullptr;

void init_sr_context(reshade::api::effect_runtime* runtime) {
    // Just in case it's being called multiple times
    if (!srContextInitialized) {
        srContext = new SR::SRContext;
        srContext->initialize();
        srContextInitialized = true;
    }
}

ID3D12Resource* CreateTestResourceTexture(ID3D12Device* device, unsigned int width, unsigned int height) {
    D3D12_CLEAR_VALUE ClearMask = {};
    ClearMask.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(ClearMask.Format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ID3D12Resource* Framebuffer;
    device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &ClearMask, IID_PPV_ARGS(&Framebuffer));
    return Framebuffer;
}

ID3D12Resource* CreateTestResourceTexture(ID3D12Device* device, D3D12_RESOURCE_DESC ResourceDesc) {
    D3D12_CLEAR_VALUE ClearMask = {};
    ClearMask.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ID3D12Resource* Framebuffer;
    device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &ClearMask, IID_PPV_ARGS(&Framebuffer));
    return Framebuffer;
}

ID3D12DescriptorHeap* CreateRTVTestHeap(ID3D12Device* device, UINT& heapSize) {
    // Create RTV heap
    ID3D12DescriptorHeap* Heap;
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.NumDescriptors = 1;
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&Heap));
    heapSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return Heap;
}

void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource_view rtv) {
    if (weaverInitialized) {
        return;
    }

    d3d12device = runtime->get_device();

    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    if (d3d12device) {
        reshade::log_message(4, "Can cast a device! to native Dx12 Device!!!!");
    }
    else {
        reshade::log_message(4, "Could not cast device!!!!");
        return;
    }

    // Describe and create the command queue.
    ID3D12CommandQueue* CommandQueue;
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    dev->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));

    if (CommandQueue == nullptr)
    {
        return;
    }

    //uint32_t back_buffer_index = runtime->get_current_back_buffer_index();
    //char buffer[1000];
    //sprintf(buffer, "backbuffer num: %i", back_buffer_index);
    //reshade::api::resource res = runtime->get_current_back_buffer();
    //uint64_t handle = res.handle;

    //sprintf(buffer, "resource value: %i", handle);
    //reshade::log_message(4, buffer);
    //ID3D12Resource* back_buffer = reinterpret_cast<ID3D12Resource*>(handle);

    //[Create Test RTV]

    // Create texture
    //const reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
    //reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);
    ID3D12Resource* testFramebufferResource = CreateTestResourceTexture(dev, 3840, 2160);

    // Create heap
    UINT TestRTVHeapSize = 0;
    ID3D12DescriptorHeap* TestRTVHeap = CreateRTVTestHeap(dev, TestRTVHeapSize);
    // Create framebuffer RTV at slot 0
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(TestRTVHeap->GetCPUDescriptorHandleForHeapStart());
    dev->CreateRenderTargetView(testFramebufferResource, nullptr, rtvHandle);

    //[Create Test RTV]

    if (runtime->get_current_back_buffer().handle == (uint64_t)INVALID_HANDLE_VALUE) {

    }

    ID3D12Resource* framebuffer = (ID3D12Resource*)d3d12device->get_resource_from_view(rtv).handle;
    weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue, framebuffer, testFramebufferResource);

    weaverInitialized = true;
    reshade::log_message(4, "Initialized weaver");
}

bool g_popup_window_visible = false;
float view_separation = 0.f;
float vertical_shift = 0.f;

static void draw_debug_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::TextUnformatted("Some text");

    if (ImGui::Button("Press me to open an additional popup window"))
        g_popup_window_visible = true;

    if (g_popup_window_visible)
    {
        ImGui::Begin("Popup", &g_popup_window_visible);
        ImGui::TextUnformatted("Some other text");
        ImGui::End();
    }
}

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list*, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource res = runtime->get_current_back_buffer();
    init_weaver(runtime, rtv);

    //weaver->setInputFrameBuffer((ID3D12Resource*)d3d12device->get_resource_from_view(rtv).handle);
    //weaver->setCommandList((ID3D12GraphicsCommandList*)command_list->get_native());
    //weaver->weave(1920, 1080);
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    init_sr_context(runtime);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

        if (!reshade::register_addon(hModule))
            return FALSE;



        reshade::register_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);

        reshade::register_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);

        //reshade::register_overlay("Test", &draw_debug_overlay);
        //reshade::register_overlay(nullptr, &draw_sr_settings_overlay);
        //reshade::log_message(3, "registered: draw_sr_settings_overlay");

        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hModule);
        //reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
        //reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
        break;
    }
    return TRUE;
}
