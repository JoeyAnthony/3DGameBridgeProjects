// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

static bool srContextInitialized = false;
static bool weaverInitialized = false;
SR::PredictingDX12Weaver* weaver = nullptr;
SR::SRContext* srContext = nullptr;
reshade::api::device* d3d12device = nullptr;
reshade::api::resource effect_frame_copy;

// My SR::EyePairListener that stores the last tracked eye positions
class MyEyes : public SR::EyePairListener {
private:
    SR::InputStream<SR::EyePairStream> stream;
public:
    DirectX::XMFLOAT3 left, right;
    MyEyes(SR::EyeTracker* tracker) : left(-30, 0, 600), right(30, 0, 600) {
        // Open a stream between tracker and this class
        stream.set(tracker->openEyePairStream(this));
    }
    // Called by the tracker for each tracked eye pair
    virtual void accept(const SR_eyePair& eyePair) override
    {
        // Remember the eye positions
        left = DirectX::XMFLOAT3(eyePair.left.x, eyePair.left.y, eyePair.left.z);
        right = DirectX::XMFLOAT3(eyePair.right.x, eyePair.right.y, eyePair.right.z);
    }
};
MyEyes* eyes = nullptr;

void init_sr_context(reshade::api::effect_runtime* runtime) {
    // Just in case it's being called multiple times
    if (!srContextInitialized) {
        srContext = new SR::SRContext;
        eyes = new MyEyes(SR::EyeTracker::create(*srContext));
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

void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaverInitialized) {
        return;
    }

    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    if (d3d12device) {
        reshade::log_message(3, "Can cast a device! to native Dx12 Device!!!!");
    }
    else {
        reshade::log_message(3, "Could not cast device!!!!");
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

    uint64_t back_buffer_index = runtime->get_current_back_buffer_index();
    char buffer[1000];
    sprintf(buffer, "backbuffer num: %l", back_buffer_index);
    reshade::api::resource res = runtime->get_current_back_buffer();
    uint64_t handle = res.handle;

    sprintf(buffer, "resource value: %l", handle);
    reshade::log_message(4, buffer);

    

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;

    //[Create Test RTV]

    if (runtime->get_current_back_buffer().handle == (uint64_t)INVALID_HANDLE_VALUE) {
        reshade::log_message(3, "backbuffer handle is invalid");
    }

    //ID3D12Resource* framebuffer = (ID3D12Resource*)d3d12device->get_resource_from_view(rtv).handle;
    try {
        weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        reshade::log_message(3, "Initialized weaver");
    }
    catch (std::exception e) {
        reshade::log_message(3, e.what());
    }
    catch (...) {
        reshade::log_message(3, "Couldn't initialize weaver");
    }

    weaverInitialized = true;
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

reshade::api::command_list* command_list;
reshade::api::resource_view game_frame_buffer;
static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);

    if (!weaverInitialized) {
        init_weaver(runtime, rtv_resource, runtime->get_current_back_buffer());

        reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);
        desc.type = reshade::api::resource_type::texture_2d;
        desc.usage = reshade::api::resource_usage::copy_dest;
        d3d12device->create_resource(desc, nullptr, reshade::api::resource_usage::unordered_access, &effect_frame_copy);
    }

    //const float color[] = {1.f, 0.3f, 0.5f, 1.f};
    //cmd_list->clear_render_target_view(rtv, color);
    //const float color2[] = { 0.0f, 0.5f, 0.2f, 1.f };
    //cmd_list->clear_render_target_view(rsv, color2);

    if (weaverInitialized) {
        reshade::api::resource_view view;
        d3d12device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d12device->get_resource_view_desc(rtv), &view);

        reshade::api::command_list* const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
        cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_dest);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(rtv_resource, effect_frame_copy);
        cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::unordered_access);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        //const float color[] = { 1.f, 0.3f, 0.5f, 1.f };
        //cmd_list->clear_render_target_view(view, color);
        cmd_list->bind_render_targets_and_depth_stencil(1, &view);

        weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
        ID3D12GraphicsCommandList* native_cmd_list = (ID3D12GraphicsCommandList*)cmd_list->get_native();
        weaver->setCommandList(native_cmd_list);
        weaver->weave(3820, 2160);
    }
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
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
