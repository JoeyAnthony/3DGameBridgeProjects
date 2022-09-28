#include "directx11weaver.h"

// My SR::EyePairListener that stores the last tracked eye positions
class MyEyes : public SR::EyePairListener {
private:
    SR::InputStream<SR::EyePairStream> stream;
public:
    DirectX::XMFLOAT3 left, right;
    MyEyes(SR::EyeTracker* tracker) : left(-30, 0, 600), right(30, 0, 600) {
        // Open a stream between tracker and this class
        stream.set(tracker->openEyePairStream(this));
    }
    // Called by the tracker for each tracked eye pair
    virtual void accept(const SR_eyePair& eyePair) override
    {
        // Remember the eye positions
        left = DirectX::XMFLOAT3(eyePair.left.x, eyePair.left.y, eyePair.left.z);
        right = DirectX::XMFLOAT3(eyePair.right.x, eyePair.right.y, eyePair.right.z);

        std::stringstream ss;
        ss << "left: " << left.x << " right: " << right.x;
        reshade::log_message(3, ss.str().c_str());
    }
};

void DirectX11Weaver::init_sr_context(reshade::api::effect_runtime* runtime) {
    // Just in case it's being called multiple times
    if (!srContextInitialized) {
        srContext = new SR::SRContext;
        eyes = new MyEyes(SR::EyeTracker::create(*srContext));
        srContext->initialize();
        srContextInitialized = true;
    }
}

void DirectX11Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::command_list* cmd_list) {
    if (weaverInitialized) {
        return;
    }

    reshade::api::resource_desc desc = d3d11device->get_resource_desc(rtv);

    if (d3d11device) {
        reshade::log_message(3, "Can cast a device! to native Dx12 Device!!!!");
    }
    else {
        reshade::log_message(3, "Could not cast device!!!!");
        return;
    }

    uint64_t back_buffer_index = runtime->get_current_back_buffer_index();
    char buffer[1000];
    sprintf(buffer, "backbuffer num: %l", back_buffer_index);
    reshade::api::resource res = runtime->get_current_back_buffer();
    uint64_t handle = res.handle;

    sprintf(buffer, "resource value: %l", handle);
    reshade::log_message(4, buffer);

    ID3D11Resource* native_frame_buffer = (ID3D11Resource*)rtv.handle;

    if (runtime->get_current_back_buffer().handle == (uint64_t)INVALID_HANDLE_VALUE) {
        reshade::log_message(3, "backbuffer handle is invalid");
    }

    try {
        weaver = new SR::PredictingDX11Weaver(*srContext, (ID3D11Device*)d3d11device, (ID3D11DeviceContext*)cmd_list->get_native(), desc.texture.width, desc.texture.height, (HWND)runtime->get_hwnd());
        weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)rtv.handle); //resourceview of the buffer
        srContext->initialize();
        reshade::log_message(3, "Initialized weaver");
    }
    catch (std::exception e) {
        reshade::log_message(3, e.what());
    }
    catch (...) {
        reshade::log_message(3, "Couldn't initialize weaver");
    }

    weaverInitialized = true;
}

void DirectX11Weaver::draw_debug_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::TextUnformatted("Some text");

    if (ImGui::Button("Press me to open an additional popup window"))
        g_popup_window_visible = true;

    if (g_popup_window_visible)
    {
        ImGui::Begin("Popup", &g_popup_window_visible);
        ImGui::TextUnformatted("Some other text");
        ImGui::End();
    }
}

void DirectX11Weaver::draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

void DirectX11Weaver::draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

static reshade::api::command_list* command_list;
static reshade::api::resource_view game_frame_buffer;
static reshade::api::resource effect_frame_copy;
void DirectX11Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    reshade::api::resource rtv_resource = d3d11device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = d3d11device->get_resource_desc(rtv_resource);

    if (!weaverInitialized) {
        reshade::log_message(3, "init effect buffer copy");
        desc.type = reshade::api::resource_type::texture_2d;
        desc.heap = reshade::api::memory_heap::gpu_only;
        desc.usage = reshade::api::resource_usage::copy_dest;

        if (d3d11device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest),
            nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
            reshade::log_message(3, "Created resource");
        }
        else {
            reshade::log_message(3, "Failed creating resource");
            return;
        }

        init_weaver(runtime, effect_frame_copy, cmd_list);
    }

    if (weaverInitialized) {
        reshade::api::resource_view view;
        d3d11device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d11device->get_resource_view_desc(rtv), &view);

        // Copy resource
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(rtv_resource, effect_frame_copy);
        cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        //const float color[] = { 1.f, 0.3f, 0.5f, 1.f };
        //cmd_list->clear_render_target_view(view, color);
        cmd_list->bind_render_targets_and_depth_stencil(1, &view);

        // TODO probably doesn't work
        weaver->setInputFrameBuffer((ID3D11ShaderResourceView*)effect_frame_copy.handle);
        ID3D11DeviceContext * native_cmd_list = (ID3D11DeviceContext*)cmd_list->get_native();
        weaver->setContext(native_cmd_list);
        weaver->weave(desc.texture.width, desc.texture.height);
    }
}

void DirectX11Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d11device = runtime->get_device();
    init_sr_context(runtime);
}
