// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <d3d12.h>
#include <combaseapi.h>
#include "sr/weaver/dx12weaver.h"
#include "sr/types.h"

SR::PredictingDX12Weaver* weaver;
SR::SRContext* srContext;

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
    // ...
}

bool g_popup_window_visible = false;
float view_separation = 0.f;
float vertical_shift = 0.f;

static void draw_debug_overlay(reshade::api::effect_runtime* runtime)
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

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Popup window is visible settings", &g_popup_window_visible);
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* command_list, reshade::api::resource_view rtv, reshade::api::resource_view rv) {
    //reshade::api::device* const device = runtime->get_device();

    //weaver->setInputFrameBuffer((ID3D12Resource*)device->get_resource_from_view(rtv).handle);
    //weaver->setCommandList((ID3D12GraphicsCommandList*)command_list->get_native());
    //weaver->weave(1920, 1080);
}

static void on_init(reshade::api::swapchain* swapchain) {
	reshade::api::device* const device = swapchain->get_device();

	//ID3D12CommandAllocator* CommandAllocator;
    //((ID3D12Device*)device->get_native())->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

	// Describe and create the command queue.
	ID3D12CommandQueue* CommandQueue;
	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	((ID3D12Device*)device->get_native())->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));

	if (CommandQueue == nullptr)
	{
		return;
	}

    if (!srContext) {
        srContext = new SR::SRContext;
        //weaver = new SR::PredictingDX12Weaver(*srContext, ((ID3D12Device*)device->get_native()), CommandAllocator, CommandQueue, nullptr, (ID3D12Resource*)(swapchain->get_current_back_buffer().handle));
        srContext->initialize();
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Call 'reshade::register_addon()' before you call any other function of the ReShade API.
        // This will look for the ReShade instance in the current process and initialize the API when found.
        if (!reshade::register_addon(hModule))
            return FALSE;
        // This registers a callback for the 'present' event, which occurs every time a new frame is presented to the screen.
        // The function signature has to match the type defined by 'reshade::addon_event_traits<reshade::addon_event::present>::decl'.
        // For more details check the inline documentation for each event in 'reshade_events.hpp'.
        reshade::register_event<reshade::addon_event::present>(&on_present);

        // reshade::register_overlay("Test", &draw_debug_overlay);
        reshade::register_overlay(nullptr, &draw_sr_settings_overlay);

        reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
        reshade::register_event<reshade::addon_event::init_swapchain>(on_init);

        break;
    case DLL_PROCESS_DETACH:
        // Optionally unregister the event callback that was previously registered during process attachment again.
        reshade::unregister_event<reshade::addon_event::present>(&on_present);
        // And finally unregister the add-on from ReShade (this will automatically unregister any events and overlays registered by this add-on too).
        reshade::unregister_addon(hModule);
        break;
    }
    return TRUE;
}
