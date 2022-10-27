#include "pch.h"
#include "igraphicsapi.h"
#include "directx11weaver.h"
#include "directx12weaver.h"
#include "hotkeymanager.h"

#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

IGraphicsApi* weaverImplementation = nullptr;
SR::SRContext* srContext = nullptr;
HotKeyManager* hotKeyManager = nullptr;
std::thread createContextThread;

void createNewSrContext()
{
    // Initialize SRContext
    while (srContext == nullptr) {
        try {
            srContext = new SR::SRContext();
        }
        catch (SR::ServerNotAvailableException e) {
            std::cout << "Server not available, trying again in 100 milisecond" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    weaverImplementation->set_context_validity(true);
}

void executeHotKeyFunctionByType(std::map<shortcutType, bool> hotKeyList) {
    std::map<shortcutType, bool>::iterator i;
    for (i = hotKeyList.begin(); i != hotKeyList.end(); i++) {
        switch (i->first) {
        case shortcutType::toggleSR:
            //reshade::log_message(3, "TOGGLE SR HOTKEY TRIGGERED!");
            if (i->second) {
                //Make a new thread here.
                createContextThread = std::thread(createNewSrContext);
            }
            else {
                weaverImplementation->set_context_validity(false);
                weaverImplementation->set_weaver_validity(false);
                if (srContext != nullptr) {
                    srContext->deleteSRContext(srContext);
                    srContext = nullptr;
                }
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
    std::map<shortcutType, bool> hotKeyList;

    //If the context creation thread is joinable and the context is created. Join.
    if (srContext != nullptr && createContextThread.joinable()) {
        createContextThread.join();
    }

    //Check if certain hotkeys are being pressed
    if (hotKeyManager != nullptr) {
        //Find out which hotkeys have changed their toggled state, then execute their respective code.
        hotKeyList = hotKeyManager->checkHotKeys(runtime, srContext);
        executeHotKeyFunctionByType(hotKeyList);
    }

    weaverImplementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static void on_reshade_present(reshade::api::effect_runtime* runtime) {
    reshade::log_message(3, "RECEIVED ON PRESENT CALLBACK");

    //std::map<shortcutType, bool> hotKeyList;

    ////Check if certain hotkeys are being pressed
    //if (hotKeyManager != nullptr) {
    //    //Find out which hotkeys have changed their toggled state, then execute their respective code.
    //    hotKeyList = hotKeyManager->checkHotKeys(runtime, srContext);
    //    executeHotKeyFunctionByType(hotKeyList);
    //}
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
