#include "directx12weaver.h"

bool srContextInitialized = false;
bool weaverInitialized = false;
SR::PredictingDX12Weaver* weaver = nullptr;
reshade::api::device* d3d12device = nullptr;

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context)
{
    //Set context here.
    if (!srContextInitialized) {
        srContext = context;
        srContextInitialized = true;
        weavingEnabled = true;
    }
}

void DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaverInitialized) {
        return;
    }

    // See if we can get a command allocator from reshade
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    if (!d3d12device) {
        reshade::log_message(3, "Couldn't get a device");
        return;
    }

    ID3D12CommandAllocator* CommandAllocator;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));
    if (CommandAllocator == nullptr) {
        reshade::log_message(3, "Couldn't ceate command allocator");
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
        reshade::log_message(3, "Couldn't create command queue");
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

bool DirectX12Weaver::create_efect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc) {
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;

    if (!d3d12device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest),
        nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(3, "Failed creating te effect frame copy");
        return false;
    }

    effect_frame_copy_x = desc.texture.width;
    effect_frame_copy_y = desc.texture.height;

    return true;
}

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);

    if (!weaverInitialized) {
        reshade::log_message(3, "init effect buffer copy");

        create_efect_copy_buffer(desc);

        init_weaver(runtime, effect_frame_copy, d3d12device->get_resource_from_view(rtv));
        if (!weaverInitialized) {
            reshade::log_message(3, "Failed to initialize weaver");

            // When buffer creation succeeds and this fails, delete the create buffer
            d3d12device->destroy_resource(effect_frame_copy);
            return;
        }

        //Set command list and input frame buffer again to make sure they are correct
        weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
        weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
    }

    if (weaverInitialized) {
        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            d3d12device->destroy_resource(effect_frame_copy);
            if (!create_efect_copy_buffer(desc)) {
                // Turn SR off or deinitialize and retry maybe?
            }
            reshade::log_message(3, "Buffer size changed");
        }

        // Create resource view for the backbuffer
        reshade::api::resource_view back_buffer_rtv;
        d3d12device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d12device->get_resource_view_desc(rtv), &back_buffer_rtv);

        // Create copy of the effect buffer
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(rtv_resource, effect_frame_copy);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        // Bind back buffer as render target
        cmd_list->bind_render_targets_and_depth_stencil(1, &back_buffer_rtv);

        // Weave to back buffer
        if (weavingEnabled) {
            weaver->weave(desc.texture.width, desc.texture.height);
        }
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
}

void DirectX12Weaver::do_weave(bool doWeave)
{
    weavingEnabled = doWeave;
}

bool DirectX12Weaver::is_initialized() {
    return srContextInitialized;
}
