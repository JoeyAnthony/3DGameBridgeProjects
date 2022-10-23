#pragma once
#include "pch.h"

// Directx
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <combaseapi.h>
#include <DirectXMath.h>

// Weaver
#include "sr/weaver/dx12weaver.h"

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

class DirectX12Weaver: public IGraphicsApi {

    //MyEyes* eyes = nullptr;
    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;
    reshade::api::resource effect_frame_copy;
    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;

public:
    void init_sr_context(reshade::api::effect_runtime* runtime);
    void init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc);

    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;

    // Inherited via IGraphicsApi
    virtual bool is_initialized() override;
};