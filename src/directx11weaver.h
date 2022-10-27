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

// My SR::EyePairListener that stores the last tracked eye positions
//class MyEyes : public SR::EyePairListener {
//private:
//    SR::InputStream<SR::EyePairStream> stream;
//public:
//    DirectX::XMFLOAT3 left, right;
//    MyEyes(SR::EyeTracker* tracker) : left(-30, 0, 600), right(30, 0, 600) {
//        // Open a stream between tracker and this class
//        stream.set(tracker->openEyePairStream(this));
//    }
//    // Called by the tracker for each tracked eye pair
//    virtual void accept(const SR_eyePair& eyePair) override
//    {
//        // Remember the eye positions
//        left = DirectX::XMFLOAT3(eyePair.left.x, eyePair.left.y, eyePair.left.z);
//        right = DirectX::XMFLOAT3(eyePair.right.x, eyePair.right.y, eyePair.right.z);
//
//        std::stringstream ss;
//        ss << "left: " << left.x << " right: " << right.x;
//        reshade::log_message(3, ss.str().c_str());
//    }
//};

class DirectX11Weaver: public IGraphicsApi {

    bool srContextInitialized = false;
    bool weaverInitialized = false;
    SR::PredictingDX11Weaver* weaver = nullptr;
    SR::SRContext* srContext = nullptr;
    reshade::api::device* d3d11device = nullptr;
    //MyEyes* eyes = nullptr;

    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;
    reshade::api::resource effect_frame_copy;

public:
    DirectX11Weaver(SR::SRContext* context);
    void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list);

    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;

    // Inherited via IGraphicsApi
    virtual bool is_initialized() override;
    virtual void set_context_validity(bool isValid) override;
    virtual void set_weaver_validity(bool isValid) override;
};