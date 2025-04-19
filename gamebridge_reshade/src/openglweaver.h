/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

// Weaver
#include "sr/weaver/glweaver.h"
// srReshade
#include "igraphicsapi.h"
#include "pch.h"

class OpenGLWeaver: public IGraphicsApi {
    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;

    bool weaving_enabled = false;
    bool popup_window_visible = false;
    bool resize_buffer_failed = false;
    bool use_srgb_rtv = false;

    float view_separation = 0.f;
    float vertical_shift = 0.f;

    SR::SRContext* sr_context;
    SR::PredictingGLWeaver* weaver = nullptr;

    reshade::api::device* gl_device = nullptr;
    reshade::api::command_list* command_list{};
    reshade::api::resource effect_frame_copy{};
    reshade::api::resource effect_frame_copy_flipped{};
    reshade::api::resource_view effect_frame_copy_srv{};
    reshade::api::resource_view effect_frame_copy_flipped_rtv{};
    reshade::api::format current_buffer_format = reshade::api::format::unknown;

    LatencyModes current_latency_mode = LatencyModes::FRAMERATE_ADAPTIVE;

    /// \brief Private function to set the internal latency mode of the weaver
    /// \param mode The latency mode to persist
    void set_latency_mode(LatencyModes mode);

    /// \brief Checks the current rtv buffer color format and chooses the correct rtv for weaving based on that.
    /// \param desc Represents the current ReShade resource view
    void check_color_format(reshade::api::resource_desc desc);

public:
    /// \brief Explicit constructor
    /// \param context Pointer to an already initialized SRContext
    explicit OpenGLWeaver(SR::SRContext* context);

    /// \brief Initialized the SR weaver appropriate for the graphics API
    /// \param runtime Represents the reshade effect runtime
    /// \param rtv Represents the current render target view
    /// \param cmd_list Represents the current command list from ReShade
    /// \return An enum representing the return code
    GbResult init_weaver(reshade::api::effect_runtime *runtime, reshade::api::resource rtv,
                         reshade::api::command_list *cmd_list);

    /// \brief Creates and reset the effect copy resource so it is similar to the back buffer resource, then use it as weaver input.
    /// \param effect_resource_desc ReShade resource representing the currently selected back buffer description
    /// \return A bool respresenting if the effect frame copy was successful
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc, reshade::api::command_list* cmd_list);

    /// \brief Takes a source buffer and copies it vertically flipped into the destination buffer with a given width and height. Both the `source` and `dest` buffers must be the same size.
    /// \param buffer_height Represents the height of the buffer to copy
    /// \param buffer_width Represents the width of the buffer to copy
    /// \param cmd_list The active command list to append the copy command to
    /// \param source The source buffer from which to copy data
    /// \param dest The destination buffer to copy data to
    static void flip_buffer(int buffer_height, int buffer_width, reshade::api::command_list *cmd_list, reshade::api::resource source, reshade::api::resource dest);

    /// \brief Destroys all resources and resource views managed by this addon.
    void destroy_all_resources_and_resource_views();

    // Inherited via IGraphicsApi
    GbResult on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) override;
    void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    void do_weave(bool do_weave) override;
    bool set_latency_in_frames(int32_t number_of_frames) override;
    bool set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) override;
    LatencyModes get_latency_mode() override;
};
