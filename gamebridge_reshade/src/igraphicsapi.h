#pragma once
#include "pch.h"

const uint32_t DEFAULT_WEAVER_LATENCY = 40000;
// framerateAdaptive = provide an amount of time in microseconds in between weave() calls.
// latencyInFrames = provide an amount of buffers or "frames" between the application and presenting to the screen.
// latencyInFramesAutomatic = gets the amount of buffers or "frames" from the backbuffer using the ReShade api every frame.
enum LatencyModes { framerateAdaptive, latencyInFrames, latencyInFramesAutomatic };

class IGraphicsApi {
public:
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_status_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) = 0;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) = 0;
    virtual void do_weave(bool doWeave) = 0;
    virtual ~IGraphicsApi() = default;

    virtual void on_destroy_swapchain(reshade::api::swapchain *swapchain) {};

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
