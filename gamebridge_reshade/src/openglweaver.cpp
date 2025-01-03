/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "openglweaver.h"

OpenGLWeaver::OpenGLWeaver(SR::SRContext* context) {
    // Set context here.
    sr_context = context;
    weaving_enabled = true;
}

bool OpenGLWeaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc) {
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;
    desc.texture.format = reshade::api::format::r8g8b8a8_unorm; // Format matching the back buffer Todo: Check if this always works!

    // Create buffer to store a copy of the effect frame
    reshade::api::resource_desc copy_rsc_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::shader_resource);
    if (!gl_device->create_resource(copy_rsc_desc, nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
        gl_device->destroy_resource(effect_frame_copy);

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating te effect frame copy");
        return false;
    }

    // Make shader resource view for the effect frame copy
    reshade::api::resource_view_desc srv_desc(reshade::api::resource_view_type::texture_2d, copy_rsc_desc.texture.format, 0, copy_rsc_desc.texture.levels, 0, copy_rsc_desc.texture.depth_or_layers);
    if (!gl_device->create_resource_view(effect_frame_copy, reshade::api::resource_usage::shader_resource, srv_desc, &effect_frame_copy_srv)) {
        gl_device->destroy_resource(effect_frame_copy);
        gl_device->destroy_resource_view(effect_frame_copy_srv);

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the effect frame copy");
        return false;
    }

    // Create a sampler descriptor
    reshade::api::sampler_desc sampler_desc = {};
    sampler_desc.filter = reshade::api::filter_mode::min_mag_mip_linear; // Linear filtering for min, mag, and mip
    sampler_desc.address_u = reshade::api::texture_address_mode::clamp; // Clamp wrapping in the U direction
    sampler_desc.address_v = reshade::api::texture_address_mode::clamp; // Clamp wrapping in the V direction
    sampler_desc.address_w = reshade::api::texture_address_mode::clamp; // Clamp wrapping in the W direction (if applicable)

    // Create a sampler handle
    reshade::api::sampler sampler;
    if (!gl_device->create_sampler(sampler_desc, &sampler)) {
        reshade::log_message(reshade::log_level::error, "Failed to create sampler for texture filtering");
        return false;
    }

    effect_frame_copy_x = copy_rsc_desc.texture.width;
    effect_frame_copy_y = copy_rsc_desc.texture.height;

    return true;
}

GbResult OpenGLWeaver::init_weaver(reshade::api::effect_runtime *runtime, reshade::api::resource rtv,
                                      reshade::api::command_list *cmd_list) {
    if (weaver_initialized) {
        return SUCCESS;
    }

    delete weaver;
    weaver = nullptr;
    reshade::api::resource_desc desc = gl_device->get_resource_desc(rtv);
//    ID3D11Device* dev = (ID3D11Device*)gl_device->get_native();
//    ID3D11DeviceContext* context = (ID3D11DeviceContext*)cmd_list->get_native();

//    if (!dev) {
//        reshade::log_message(reshade::log_level::info, "Couldn't get a device");
//        return GENERAL_FAIL;
//    }

//    if (!context) {
//        reshade::log_message(reshade::log_level::info, "Couldn't get a device context");
//        return GENERAL_FAIL;
//    }

    // Retrieve the framebuffer and render target view from the runtime
    auto back_buffer_resource = runtime->get_back_buffer(0);

    GLuint frameBufferID;
    // Retrieve the OpenGL framebuffer ID directly
    frameBufferID = static_cast<GLuint>(back_buffer_resource.handle);

    GLuint renderedTextureID;
    // Ensure the RTV is compatible with OpenGL and cast to GLuint
    renderedTextureID = static_cast<GLuint>(rtv.handle);

    try {
        weaver = new SR::PredictingGLWeaver(*sr_context, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        weaver->setInputFrameBuffer(renderedTextureID, frameBufferID); // Resourceview of the buffer
//        weaver->setInputFrameBuffer(frameBufferID, renderedTextureID); // Resourceview of the buffer
        sr_context->initialize();
        reshade::log_message(reshade::log_level::info, "Initialized weaver");

        // Set mode to latency in frames by default.
        set_latency_frametime_adaptive(default_weaver_latency);
        std::string latency_log = "Current latency mode set to: STATIC " + std::to_string(default_weaver_latency) + " Microseconds";
        reshade::log_message(reshade::log_level::info, latency_log.c_str());
    }
    catch (std::runtime_error &e) {
        if (std::strcmp(e.what(), "Failed to load library") == 0) {
            return DLL_NOT_LOADED;
        }
    }
    catch (std::exception &e) {
        reshade::log_message(reshade::log_level::info, e.what());
        return GENERAL_FAIL;
    }
    catch (...) {
        reshade::log_message(reshade::log_level::info, "Couldn't initialize weaver");
        return GENERAL_FAIL;
    }

    weaver_initialized = true;
    return SUCCESS;
}

void OpenGLWeaver::draw_status_overlay(reshade::api::effect_runtime *runtime) {
    // Log activity status
    ImGui::TextUnformatted("Status: ACTIVE");

    // Log the latency mode
    std::string latencyModeDisplay = "Latency mode: ";
    if (current_latency_mode == LatencyModes::FRAMERATE_ADAPTIVE) {
        latencyModeDisplay += "IN " + std::to_string(last_latency_frame_time_set) + " MICROSECONDS";
    }
    else {
        latencyModeDisplay += "IN " + std::to_string(runtime->get_back_buffer_count()) + " FRAMES";
    }
    ImGui::TextUnformatted(latencyModeDisplay.c_str());

    // Log the buffer type, this can be removed once we've tested a larger amount of games.
    std::string s = "Buffer type: " + std::to_string(static_cast<uint32_t>(current_buffer_format));
    ImGui::TextUnformatted(s.c_str());
}

GbResult OpenGLWeaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    reshade::api::resource_view chosen_rtv;

    if (use_srgb_rtv) {
        chosen_rtv = rtv_srgb;
    }
    else {
        chosen_rtv = rtv;
    }

    reshade::api::resource rtv_resource = gl_device->get_resource_from_view(chosen_rtv);
    reshade::api::resource_desc desc = gl_device->get_resource_desc(rtv_resource);
    reshade::api::resource rtv_resource2 = runtime->get_back_buffer(0);
    reshade::api::resource_desc desc2 = gl_device->get_resource_desc(rtv_resource2);

    // Bind a viewport for the weaver in case there isn't one defined already. This happens when no effects are enabled in ReShade.
//    const reshade::api::viewport viewport = {
//            0.0f, 0.0f,
//            static_cast<float>(desc.texture.width),
//            static_cast<float>(desc.texture.height),
//            0.0f, 1.0f
//    };
//    cmd_list->bind_viewports(0, 1, &viewport);

    if (weaver_initialized) {
        // Check if we need to set the latency in frames.
        if (get_latency_mode() == LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC) {
            weaver->setLatencyInFrames(runtime->get_back_buffer_count()); // Set the latency with which the weaver should do prediction.
        }

        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            // Update current buffer format
            current_buffer_format = desc.texture.format;

            // Update color format settings
            check_color_format(desc);

            // Todo: Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation succeeds
            gl_device->destroy_resource(effect_frame_copy);
            gl_device->destroy_resource_view(effect_frame_copy_srv);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            GLuint frameBufferID;
            // Retrieve the OpenGL framebuffer ID directly
            frameBufferID = static_cast<GLuint>(effect_frame_copy_srv.handle);

            GLuint renderedTextureID;
            // Ensure the RTV is compatible with OpenGL and cast to GLuint
            renderedTextureID = static_cast<GLuint>(effect_frame_copy.handle);

            // Set newly create buffer as input
            weaver->setInputFrameBuffer(renderedTextureID, frameBufferID);
//            weaver->setInputFrameBuffer(frameBufferID, renderedTextureID);
            reshade::log_message(reshade::log_level::info, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                // Copy resource
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &chosen_rtv);

                // Weave to back buffer
                weaver->weave(desc.texture.width, desc.texture.height, 0, 0);
            }
        }
    }
    else {
        // Set current buffer format
        current_buffer_format = desc.texture.format;

        // Set color format settings
        check_color_format(desc);

        create_effect_copy_buffer(desc);
        GbResult result = init_weaver(runtime, effect_frame_copy, cmd_list);
        if (result == SUCCESS) {
//            // Set context and input frame buffer again to make sure they are correct
//            weaver->setContext((ID3D11DeviceContext*)cmd_list->get_native());

            GLuint frameBufferID;
            // Retrieve the OpenGL framebuffer ID directly
            frameBufferID = static_cast<GLuint>(effect_frame_copy_srv.handle);

            GLuint renderedTextureID;
            // Ensure the RTV is compatible with OpenGL and cast to GLuint
            renderedTextureID = static_cast<GLuint>(effect_frame_copy.handle);

            weaver->setInputFrameBuffer(renderedTextureID, frameBufferID);
//            weaver->setInputFrameBuffer(frameBufferID, renderedTextureID);
        }
        else if (result == DLL_NOT_LOADED) {
            return DLL_NOT_LOADED;
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            gl_device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return GENERAL_FAIL;
        }
    }
    return SUCCESS;
}

void OpenGLWeaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    gl_device = runtime->get_device();
}

void OpenGLWeaver::do_weave(bool do_weave)
{
    weaving_enabled = do_weave;
}

bool OpenGLWeaver::set_latency_in_frames(int32_t number_of_frames) {
    if (weaver_initialized) {
        if (number_of_frames < 0) {
            set_latency_mode(LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC);
        }
        else {
            set_latency_mode(LatencyModes::LATENCY_IN_FRAMES);
            weaver->setLatencyInFrames(number_of_frames);
        }
        return true;
    }
    return false;
}

bool OpenGLWeaver::set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) {
    if (weaver_initialized) {
        set_latency_mode(LatencyModes::FRAMERATE_ADAPTIVE);
        weaver->setLatency(frametime_in_microseconds);
        last_latency_frame_time_set = frametime_in_microseconds;
        return true;
    }
    return false;
}

void OpenGLWeaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

void OpenGLWeaver::check_color_format(reshade::api::resource_desc desc) {
    // Check buffer format and see if it's in the list of known SRGB ones. Change to SRGB rtv if so.
    if ((std::find(srgb_color_formats.begin(), srgb_color_formats.end(), desc.texture.format) != srgb_color_formats.end())) {
        // SRGB format detected, switch to SRGB buffer.
        use_srgb_rtv = true;
    }
    else {
        use_srgb_rtv = false;
    }
}

LatencyModes OpenGLWeaver::get_latency_mode() {
    return current_latency_mode;
}
