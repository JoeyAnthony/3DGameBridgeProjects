#include "directx12weaver.h"

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context)
{
    //Set context here.
    srContext = context;
    weaving_enabled = true;
}

bool DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaver_initialized) {
        return weaver_initialized;
    }

    // See if we can get a command allocator from reshade
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    if (!d3d12device) {
        reshade::log_message(reshade::log_level::info, "Couldn't get a device");
        return false;
    }

    ID3D12CommandAllocator* CommandAllocator;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));
    if (CommandAllocator == nullptr) {
        reshade::log_message(reshade::log_level::info, "Couldn't ceate command allocator");
        return false;
    }

    // Describe and create the command queue.
    ID3D12CommandQueue* CommandQueue;
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    dev->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));
    if (CommandQueue == nullptr)
    {
        reshade::log_message(reshade::log_level::info, "Couldn't create command queue");
        return false;
    }

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;

    // Reshade command queue for use later maybe
    //(ID3D12CommandQueue*)runtime->get_command_queue()->get_native()
    try {
        weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        weaver->setLatencyInFrames(1);
        srContext->initialize();
        reshade::log_message(reshade::log_level::info, "Initialized weaver");
    }
    catch (std::exception e) {
        reshade::log_message(reshade::log_level::info, e.what());
        return false;
    }
    catch (...) {
        reshade::log_message(reshade::log_level::info, "Couldn't initialize weaver");
        return false;
    }

    weaver_initialized = true;
    return weaver_initialized;
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

bool DirectX12Weaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc) {
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;

    if (!d3d12device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest),
        nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating te effect frame copy");
        return false;
    }

    effect_frame_copy_x = desc.texture.width;
    effect_frame_copy_y = desc.texture.height;

    return true;
}

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);

    if (weaver_initialized) {
        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            //TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation suceeds
            d3d12device->destroy_resource(effect_frame_copy);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
            reshade::log_message(reshade::log_level::info, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                // Create copy of the effect buffer
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &rtv);

                // Weave to back buffer
                weaver->weave(desc.texture.width, desc.texture.height);
            }
        }
    }
    else {
        create_effect_copy_buffer(desc);
        if (init_weaver(runtime, effect_frame_copy, d3d12device->get_resource_from_view(rtv))) {
            //Set command list and input frame buffer again to make sure they are correct
            weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            d3d12device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return;
        }
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
}

void DirectX12Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}
