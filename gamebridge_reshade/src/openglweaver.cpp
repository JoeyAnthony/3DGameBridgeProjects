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

void OpenGLWeaver::flip_buffer(int buffer_height, int buffer_width, reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest) {
    reshade::api::subresource_box vertically_flipped_box;
    vertically_flipped_box.left = 0;
    vertically_flipped_box.top = buffer_height; // Normally this would be 0 and bottom would be height if you are not trying to flip vertically
    vertically_flipped_box.front = 0;
    vertically_flipped_box.right = buffer_width;
    vertically_flipped_box.bottom = 0;
    vertically_flipped_box.back = 1;
    cmd_list->copy_texture_region(source, 0, nullptr, dest, 0, &vertically_flipped_box, reshade::api::filter_mode::min_mag_mip_point);
}

bool OpenGLWeaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc, reshade::api::command_list* cmd_list) {
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;
    desc.texture.format = reshade::api::format::r8g8b8a8_unorm; // Format matching the back buffer Todo: Check if this always works!
    desc.flags = reshade::api::resource_flags::none;
    desc.texture.depth_or_layers = 0;

    // Create buffer to store a copy of the effect frame
    reshade::api::resource_desc copy_rsc_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::shader_resource);
    if (!gl_device->create_resource(copy_rsc_desc, nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
        destroy_all_resources_and_resource_views();

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the effect frame copy");
        return false;
    }

    // Make shader resource view for the effect frame copy
    reshade::api::resource_view_desc srv_desc(reshade::api::resource_view_type::texture_2d, copy_rsc_desc.texture.format, 0, copy_rsc_desc.texture.levels, 0, copy_rsc_desc.texture.depth_or_layers);
    if (!gl_device->create_resource_view(effect_frame_copy, reshade::api::resource_usage::shader_resource, srv_desc, &effect_frame_copy_srv)) {
        destroy_all_resources_and_resource_views();

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the effect frame copy's resource view");
        return false;
    }

    // Create flipped buffers to correct for ReShade's built-in OpenGL specific buffer flip after rendering effects
    reshade::api::resource_desc flipped_copy_rsc_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::shader_resource);

    if (!gl_device->create_resource(flipped_copy_rsc_desc, nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy_flipped)) {
        destroy_all_resources_and_resource_views();

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the flipped effect frame copy");
        return false;
    }

    reshade::api::resource_view_desc copy_rtv_desc(reshade::api::resource_view_type::texture_2d, flipped_copy_rsc_desc.texture.format, 0, flipped_copy_rsc_desc.texture.levels, 0, flipped_copy_rsc_desc.texture.depth_or_layers);
    if (!gl_device->create_resource_view(effect_frame_copy_flipped, reshade::api::resource_usage::render_target, copy_rtv_desc, &effect_frame_copy_flipped_rtv)) {
        destroy_all_resources_and_resource_views();

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::info, "Failed creating the flipped effect frame copy's RTV");
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

    try {
        if (is_overlay_workaround_enabled()) {
            weaver = new SR::PredictingGLWeaver(*sr_context, desc.texture.width, desc.texture.height);
            weaver->setWindowHandle((HWND)runtime->get_hwnd());
        } else {
            weaver = new SR::PredictingGLWeaver(*sr_context, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        }
        GLuint renderedTextureID = effect_frame_copy.handle & 0xFFFFFFFF;
        weaver->setInputFrameBuffer(0, renderedTextureID); // Resourceview of the buffer
        sr_context->initialize();
        reshade::log_message(reshade::log_level::info, "Initialized weaver");
    }
    catch (std::runtime_error &e) {
        if (std::strcmp(e.what(), "Failed to load library") == 0) {
            return DLL_NOT_LOADED;
        }
        return GENERAL_FAIL;
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

    // Determine the default latency mode for the weaver
    determine_default_latency_mode();

    return SUCCESS;
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

    // Bind a viewport for the weaver in case there isn't one defined already. This happens when no effects are enabled in ReShade.
    const reshade::api::viewport viewport = {
            0.0f, 0.0f,
            static_cast<float>(desc.texture.width),
            static_cast<float>(desc.texture.height),
            0.0f, 1.0f
    };
    cmd_list->bind_viewports(0, 1, &viewport);

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
            destroy_all_resources_and_resource_views();
            if (!create_effect_copy_buffer(desc, cmd_list) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            GLuint renderedTextureID;
            // Set newly create buffer as input
            renderedTextureID = effect_frame_copy.handle & 0xFFFFFFFF;
            weaver->setInputFrameBuffer(0, renderedTextureID); // Resourceview of the buffer
            reshade::log_message(reshade::log_level::info, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                // Copy the RTV, ReShade supplies it to us upside down, so we flip it during this copy.
                flip_buffer(desc.texture.height, desc.texture.width, cmd_list, rtv_resource, effect_frame_copy);

                // Bind copy buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &effect_frame_copy_flipped_rtv);

                // Weave to copy buffer
                weaver->weave(desc.texture.width, desc.texture.height);

                // At this point, the buffer is rightside-up but ReShade expects it to be upside-down, so we have to flip it after weaving.
                flip_buffer(desc.texture.height, desc.texture.width, cmd_list, effect_frame_copy_flipped, rtv_resource);
            }
        }
    }
    else {
        // Set current buffer format
        current_buffer_format = desc.texture.format;

        // Set color format settings
        check_color_format(desc);

        create_effect_copy_buffer(desc, cmd_list);
        GbResult result = init_weaver(runtime, effect_frame_copy, cmd_list);
        if (result == SUCCESS) {
            GLuint renderedTextureID;
            // Ensure the RTV is compatible with OpenGL and cast to GLuint
            renderedTextureID = effect_frame_copy.handle & 0xFFFFFFFF;
            weaver->setInputFrameBuffer(0, renderedTextureID); // Resourceview of the buffer
        }
        else if (result == DLL_NOT_LOADED) {
            return DLL_NOT_LOADED;
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            destroy_all_resources_and_resource_views();
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return GENERAL_FAIL;
        }
    }
    return SUCCESS;
}

void OpenGLWeaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    gl_device = runtime->get_device();
}

void OpenGLWeaver::do_weave(bool do_weave) {
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
        weaver_latency_in_us = frametime_in_microseconds;
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

void OpenGLWeaver::destroy_all_resources_and_resource_views() {
    gl_device->destroy_resource(effect_frame_copy);
    gl_device->destroy_resource_view(effect_frame_copy_srv);
    gl_device->destroy_resource(effect_frame_copy_flipped);
    gl_device->destroy_resource_view(effect_frame_copy_flipped_rtv);
}
