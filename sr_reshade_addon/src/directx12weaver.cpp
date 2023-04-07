#include "directx12weaver.h"

DirectX12Weaver current_weaver;
reshade::api::effect_runtime* current_runtime;
reshade::api::resource_view latest_created_rtv;

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context)
{
    //Set context here.
    srContext = context;
    weaving_enabled = true;
}

DirectX12Weaver::DirectX12Weaver() {}

bool DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaver_initialized) {
        return weaver_initialized;
    }

    // See if we can get a command allocator from reshade
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    if (!d3d12device) {
        reshade::log_message(3, "Couldn't get a device");
        return false;
    }

    ID3D12CommandAllocator* CommandAllocator;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));
    if (CommandAllocator == nullptr) {
        reshade::log_message(3, "Couldn't ceate command allocator");
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
        reshade::log_message(3, "Couldn't create command queue");
        return false;
    }

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;

    // Reshade command queue for use later maybe
    //(ID3D12CommandQueue*)runtime->get_command_queue()->get_native()
    try {
        weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        srContext->initialize();
        reshade::log_message(3, "Initialized weaver");

        // Todo: Make this setting of latency/mode a helper function.
        // Set mode to latency in frames by default.
        set_latency_mode(LatencyModes::framerateAdaptive);
        // Todo: The amount of buffers set here should be configurable!
        set_latency_framerate_adaptive(40000);
        reshade::log_message(3, "Current latency mode set to: STATIC 40000 Microseconds");
    }
    catch (std::exception e) {
        reshade::log_message(3, e.what());
        return false;
    }
    catch (...) {
        reshade::log_message(3, "Couldn't initialize weaver");
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

        reshade::log_message(3, "Failed creating te effect frame copy");
        return false;
    }

    effect_frame_copy_x = desc.texture.width;
    effect_frame_copy_y = desc.texture.height;

    return true;
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects) {
    reshade::api::resource backBuffer = swapchain->get_current_back_buffer();
    reshade::api::resource_desc desc = queue->get_device()->get_resource_desc(backBuffer);
    reshade::api::command_list* cmd_list = queue->get_immediate_command_list();

    current_weaver.on_present_called(current_runtime, cmd_list, backBuffer, desc);
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
                reshade::log_message(2, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
            reshade::log_message(3, "Buffer size changed");
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
            reshade::log_message(3, "Failed to initialize weaver");
            return;
        }
    }
}

void DirectX12Weaver::on_present_called(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource rtv_resource, reshade::api::resource_desc desc) {
    if (weaver_initialized) {
        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            //TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation suceeds
            d3d12device->destroy_resource(effect_frame_copy);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(2, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
            reshade::log_message(3, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                // Create copy of the effect buffer
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &latest_created_rtv);

                // Weave to back buffer
                weaver->weave(desc.texture.width, desc.texture.height);
            }
        }
    }
    else {
        create_effect_copy_buffer(desc);
        if (init_weaver(runtime, effect_frame_copy, d3d12device->get_resource_from_view(latest_created_rtv))) {
            //Set command list and input frame buffer again to make sure they are correct
            weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            d3d12device->destroy_resource(effect_frame_copy);
            reshade::log_message(3, "Failed to initialize weaver");
            return;
        }
    }
}

static void on_init_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc& desc, reshade::api::resource_view view) {
    latest_created_rtv = view;
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
    current_weaver = *this;
    current_runtime = runtime;
    reshade::register_event<reshade::addon_event::present>(&on_present);
    reshade::register_event<reshade::addon_event::init_resource_view >(&on_init_resource_view);
}

void DirectX12Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}

bool DirectX12Weaver::set_latency_in_frames(int numberOfFrames) {
    if (current_latency_mode == LatencyModes::latencyInFrames) {
        weaver->setLatencyInFrames(numberOfFrames);
        return true;
    }
    return false;
}

bool DirectX12Weaver::set_latency_framerate_adaptive(int frametimeInMicroseconds) {
    if (current_latency_mode == LatencyModes::framerateAdaptive) {
        weaver->setLatency(frametimeInMicroseconds);
        return true;
    }
    return false;
}

void DirectX12Weaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

LatencyModes DirectX12Weaver::get_latency_mode() {
    return current_latency_mode;
}
