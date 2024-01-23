#include "directx9weaver.h"

DirectX9Weaver::DirectX9Weaver(SR::SRContext* context) {
    //Set context here.
    sr_context = context;
    weaving_enabled = true;
}

bool DirectX9Weaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc)
{
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;

    // Create buffer to store a copy of the effect frame
    // Make sure to use reshade::api::resource_usage::render_target when creating the resource because we are copying from the RTV and the destination resource has to match the RTV resource usage type.
    reshade::api::resource_desc copy_rsc_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::render_target);
    if (!d3d9_device->create_resource(copy_rsc_desc, nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
        d3d9_device->destroy_resource(effect_frame_copy);

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the effect frame copy");
        return false;
    }

    effect_frame_copy_x = copy_rsc_desc.texture.width;
    effect_frame_copy_y = copy_rsc_desc.texture.height;

    return true;
}

bool DirectX9Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list) {
    if (weaver_initialized) {
        return weaver_initialized;
    }

    delete weaver;
    weaver = nullptr;
    reshade::api::resource_desc desc = d3d9_device->get_resource_desc(rtv);
    IDirect3DDevice9* dev = (IDirect3DDevice9*)d3d9_device->get_native();

    if (!dev) {
        reshade::log_message(reshade::log_level::info, "Couldn't get a device");
        return false;
    }

    try {
        weaver = new SR::PredictingDX9Weaver(*sr_context, dev, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        weaver->setInputFrameBuffer((IDirect3DTexture9*)rtv.handle); //resourceview of the buffer
        sr_context->initialize();
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

void DirectX9Weaver::draw_debug_overlay(reshade::api::effect_runtime* runtime)
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

void DirectX9Weaver::draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

void DirectX9Weaver::draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

void DirectX9Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    reshade::api::resource rtv_resource = d3d9_device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d9_device->get_resource_desc(rtv_resource);

    if (weaver_initialized) {
        //Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            //TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation suceeds
            d3d9_device->destroy_resource(effect_frame_copy);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((IDirect3DTexture9*)effect_frame_copy.handle);
            reshade::log_message(reshade::log_level::info, "Buffer size changed");
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
            }
        }
    }
    else {
        create_effect_copy_buffer(desc);
        if (init_weaver(runtime, effect_frame_copy, cmd_list)) {
            // Set context and input frame buffer again to make sure they are correct
            weaver->setInputFrameBuffer((IDirect3DTexture9*)effect_frame_copy.handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            d3d9_device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return;
        }
    }
}

void DirectX9Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d9_device = runtime->get_device();
}

void DirectX9Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}

// Called before IDirect3DDevice9::Reset
void DirectX9Weaver::on_destroy_swapchain(reshade::api::swapchain *swapchain) {
    if (weaver) {
        weaver->invalidateDeviceObjects();
    }
}
