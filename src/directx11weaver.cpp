#include "directx11weaver.h"

DirectX11Weaver::DirectX11Weaver(SR::SRContext* context) {
    //Set context here.
    srContext = context;
}

void DirectX11Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list) {
    if (weaverInitialized) {
        return;
    }

    delete weaver;
    weaver = nullptr;
    reshade::api::resource_desc desc = d3d11device->get_resource_desc(rtv);
    ID3D11Device* dev = (ID3D11Device*)d3d11device->get_native();
    ID3D11DeviceContext* context = (ID3D11DeviceContext*)cmd_list->get_native();

    if (dev) {
        reshade::log_message(3, "Can cast a device! to native Dx11 Device!!!!");
    }
    else {
        reshade::log_message(3, "Could not cast device!!!!");
        return;
    }

    if (context) {
        reshade::log_message(3, "Can cast a context! to native Dx11 context!!!!");
    }
    else {
        reshade::log_message(3, "Could not cast context!!!!");
        return;
    }

    try {
        weaver = new SR::PredictingDX11Weaver(*srContext, dev, context, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        ID3D11DeviceContext* native_cmd_list = (ID3D11DeviceContext*)cmd_list->get_native();
        weaver->setContext(native_cmd_list);
        weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)rtv.handle); //resourceview of the buffer
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

    if (!weaverInitialized) {
        reshade::log_message(3, "init effect buffer copy");
        desc.type = reshade::api::resource_type::texture_2d;
        desc.heap = reshade::api::memory_heap::gpu_only;
        desc.usage = reshade::api::resource_usage::copy_dest;

        if (d3d11device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::shader_resource),
            nullptr, reshade::api::resource_usage::shader_resource, &effect_frame_copy)) {
            reshade::log_message(3, "Created resource");
        }
        else {
            reshade::log_message(3, "Failed creating resource");
            return;
        }

        init_weaver(runtime, effect_frame_copy, cmd_list);
    }

    if (weaverInitialized) {
        reshade::api::resource_view view;
        d3d11device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d11device->get_resource_view_desc(rtv), &view);

        // Copy resource
        cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);

        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(rtv_resource, effect_frame_copy);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        // Make shader resource view from the copied buffer
        cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);

        //const float color[] = { 1.f, 0.3f, 0.5f, 1.f };
        //cmd_list->clear_render_target_view(view, color);
        cmd_list->bind_render_targets_and_depth_stencil(1, &view);

        ID3D11DeviceContext* native_cmd_list = (ID3D11DeviceContext*)cmd_list->get_native();
        ID3D11DeviceContext* native_device_context = (ID3D11DeviceContext*)cmd_list->get_native();

        reshade::api::resource_view effect_frame_copy_srv;
        reshade::api::resource_view_desc srv_desc(reshade::api::resource_view_type::texture_2d, desc.texture.format, 0, desc.texture.levels, 0, desc.texture.depth_or_layers);
        d3d11device->create_resource_view(effect_frame_copy, reshade::api::resource_usage::shader_resource, srv_desc, &effect_frame_copy_srv);

        try {
            weaver->setContext(native_cmd_list);
            // TODO probably doesn't work
            weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)effect_frame_copy_srv.handle);
            weaver->weave(desc.texture.width, desc.texture.height);
        }
        catch (std::exception e) {
            reshade::log_message(3, e.what());
        }
        catch (...) {
            reshade::log_message(3, "Couldn't initialize weaver");
        }
    }
}

void DirectX11Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d11device = runtime->get_device();
}

bool DirectX11Weaver::is_initialized() {
    return false;
}
