/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

// Weaver
#include "sr/weaver/dx12weaver.h"
// srReshade
#include "igraphicsapi.h"
#include "pch.h"

// Directx
#include <DirectXMath.h>

class DirectX12Weaver: public IGraphicsApi {
    uint32_t last_latency_frame_time_set = g_default_weaver_latency;
    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;

    bool weaver_initialized = false;
    bool weaving_enabled = false;
    bool popup_window_visible = false;
    bool resize_buffer_failed = false;
    bool use_srgb_rtv = false;

    float view_separation = 0.f;
    float vertical_shift = 0.f;

    SR::SRContext* sr_context;
    SR::PredictingDX12Weaver* weaver = nullptr;

    reshade::api::device* d3d12_device = nullptr;
    reshade::api::command_list* command_list{};
    reshade::api::resource effect_frame_copy{};
    reshade::api::resource_view game_frame_buffer{};
    reshade::api::format current_buffer_format = reshade::api::format::unknown;

    LatencyModes current_latency_mode = LatencyModes::FRAMERATE_ADAPTIVE;

    /// \brief Private function to set the internal latency mode of the weaver
    /// \param mode The latency mode to persist
    void set_latency_mode(LatencyModes mode);

    /// \brief Checks the current color format of the RTV's back buffer and chooses the correct buffer from ReShade for weaving based on that.
    /// \param desc Represents the current ReShade resource view
    void DirectX12Weaver::check_color_format(reshade::api::resource_desc desc);

public:
    /// \brief Explicit constructor
    /// \param context Pointer to an already initialized SRContext
    explicit DirectX12Weaver(SR::SRContext* context);

    /// \brief Initialized the SR weaver appropriate for the graphics API
    /// \param runtime Represents the reshade effect runtime
    /// \param rtv Represents the buffer that the weaver uses as a source to weave with
    /// \param back_buffer Represents the current back buffer from ReShade, this is used by the weaver as output location
    /// \return A bool representing if the weaver was initialized successfully
    bool init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);

    /// \brief Method that creates a copy of the RTV so we can weave on it
    /// \param effect_resource_desc ReShade resource representing the currently selected RTV
    /// \return A bool respresenting if the effect frame copy was successful
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc);

    // Inherited via IGraphicsApi
    void draw_status_overlay(reshade::api::effect_runtime *runtime) override;
    void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    void do_weave(bool do_weave) override;
    bool set_latency_in_frames(int32_t number_of_frames) override;
    bool set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) override;
    LatencyModes get_latency_mode() override;
};
