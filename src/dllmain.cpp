#include "pch.h"
#include "igraphicsapi.h"
#include "directx11weaver.h"
#include "directx12weaver.h"

IGraphicsApi* weaverImplementation = nullptr;

static void draw_debug_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_debug_overlay(runtime);
}

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_sr_settings_overlay(runtime);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_settings_overlay(runtime);
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb){
    weaverImplementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    if (weaverImplementation == nullptr) {
        //weaverImplementation = new DirectX12Weaver;
        weaverImplementation = new DirectX11Weaver;
    }

    weaverImplementation->on_init_effect_runtime(runtime);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

        if (!reshade::register_addon(hModule))
            return FALSE;

        reshade::register_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);

        //reshade::register_overlay("Test", &draw_debug_overlay);
        //reshade::register_overlay(nullptr, &draw_sr_settings_overlay);
        //reshade::log_message(3, "registered: draw_sr_settings_overlay");

        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hModule);
        //reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
        //reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
        break;
    }
    return TRUE;
}
