#pragma once
#include "pch.h"

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
    bool weaver_initialized = false;
    bool weaving_enabled = false;
    SR::SRContext* srContext;
    SR::PredictingDX11Weaver* weaver = nullptr;
    reshade::api::device* d3d11device = nullptr;

    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;
    reshade::api::resource effect_frame_copy;
    reshade::api::resource_view effect_frame_copy_srv;
    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;
    bool resize_buffer_failed = false;

    LatencyModes current_latency_mode = LatencyModes::framerateAdaptive;
    std::chrono::high_resolution_clock::time_point clock_last_frame;
    static const int frame_time_list_size = 100;
    long long frame_time_list[frame_time_list_size] = { 1 / 60 * 1000 }; // Initialize frame time list with default of 16.66 ms (Average frame time for 60Hz)
    short frame_time_index = 0;

public:
    explicit DirectX11Weaver(SR::SRContext* context);
    bool init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list);
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc);


    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    virtual void do_weave(bool doWeave) override;
    virtual bool set_latency_in_frames(int numberOfFrames) override;
    virtual bool set_latency_framerate_adaptive(int frametimeInMicroseconds) override;
    virtual void set_latency_mode(LatencyModes mode) override;
    virtual LatencyModes get_latency_mode() override;
};
