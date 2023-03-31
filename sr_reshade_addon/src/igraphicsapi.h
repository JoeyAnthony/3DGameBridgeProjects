#pragma once
#include "pch.h"


enum LatencyModes { framerateAdaptive, latencyInFrames };

class IGraphicsApi {
public:
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) = 0;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) = 0;
    virtual void do_weave(bool doWeave) = 0;

    //Latency settings
    // The latency mode must be set before attempting to set the latency with "set_latency_in_frames" or "set_latency_framerate_adaptive".
    virtual void set_latency_mode(LatencyModes mode) = 0;
    virtual LatencyModes get_latency_mode() = 0;

    // The two methods below return true if the latency was succesfully set, they return false if their current latency mode does not permit them to set the latency.

    // This method should only be called once, after which it will use the maximum framerate of the monitor in use to determine the latency of the eye tracker. 
    // This does NOT look at the current framerate, use this when you are able to consistently reach your monitor's maximum refresh rate.
    virtual bool set_latency_in_frames(int numberOfFrames) = 0;

    // This method must be called every frame with the current frametime.
    virtual bool set_latency_framerate_adaptive(int frametimeInMicroseconds) = 0;
};
