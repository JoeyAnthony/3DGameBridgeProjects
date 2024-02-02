/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "directx12weaver.h"
#include <sstream>

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context)
{
    //Set context here.
    sr_context = context;
    weaving_enabled = true;
}

bool DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaver_initialized) {
        return weaver_initialized;
    }

    // See if we can get a command allocator from reshade
    ID3D12Device* dev = ((ID3D12Device*)d3d12_device->get_native());
    if (!dev) {
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
    try {
        weaver = new SR::PredictingDX12Weaver(*sr_context, dev, CommandAllocator, CommandQueue, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        sr_context->initialize();
        reshade::log_message(reshade::log_level::info, "Initialized weaver");

        // Set mode to latency in frames by default.
        set_latency_mode(LatencyModes::framerateAdaptive);
        set_latency_framerate_adaptive(DEFAULT_WEAVER_LATENCY);
        std::string latencyLog = "Current latency mode set to: STATIC " + std::to_string(DEFAULT_WEAVER_LATENCY) + " Microseconds";
        reshade::log_message(reshade::log_level::info, latencyLog.c_str());
    }
    catch (std::exception& e) {
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

void DirectX12Weaver::draw_status_overlay(reshade::api::effect_runtime *runtime) {
    // Log activity status
    ImGui::TextUnformatted("Status: ACTIVE");

    // Log the latency mode
    std::string latencyModeDisplay = "Latency mode: ";
    if(current_latency_mode == LatencyModes::framerateAdaptive) {
        latencyModeDisplay += "IN " + std::to_string(lastLatencyFrameTimeSet) + " MICROSECONDS";
    } else {
        latencyModeDisplay += "IN " + std::to_string(runtime->get_back_buffer_count()) + " FRAMES";
    }
    ImGui::TextUnformatted(latencyModeDisplay.c_str());
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

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12_device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d12_device->get_resource_desc(rtv_resource);

    if (weaver_initialized) {
        // Check if we need to set the latency in frames.
        if(get_latency_mode() == LatencyModes::latencyInFramesAutomatic) {
            weaver->setLatencyInFrames(runtime->get_back_buffer_count()); // Set the latency with which the weaver should do prediction.
        }

        // Check texture size
        if (desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y) {
            // TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation succeeds
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
                weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());

                // Create copy of the effect buffer
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_dest);

                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &rtv);

                // Weave to back buffer
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::unordered_access);
                weaver->weave(desc.texture.width, desc.texture.height);

                // Check if the descriptor heap offset is set. If it is, we have to reset the descriptor heaps to ensure the ReShade overlay can render.
                cmd_list->bind_descriptor_tables(reshade::api::shader_stage::all, reshade::api::pipeline_layout {}, 0, 0, nullptr);
            }
        }
    }
    else {
        create_effect_copy_buffer(desc);
        if (init_weaver(runtime, effect_frame_copy, d3d12_device->get_resource_from_view(rtv))) {
            //Set command list and input frame buffer again to make sure they are correct
            weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer after waiting for the GPU to finish.
            runtime->get_command_queue()->wait_idle();
            d3d12_device->destroy_resource(effect_frame_copy);
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return;
        }
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12_device = runtime->get_device();
}

void DirectX12Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}

/**
 * Find the closest element to a given value in a sorted array.
 *
 * @param sorted_array A sorted vector of integers.
 * @param target_value The value to find the closest element to.
 * @return The closest element in the array to the target value.
 * @throws std::invalid_argument if the array is empty.
 */
int32_t find_closest(const std::vector<int32_t>& sorted_array, int target_value) {
    // Check if the array is empty
    if (sorted_array.empty()) {
        throw std::invalid_argument("Unable to find closest, array is empty.");
    }

    // Use binary search to find the lower bound of the target value
    const auto lower_bound = std::lower_bound(sorted_array.begin(), sorted_array.end(), target_value);

    // Initialize the answer with the closest value found so far
    int32_t closest_value = (lower_bound != sorted_array.end()) ? *lower_bound : sorted_array.back();

    // Check if there is a predecessor to the lower bound
    if (lower_bound != sorted_array.begin()) {
        auto predecessor = lower_bound - 1;

        // Update the answer if the predecessor is closer to the target value
        if (std::abs(closest_value - target_value) > std::abs(*predecessor - target_value)) {
            closest_value = *predecessor;
        }
    }

    // Return the closest value found
    return closest_value;
}

bool DirectX12Weaver::set_latency_in_frames(int32_t numberOfFrames) {
    if (weaver_initialized) {
        if (numberOfFrames < 0) {
            set_latency_mode(LatencyModes::latencyInFramesAutomatic);
        } else {
            set_latency_mode(LatencyModes::latencyInFrames);
            weaver->setLatencyInFrames(numberOfFrames);
        }
        return true;
    }
    return false;
}

bool DirectX12Weaver::set_latency_framerate_adaptive(uint32_t frametimeInMicroseconds) {
    if (weaver_initialized) {
        set_latency_mode(LatencyModes::framerateAdaptive);
        weaver->setLatency(frametimeInMicroseconds);
        lastLatencyFrameTimeSet = frametimeInMicroseconds;
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
