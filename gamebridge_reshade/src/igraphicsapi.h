#pragma once
#include "pch.h"

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
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) = 0;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) = 0;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) = 0;
    virtual void do_weave(bool doWeave) = 0;
    virtual ~IGraphicsApi() = default;

    // Only used for dx9
    virtual void on_destroy_swapchain(reshade::api::swapchain *swapchain) {};
};
