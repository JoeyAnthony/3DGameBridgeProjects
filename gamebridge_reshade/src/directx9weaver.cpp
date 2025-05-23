/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "directx9weaver.h"

// Directx
#include <DirectXMath.h>

#include "configManager.h"

DirectX9Weaver::DirectX9Weaver(SR::SRContext* context) {
    // Set context here.
    sr_context = context;
    weaving_enabled = true;
}

bool DirectX9Weaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc) {
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

GbResult DirectX9Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list) {
    if (weaver_initialized) {
        return SUCCESS;
    }

    delete weaver;
    weaver = nullptr;
    reshade::api::resource_desc desc = d3d9_device->get_resource_desc(rtv);
    IDirect3DDevice9* dev = (IDirect3DDevice9*)d3d9_device->get_native();

    if (!dev) {
        reshade::log_message(reshade::log_level::info, "Couldn't get a device");
        return GENERAL_FAIL;
    }

    try {
        if (is_overlay_workaround_enabled()) {
            reshade::log_message(reshade::log_level::warning, "Overlay workaround is not supported in DirectX 9, please disable any overlays such as the Discord overlay if you have issues enabling 3D.");
        }
        weaver = new SR::PredictingDX9Weaver(*sr_context, dev, desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        weaver->setInputFrameBuffer((IDirect3DTexture9*)rtv.handle); // Resourceview of the buffer
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

GbResult DirectX9Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    reshade::api::resource_view chosen_rtv;

    if (use_srgb_rtv) {
        chosen_rtv = rtv_srgb;
    }
    else {
        chosen_rtv = rtv;
    }

    reshade::api::resource rtv_resource = d3d9_device->get_resource_from_view(chosen_rtv);
    reshade::api::resource_desc desc = d3d9_device->get_resource_desc(rtv_resource);

    if (weaver_initialized) {
        // Check if we need to set the latency in frames.
        if (get_latency_mode() == LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC) {
            weaver->setLatencyInFrames(runtime->get_back_buffer_count()); // Set the latency with which the weaver should do prediction.
        }

        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            // Update current buffer format
            current_buffer_format = desc.texture.format;

            // Invalidate weaver device objects before resizing the resource.
            weaver->invalidateDeviceObjects();

            // Update color format settings.
            check_color_format(desc);

            // TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation suceeds
            d3d9_device->destroy_resource(effect_frame_copy);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Restore weaver device objects after resizing the resource.
            weaver->restoreDeviceObjects();

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
                cmd_list->bind_render_targets_and_depth_stencil(1, &chosen_rtv);

                // Weave to back buffer
                weaver->weave(desc.texture.width, desc.texture.height);
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
            // Set context and input frame buffer again to make sure they are correct
            weaver->setInputFrameBuffer((IDirect3DTexture9*)effect_frame_copy.handle);
        }
        else if (result == DLL_NOT_LOADED) {
            return DLL_NOT_LOADED;
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            d3d9_device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return GENERAL_FAIL;
        }
    }
    return SUCCESS;
}

void DirectX9Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d9_device = runtime->get_device();
}

void DirectX9Weaver::do_weave(bool do_weave)
{
    weaving_enabled = do_weave;
}

bool DirectX9Weaver::set_latency_in_frames(int32_t number_of_frames) {
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

bool DirectX9Weaver::set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) {
    if (weaver_initialized) {
        set_latency_mode(LatencyModes::FRAMERATE_ADAPTIVE);
        weaver->setLatency(frametime_in_microseconds);
        weaver_latency_in_us = frametime_in_microseconds;
        return true;
    }
    return false;
}

void DirectX9Weaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

void DirectX9Weaver::check_color_format(reshade::api::resource_desc desc) {
    // Check buffer format and see if it's in the list of known SRGB ones. Change to SRGB rtv if so.
    if ((std::find(srgb_color_formats.begin(), srgb_color_formats.end(), desc.texture.format) != srgb_color_formats.end())) {
        // SRGB format detected, switch to SRGB buffer.
        use_srgb_rtv = true;
    }
    else {
        use_srgb_rtv = false;
    }
}

LatencyModes DirectX9Weaver::get_latency_mode() {
    return current_latency_mode;
}

