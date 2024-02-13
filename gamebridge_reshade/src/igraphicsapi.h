/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "pch.h"

// List of color formats that require color linearization in order restore the correct gamma.
const std::list<reshade::api::format> problematic_color_formats {
    reshade::api::format::r8g8b8a8_unorm_srgb,
};

const uint32_t DEFAULT_WEAVER_LATENCY = 40000;
// framerateAdaptive = provide an amount of time in microseconds in between weave() calls.
// latencyInFrames = provide an amount of buffers or "frames" between the application and presenting to the screen.
// latencyInFramesAutomatic = gets the amount of buffers or "frames" from the backbuffer using the ReShade api every frame.
enum LatencyModes { framerateAdaptive, latencyInFrames, latencyInFramesAutomatic };

struct Int32XY
{
    uint32_t x = 0;
    uint32_t y = 0;
};

struct Destroy_Resource_Data
{
    reshade::api::resource resource;
    uint32_t frames_alive = 0;
};

class IGraphicsApi {
public:
    int32_t reshade_version_nr_major = 0;
    int32_t reshade_version_nr_minor = 0;
    int32_t reshade_version_nr_patch = 0;

    int32_t get_concatinated_reshade_version();

    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_status_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) = 0;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) = 0;
    virtual void do_weave(bool doWeave) = 0;
    virtual ~IGraphicsApi() = default;

    virtual LatencyModes get_latency_mode() = 0;

    // The two methods below return true if the latency was succesfully set, they return false if their current latency mode does not permit them to set the latency.

    // This method should only be called once, after which it will use the maximum framerate of the monitor in use to determine the latency of the eye tracker.
    // This does NOT look at the current framerate, use this when you are able to consistently reach your monitor's maximum refresh rate.
    // The numberOfFrames setting is overwritten when the latency mode is changed to latencyInFramesAutomatic.
    // Sets the latency mode to latencyInFramesAutomatic if numberOfFrames is negative.
    virtual bool set_latency_in_frames(int32_t numberOfFrames) = 0;

    // This method must be called every frame with the current frametime.
    virtual bool set_latency_framerate_adaptive(uint32_t frametimeInMicroseconds) = 0;
};
