#include "pch.h"
#include "igraphicsapi.h"
#include "directx11weaver.h"
#include "directx12weaver.h"
#include "hotkeymanager.h"

#include <vector>
#include <iostream>

IGraphicsApi* weaverImplementation = nullptr;
SR::SRContext* srContext = nullptr;
HotKeyManager* hotKeyManager = nullptr;

static void draw_debug_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_debug_overlay(runtime);
}

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_sr_settings_overlay(runtime);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_settings_overlay(runtime);
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    weaverImplementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static void on_reshade_present(reshade::api::effect_runtime* runtime) {
    std::map<shortcutType, bool> hotKeyList;

    //Check if certain hotkeys are being pressed
    if (hotKeyManager != nullptr) {
        //Find out which hotkeys have changed their toggled state, then execute their respective code.
        hotKeyList = hotKeyManager->checkHotKeys(runtime, srContext);
        executeHotKeyFunctionByType(hotKeyList);
    }
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    //First, construct the SR context
    if (srContext == nullptr) {
        srContext = new SR::SRContext;
        srContext->initialize();
    }

    //Todo: Move these hard-coded hotkeys to user-definable hotkeys in the .ini file
    //Register some standard hotkeys
    if (hotKeyManager == nullptr) {
        hotKeyManager = new HotKeyManager();
    }

    //Then, check the active graphics API and pass it a new context.
    if (weaverImplementation == nullptr) {
        switch (runtime->get_device()->get_api()) {
        case reshade::api::device_api::d3d11:
            weaverImplementation = new DirectX11Weaver(srContext);
            break;
        case reshade::api::device_api::d3d12:
            weaverImplementation = new DirectX12Weaver(srContext);
            break;
        default:
            //Games will be DX11 in the majority of cases.
            //Todo: This may still crash our code so we should leave the API switching to user input if we cannot detect it ourselves.
            reshade::log_message(3, "Unable to determine graphics API, attempting to switch to DX11...");
            weaverImplementation = new DirectX11Weaver(srContext);
            break;
        }
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
        reshade::register_event<reshade::addon_event::reshade_present>(&on_reshade_present);

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

void executeHotKeyFunctionByType(std::map<shortcutType, bool> hotKeyList) {
    std::map<shortcutType, bool>::iterator i;
    for (i = hotKeyList.begin(); i != hotKeyList.end(); i++){
        switch (i->first) {
        case shortcutType::toggleSR:
            if (i->second) {
                srContext = new SR::SRContext;
                srContext->initialize();
                weaverImplementation->set_context_validity(true);
            }
            else {
                srContext->deleteSRContext(srContext);
                srContext->~SRContext();
                weaverImplementation->set_context_validity(false);
            }
            break;
        case shortcutType::toggleLens:
            //Todo: Implement lens toggling mechanism.
            break;
        case shortcutType::flattenDepthMap:
            //Todo: Implement depth map flattening mechanism.
            break;
        default:
            break;
        }
    }
}
