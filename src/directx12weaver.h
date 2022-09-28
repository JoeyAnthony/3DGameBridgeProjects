#pragma once
#include "pch.h"

// Directx
#include <d3d12.h>
#include <d3dx12.h>
#include <combaseapi.h>
#include <DirectXMath.h>

// Weaver
#include "sr/weaver/dx12weaver.h"

// srReshade
#include "igraphicsapi.h"

class MyEyes;

class DirectX12Weaver: public IGraphicsApi {

    MyEyes* eyes = nullptr;
    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;
    reshade::api::resource effect_frame_copy;

public:
    void init_sr_context(reshade::api::effect_runtime* runtime);
    void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);

    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
};