/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "pch.h"

// Directx
#include <DirectX-Headers/include/directx/d3d12.h>
#include <DirectX-Headers/include/directx/d3dx12.h>
#include <combaseapi.h>
#include <DirectXMath.h>

// Weaver
#include "sr/weaver/dx12weaver.h"

// srReshade
#include "igraphicsapi.h"

class DirectX12Weaver: public IGraphicsApi {
    uint32_t lastLatencyFrameTimeSet = DEFAULT_WEAVER_LATENCY;

    bool weaver_initialized = false;
    bool weaving_enabled = false;
    SR::SRContext* sr_context;
    SR::PredictingDX12Weaver* weaver = nullptr;
    reshade::api::device* d3d12_device = nullptr;

    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    size_t descriptor_heap_impl_offset_in_bytes = -1;
    // This must be updated for every new version of ReShade as it can change when the class layout changes!
    // [key] int32_t represents the version number of ReShade without the periods. [value] int32_t represents the offset in bytes inside the class.
    const std::map<int32_t, int32_t> known_descriptor_heap_offsets_by_version = {
            {600, 80},
    };

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;

    std::vector<reshade::api::resource> effect_copy_resources;
    std::vector<Int32XY> effect_copy_resource_res;
    bool effect_copy_resources_initialized = false;

    std::vector<Destroy_Resource_Data> to_destroy;

    LatencyModes current_latency_mode = LatencyModes::framerateAdaptive;
    void set_latency_mode(LatencyModes mode);

public:
    explicit DirectX12Weaver(SR::SRContext* context);
    bool init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);
    bool init_effect_copy_resources(reshade::api::effect_runtime* runtime);
    bool destroy_effect_copy_resources();
    bool create_effect_copy_resource(reshade::api::effect_runtime* runtime, uint32_t back_buffer_index);
    int32_t determine_offset_for_descriptor_heap();

    // Inherited via IGraphicsApi
    void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    void do_weave(bool doWeave) override;
    bool set_latency_in_frames(int32_t numberOfFrames) override;
    bool set_latency_framerate_adaptive(uint32_t frametimeInMicroseconds) override;
    void draw_status_overlay(reshade::api::effect_runtime *runtime) override;
    LatencyModes get_latency_mode() override;
};
