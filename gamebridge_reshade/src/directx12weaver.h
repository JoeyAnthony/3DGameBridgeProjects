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

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;

    std::vector<Int32XY> effect_copy_resource_res;
    reshade::api::resource effect_frame_copy;
    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;

    std::vector<Destroy_Resource_Data> to_destroy;

    LatencyModes current_latency_mode = LatencyModes::framerateAdaptive;
    void set_latency_mode(LatencyModes mode);

public:
    bool resize_buffer_failed = false;
    explicit DirectX12Weaver(SR::SRContext* context);
    bool init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc);

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
