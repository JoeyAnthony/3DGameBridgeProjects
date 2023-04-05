#include "directx11weaver.h"
#include <numeric>

DirectX11Weaver::DirectX11Weaver(SR::SRContext* context) {
    //Set context here.
    srContext = context;
    weaving_enabled = true;
}

bool DirectX11Weaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc)
{
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;

    // Create buffer to store a copy of the effect frame
    reshade::api::resource_desc copy_rsc_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource);
    if (!d3d11device->create_resource(copy_rsc_desc,nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
        d3d11device->destroy_resource(effect_frame_copy);

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(3, "Failed creating te effect frame copy");
        return false;
    }

    // Make shader resource view for the effect frame copy
    reshade::api::resource_view_desc srv_desc(reshade::api::resource_view_type::texture_2d, copy_rsc_desc.texture.format, 0, copy_rsc_desc.texture.levels, 0, copy_rsc_desc.texture.depth_or_layers);
    if (!d3d11device->create_resource_view(effect_frame_copy, reshade::api::resource_usage::shader_resource, srv_desc, &effect_frame_copy_srv)) {
        d3d11device->destroy_resource(effect_frame_copy);
        d3d11device->destroy_resource_view(effect_frame_copy_srv);

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(3, "Failed creating te effect frame copy");
        return false;
    }

    effect_frame_copy_x = copy_rsc_desc.texture.width;
    effect_frame_copy_y = copy_rsc_desc.texture.height;

    return true;
}

bool DirectX11Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list) {
    if (weaver_initialized) {
        return weaver_initialized;
    }

    delete weaver;
    weaver = nullptr;
    reshade::api::resource_desc desc = d3d11device->get_resource_desc(rtv);
    ID3D11Device* dev = (ID3D11Device*)d3d11device->get_native();
    ID3D11DeviceContext* context = (ID3D11DeviceContext*)cmd_list->get_native();

    if (!dev) {
        reshade::log_message(3, "Couldn't get a device");
        return false;
    }

    if (!context) {
        reshade::log_message(3, "Couldn't get a device context");
        return false;
    }

    try {
        weaver = new SR::PredictingDX11Weaver(*srContext, dev, context, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        weaver->setContext((ID3D11DeviceContext*)cmd_list->get_native());
        weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)rtv.handle); //resourceview of the buffer.
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

void DirectX11Weaver::draw_debug_overlay(reshade::api::effect_runtime* runtime)
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

void DirectX11Weaver::draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

void DirectX11Weaver::draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

void DirectX11Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    reshade::api::resource rtv_resource = d3d11device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d11device->get_resource_desc(rtv_resource);

    if (weaver_initialized) {
        //Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            //TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation suceeds
            d3d11device->destroy_resource(effect_frame_copy);
            d3d11device->destroy_resource_view(effect_frame_copy_srv);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(2, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)effect_frame_copy_srv.handle);
            reshade::log_message(3, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                // Copy resource
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &rtv);

                // Weave to back buffer
                weaver->weave(desc.texture.width, desc.texture.height);
                
                // Todo: Make sure to limit the latency to the monitor's max refresh rate!!!
                // Todo: Make this a helper function of iGraphicsAPI
                // Calculate the current frametime and set the latency of the eye tracker accordingly. Only do this in framerate adaptive latency mode.
                //if (get_latency_mode() == LatencyModes::framerateAdaptive) {
                //    auto current_time = std::chrono::high_resolution_clock::now();
                //    long long frame_time_in_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(current_time - clock_last_frame).count();

                //    // Take average of vector containing past frame times, then set the eye tracker latency to the average.
                //    weaver->setLatency(std::reduce(&frame_time_list[0], &frame_time_list[frame_time_list_size - 1]) / frame_time_list_size);

                //    // Update list of past frames and set list index back to 0 once we reach the 100th item in the list at which point we reset the index to 0.
                //    frame_time_list[frame_time_index] = frame_time_in_microseconds;
                //    frame_time_index = frame_time_index++ % 100;

                //    // Update the last frame timestamp
                //    clock_last_frame = current_time;
                //}
            }
        }
    }
    else {
        clock_last_frame = std::chrono::high_resolution_clock::now();
        create_effect_copy_buffer(desc);
        if (init_weaver(runtime, effect_frame_copy, cmd_list)) {
            // Set context and input frame buffer again to make sure they are correct
            weaver->setContext((ID3D11DeviceContext*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)effect_frame_copy_srv.handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            d3d11device->destroy_resource(effect_frame_copy);
            reshade::log_message(3, "Failed to initialize weaver");
            return;
        }
    }
}

void DirectX11Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d11device = runtime->get_device();
}

void DirectX11Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}

bool DirectX11Weaver::set_latency_in_frames(int numberOfFrames) {
    if (current_latency_mode == LatencyModes::latencyInFrames) {
        weaver->setLatencyInFrames(numberOfFrames);
        return true;
    }
    return false;
}

bool DirectX11Weaver::set_latency_framerate_adaptive(int frametimeInMicroseconds) {
    if (current_latency_mode == LatencyModes::framerateAdaptive) {
        weaver->setLatency(frametimeInMicroseconds);
        return true;
    }
    return false;
}

void DirectX11Weaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

LatencyModes DirectX11Weaver::get_latency_mode() {
    return current_latency_mode;
}
