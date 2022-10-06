#include "directx12weaver.h"

bool srContextInitialized = false;
bool weaverInitialized = false;
SR::PredictingDX12Weaver* weaver = nullptr;
SR::SRContext* srContext = nullptr;
reshade::api::device* d3d12device = nullptr;

void DirectX12Weaver::init_sr_context(reshade::api::effect_runtime* runtime) {
    // Just in case it's being called multiple times
    if (!srContextInitialized) {
        srContext = new SR::SRContext;
        //eyes = new MyEyes(SR::EyeTracker::create(*srContext));
        srContext->initialize();
        srContextInitialized = true;
    }
}

void DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaverInitialized) {
        return;
    }

    // See if we can get a command allocator from reshade
    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    if (d3d12device) {
        reshade::log_message(3, "Couldn't get a device");
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

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;

    // Reshade command queue for use later maybe
    //(ID3D12CommandQueue*)runtime->get_command_queue()->get_native()

    try {
        weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        srContext->initialize();
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

void DirectX12Weaver::draw_debug_overlay(reshade::api::effect_runtime* runtime)
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

void DirectX12Weaver::draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

void DirectX12Weaver::draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);

    if (!weaverInitialized) {
        reshade::log_message(3, "init effect buffer copy");
        desc.type = reshade::api::resource_type::texture_2d;
        desc.heap = reshade::api::memory_heap::gpu_only;
        desc.usage = reshade::api::resource_usage::copy_dest;

        if (!d3d12device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest),
            nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {

            reshade::log_message(3, "Failed creating te effect frame copy");
            return;
        }

        init_weaver(runtime, effect_frame_copy, d3d12device->get_resource_from_view(rtv));

        // Set command list and input frame buffer again to make sure they are correct
        weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
        weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);

        // Create resource view for the backbuffer
        d3d12device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d12device->get_resource_view_desc(rtv), &back_buffer_rtv);
    }

    if (weaverInitialized) {
        // Create copy of the effect buffer
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(rtv_resource, effect_frame_copy);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        // Bind back buffer as render target
        cmd_list->bind_render_targets_and_depth_stencil(1, &back_buffer_rtv);

        // Weave to back buffer
        weaver->weave(desc.texture.width, desc.texture.height);
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
    init_sr_context(runtime);
}

bool DirectX12Weaver::is_initialized()
{
    return srContextInitialized;
}
