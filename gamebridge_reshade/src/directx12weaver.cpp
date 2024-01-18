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

bool DirectX12Weaver::init_effect_copy_resources(reshade::api::effect_runtime* runtime) {
    // Only initialize resources once
    if (effect_copy_resources_initialized) {
        return true;
    }

    uint32_t num_buffers = runtime->get_back_buffer_count();

    // Init copy resource vectors
    effect_copy_resources = std::vector<reshade::api::resource>(num_buffers);
    effect_copy_resource_res = std::vector<Int32XY>(num_buffers);

    // Create copy resource for every back buffer
    for (uint32_t i = 0; i < num_buffers; i++) {
        if (!create_effect_copy_resource(runtime, i)) {
            // Destroy on failure
            destroy_effect_copy_resources();

            log_message(reshade::log_level::info, "Failed initialing frame copy resources");
            return false;
        }
    }

    effect_copy_resources_initialized = true;
    return true;
}

bool DirectX12Weaver::create_effect_copy_resource(reshade::api::effect_runtime* runtime, uint32_t back_buffer_index) {
    reshade::api::resource_desc back_buffer_desc(d3d12device->get_resource_desc(runtime->get_back_buffer(back_buffer_index)));
    reshade::api::resource_desc copy_resource_description(
        reshade::api::resource_type::texture_2d,
        back_buffer_desc.texture.width,
        back_buffer_desc.texture.height,
        back_buffer_desc.texture.depth_or_layers,
        back_buffer_desc.texture.levels,
        back_buffer_desc.texture.format,
        1,
        reshade::api::memory_heap::gpu_only,
        reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::unordered_access
    );

    if (!d3d12device->create_resource(copy_resource_description, nullptr, reshade::api::resource_usage::unordered_access, &effect_copy_resources[back_buffer_index])) {

        log_message(reshade::log_level::info, "Failed to initialize copy resource");

        effect_copy_resource_res[back_buffer_index].x = 0;
        effect_copy_resource_res[back_buffer_index].y = 0;

        return false;
    }

    effect_copy_resource_res[back_buffer_index].x = back_buffer_desc.texture.width;
    effect_copy_resource_res[back_buffer_index].y = back_buffer_desc.texture.height;

    return true;
}

bool DirectX12Weaver::destroy_effect_copy_resources()
{
    for (uint32_t i = 0; i < effect_copy_resources.size(); i++) {
        d3d12device->destroy_resource(effect_copy_resources[i]);

        effect_copy_resource_res[i].x = 0;
        effect_copy_resource_res[i].y = 0;
    }

    effect_copy_resources_initialized = false;
    return true;
}

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);

    uint32_t back_buffer_index = runtime->get_current_back_buffer_index();

    if (weaver_initialized) {
        // Check texture size
        if (desc.texture.width != effect_copy_resource_res[back_buffer_index].x || desc.texture.height != effect_copy_resource_res[back_buffer_index].y) {
            //TODO Might have to get the buffer from the create_effect_copy_buffer function and only swap them when creation succeeds

            // Copy to destroy list
            Destroy_Resource_Data type;
            type.frames_alive = 0;
            type.resource = effect_copy_resources[back_buffer_index];
            to_destroy.push_back(type);

            reshade::log_message(reshade::log_level::info, "Resource marked for destroy");

            if (!create_effect_copy_resource(runtime, back_buffer_index) /** && !resize_buffer_failed **/) {
                reshade::log_message(reshade::log_level::warning, "Couldn't create effect copy buffer, trying again next frame");
            }

            reshade::log_message(reshade::log_level::info, "Buffer size changed");
        }
        else {
            if (weaving_enabled) {
                // Create copy of the effect buffer
                cmd_list->barrier(effect_copy_resources[back_buffer_index], reshade::api::resource_usage::unordered_access, reshade::api::resource_usage::copy_dest);

                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_copy_resources[back_buffer_index]);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

                // Bind back buffer as render target
                cmd_list->bind_render_targets_and_depth_stencil(1, &rtv);

                // Weave to back buffer
                weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
                cmd_list->barrier(effect_copy_resources[back_buffer_index], reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::unordered_access);
                weaver->setInputFrameBuffer((ID3D12Resource*)effect_copy_resources[back_buffer_index].handle);
                weaver->weave(desc.texture.width, desc.texture.height);

            }
        }
    }
    else {
        if(!init_effect_copy_resources(runtime))
        {
            // Initialization of resources failed.
            return;
        }

        if (init_weaver(runtime, effect_copy_resources[0], d3d12device->get_resource_from_view(rtv))) {
            //Set command list and input frame buffer again to make sure they are correct
            weaver->setCommandList((ID3D12GraphicsCommandList*)cmd_list->get_native());
            weaver->setInputFrameBuffer((ID3D12Resource*)effect_copy_resources[back_buffer_index].handle);
        }
        else {
            // When buffer creation succeeds and this fails, delete the created buffer
            destroy_effect_copy_resources();
            reshade::log_message(reshade::log_level::info, "Failed to initialize weaver");
            return;
        }
    }

    // Destroy resources
    for(auto it = to_destroy.begin(); it != to_destroy.end();)
    {
        it->frames_alive++;
        if (it->frames_alive > 5)
        {
            d3d12device->destroy_resource(it->resource);
            it = to_destroy.erase(it);
            reshade::log_message(reshade::log_level::info, "Resource destroyed");
            continue;
        }

        ++it;
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
}

void DirectX12Weaver::do_weave(bool doWeave)
{
    weaving_enabled = doWeave;
}
