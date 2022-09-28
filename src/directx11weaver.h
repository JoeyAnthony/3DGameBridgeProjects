#pragma once
#include "pch.h"

// Third party
#include <imgui.h>
#include <reshade.hpp>

// Directx
#include <d3d11.h>
#include <DirectXMath.h>

// Weaver
#include "sr/weaver/dx11weaver.h"

// srReshade
#include "igraphicsapi.h"

class MyEyes;

class DirectX11Weaver: public IGraphicsApi {

    bool srContextInitialized = false;
    bool weaverInitialized = false;
    SR::PredictingDX11Weaver* weaver = nullptr;
    SR::SRContext* srContext = nullptr;
    reshade::api::device* d3d11device = nullptr;
    MyEyes* eyes = nullptr;

    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

public:
    void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list);
    void init_sr_context(reshade::api::effect_runtime* runtime);

    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
};