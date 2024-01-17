#pragma once
#include "pch.h"

// Directx
#include <DirectX-Headers/include/directx/d3d12.h>
#include <DirectX-Headers/include/directx/d3dx12.h>
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
    bool weaver_initialized = false;
    bool weaving_enabled = false;
    SR::SRContext* srContext;
    SR::PredictingDX12Weaver* weaver = nullptr;
    reshade::api::device* d3d12device = nullptr;

    bool g_popup_window_visible = false;
    float view_separation = 0.f;
    float vertical_shift = 0.f;

    // This must be updated for every new version of ReShade as it can change when the class layout changes.
    int descriptor_heap_impl_offset_in_bytes = 0;
    std::string descriptor_heap_impl_offset_target_reshade_version = "6.0.0";

    reshade::api::command_list* command_list;
    reshade::api::resource_view game_frame_buffer;

    std::vector<reshade::api::resource> effect_copy_resources;
    std::vector<Int32XY> effect_copy_resource_res;
    bool effect_copy_resources_initialized = false;

    std::vector<Destroy_Resource_Data> to_destroy;

public:
    DirectX12Weaver(SR::SRContext* context);
    bool init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer);
    bool init_effect_copy_resources(reshade::api::effect_runtime* runtime);
    bool destroy_effect_copy_resources();
    bool create_effect_copy_resource(reshade::api::effect_runtime* runtime, uint32_t back_buffer_index);

    // Inherited via IGraphicsApi
    virtual void draw_debug_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void draw_settings_overlay(reshade::api::effect_runtime* runtime) override;
    virtual void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) override;
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    virtual void do_weave(bool doWeave) override;
};
