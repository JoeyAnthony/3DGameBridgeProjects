/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "directx12weaver.h"

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context) {
    // Set context here.
    sr_context = context;
    weaving_enabled = true;
}

GbResult DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaver_initialized) {
        return SUCCESS;
    }

    // See if we can get a command allocator from reshade
    ID3D12Device* dev = ((ID3D12Device*)d3d12_device->get_native());
    if (!dev) {
        reshade::log_message(reshade::log_level::info, "Couldn't get a device");
        return GENERAL_FAIL;
    }

    ID3D12CommandAllocator* command_allocator;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));
    if (command_allocator == nullptr) {
        reshade::log_message(reshade::log_level::info, "Couldn't ceate command allocator");
        return GENERAL_FAIL;
    }

    // Describe and create the command queue.
    ID3D12CommandQueue* command_queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    dev->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue));
    if (command_queue == nullptr)
    {
        reshade::log_message(reshade::log_level::info, "Couldn't create command queue");
        return GENERAL_FAIL;
    }

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;
    try {
        weaver = new SR::PredictingDX12Weaver(*sr_context, dev, command_allocator, command_queue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        sr_context->initialize();
        reshade::log_message(reshade::log_level::info, "Initialized weaver");
    }
    catch (std::runtime_error &e) {
        if (std::strcmp(e.what(), "Failed to load library") == 0) {
            return DLL_NOT_LOADED;
        }
        return GENERAL_FAIL;
    }
    catch (std::exception& e) {
        reshade::log_message(reshade::log_level::info, e.what());
        return GENERAL_FAIL;
    }
    catch (...) {
        reshade::log_message(reshade::log_level::info, "Couldn't initialize weaver");
        return GENERAL_FAIL;
    }

    weaver_initialized = true;

    // Determine the default latency mode for the weaver
    DirectX12Weaver::determine_default_latency_mode();

    return SUCCESS;
}

bool DirectX12Weaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc) {
    reshade::api::resource_desc desc = effect_resource_desc;
    desc.type = reshade::api::resource_type::texture_2d;
    desc.heap = reshade::api::memory_heap::gpu_only;
    desc.usage = reshade::api::resource_usage::copy_dest;

    if (!d3d12_device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::unordered_access),
                                      nullptr, reshade::api::resource_usage::unordered_access, &effect_frame_copy)) {

        effect_frame_copy_x = 0;
        effect_frame_copy_y = 0;

        reshade::log_message(reshade::log_level::warning, "Failed creating the effect frame copy");
        return false;
    }

    effect_frame_copy_x = desc.texture.width;
    effect_frame_copy_y = desc.texture.height;

    return true;
}

GbResult DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    reshade::api::resource_view chosen_rtv;

    if (use_srgb_rtv) {
        chosen_rtv = rtv_srgb;
    }
    else {
        chosen_rtv = rtv;
    }

    reshade::api::resource rtv_resource = d3d12_device->get_resource_from_view(chosen_rtv);
    reshade::api::resource_desc desc = d3d12_device->get_resource_desc(rtv_resource);

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

            // Check buffer format and see if it's in the list of known problematic ones. Change to SRGB rtv if so.
            if ((std::find(srgb_color_formats.begin(), srgb_color_formats.end(), desc.texture.format) != srgb_color_formats.end())) {
                // SRGB format detected, switch to SRGB buffer.
                use_srgb_rtv = true;
            }
            else {
                use_srgb_rtv = false;
            }

            // Todo: Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation succeeds
            // Destroy the resource only when the GPU is finished drawing.
            runtime->get_command_queue()->wait_idle();
            d3d12_device->destroy_resource(effect_frame_copy);
            if (!create_effect_copy_buffer(desc) && !resize_buffer_failed) {
                reshade::log_message(reshade::log_level::info, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }

            // Set newly create buffer as input
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
            reshade::log_message(reshade::log_level::warning, "Buffer size changed");
        }
        else {
            if (weaving_enabled) {
                cmd_list->bind_descriptor_tables(reshade::api::shader_stage::all, reshade::api::pipeline_layout {}, 0, 0, nullptr);
                weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());

                // Create copy of the effect buffer
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_dest);

                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &chosen_rtv);

                // Weave to back buffer
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::unordered_access);

                weaver->weave(desc.texture.width, desc.texture.height);

                // Check if the descriptor heap offset is set. If it is, we have to reset the descriptor heaps to ensure the ReShade overlay can render.
                cmd_list->bind_descriptor_tables(reshade::api::shader_stage::all, reshade::api::pipeline_layout {}, 0, 0, nullptr);
            }
        }
    }
    else {
        // Set current buffer format
        current_buffer_format = desc.texture.format;

        // Set color format settings
        check_color_format(desc);

        create_effect_copy_buffer(desc);
        GbResult result = init_weaver(runtime, effect_frame_copy, d3d12_device->get_resource_from_view(chosen_rtv));
        if (result == SUCCESS) {
            // Set command list and input frame buffer again to make sure they are correct
            weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
        }
        else if (result == DLL_NOT_LOADED) {
            return DLL_NOT_LOADED;
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer after waiting for the GPU to finish.
            runtime->get_command_queue()->wait_idle();
            d3d12_device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return GENERAL_FAIL;
        }
    }
    return SUCCESS;
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12_device = runtime->get_device();
}

void DirectX12Weaver::do_weave(bool do_weave) {
    weaving_enabled = do_weave;
}

bool DirectX12Weaver::set_latency_in_frames(int32_t number_of_frames) {
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

bool DirectX12Weaver::set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) {
    if (weaver_initialized) {
        set_latency_mode(LatencyModes::FRAMERATE_ADAPTIVE);
        weaver->setLatency(frametime_in_microseconds);
        weaver_latency_in_us = frametime_in_microseconds;
        return true;
    }
    return false;
}

void DirectX12Weaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

void DirectX12Weaver::check_color_format(reshade::api::resource_desc desc) {
    // Check buffer format and see if it's in the list of known SRGB ones. Change to SRGB rtv if so.
    if ((std::find(srgb_color_formats.begin(), srgb_color_formats.end(), desc.texture.format) != srgb_color_formats.end())) {
        // SRGB format detected, switch to SRGB buffer.
        use_srgb_rtv = true;
    }
    else {
        use_srgb_rtv = false;
    }
}

LatencyModes DirectX12Weaver::get_latency_mode() {
    return current_latency_mode;
}
