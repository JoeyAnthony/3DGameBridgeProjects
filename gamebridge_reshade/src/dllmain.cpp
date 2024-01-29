/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include <windows.h>
#include <winver.h>
#pragma comment(lib, "Version.lib")

#include "pch.h"
#include "igraphicsapi.h"
#include "directx11weaver.h"
#include "directx12weaver.h"
#include "hotkeymanager.h"
#include "directx10weaver.h"
#include "directx9weaver.h"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
#include <unordered_map>

#define CHAR_BUFFER_SIZE 256

using namespace std;

IGraphicsApi* weaver_implementation = nullptr;
SR::SRContext* sr_context = nullptr;
SR::SwitchableLensHint* lens_hint = nullptr;
HotKeyManager* hotKey_manager = nullptr;

//Currently we use this string to determine if we should toggle this shader on press of the shortcut. We can expand this to a list later.
static const std::string depth_3D_shader_name = "SuperDepth3D";
static const std::string fxaa_shader_name = "FXAA";
static char g_charBuffer[CHAR_BUFFER_SIZE];
static size_t g_charBufferSize = CHAR_BUFFER_SIZE;

std::vector<LPCWSTR> reshade_dll_names =  { L"dxgi.dll", L"ReShade.dll", L"ReShade64.dll", L"ReShade32.dll", L"d3d9.dll", L"d3d10.dll", L"d3d11.dll", L"d3d12.dll", L"opengl32.dll" };

struct DeviceDataContainer {
    reshade::api::effect_runtime* current_runtime = nullptr;
    unordered_map<std::string, bool> all_enabled_techniques;
};

// Function to get the version information of a module
vector<size_t> get_module_version_info(LPCWSTR moduleName) {
    vector<size_t> reshade_version_numbers = {};

    // Get the size of the version info buffer
    DWORD version_info_size_handle;
    DWORD dw_size = GetFileVersionInfoSizeExW(FILE_VER_GET_NEUTRAL , moduleName, &version_info_size_handle);
    if (dw_size == 0) {
        reshade::log_message(reshade::log_level::warning, "Failed to get version info size.");
        return reshade_version_numbers;
    }

    // Allocate memory for the version info
    std::shared_ptr<BYTE> version_info_buffer(new BYTE[dw_size], std::default_delete<BYTE[]>());

    // Retrieve the version info
    if (!GetFileVersionInfoExW(FILE_VER_GET_NEUTRAL , moduleName, 0, dw_size, version_info_buffer.get())) {
        reshade::log_message(reshade::log_level::warning, "Failed to get version info.");
        return reshade_version_numbers;
    }

    // Query the version information
    LPVOID file_info;
    UINT file_info_size;
    if (!VerQueryValueW(version_info_buffer.get(), L"\\", &file_info, &file_info_size)) {
        reshade::log_message(reshade::log_level::warning, "Failed to query version info.");
        return reshade_version_numbers;
    }

    // Extract version components
    auto* p_vs_fixed_file_info = reinterpret_cast<VS_FIXEDFILEINFO*>(file_info);
    WORD majorVersion = HIWORD(p_vs_fixed_file_info->dwFileVersionMS);
    WORD minorVersion = LOWORD(p_vs_fixed_file_info->dwFileVersionMS);
    WORD buildNumber = HIWORD(p_vs_fixed_file_info->dwFileVersionLS);

    // Format the version information as a string
    reshade_version_numbers.push_back(std::stoi(std::to_string(majorVersion)));
    reshade_version_numbers.push_back(std::stoi(std::to_string(minorVersion)));
    reshade_version_numbers.push_back(std::stoi(std::to_string(buildNumber)));

    return reshade_version_numbers;
}

static void enumerate_techniques(reshade::api::effect_runtime* runtime, std::function<void(reshade::api::effect_runtime*, reshade::api::effect_technique, string&)> func)
{
    runtime->enumerate_techniques(nullptr, [func](reshade::api::effect_runtime* rt, reshade::api::effect_technique technique) {
        g_charBufferSize = CHAR_BUFFER_SIZE;
        rt->get_technique_name(technique, g_charBuffer, &g_charBufferSize);
        string name(g_charBuffer);
        func(rt, technique, name);
        });
}

//Todo: Move this function outside of the dllmain. 
//It was placed here because it needed access to the SRContext but it should be moved to another class for cleanliness sake.
static void execute_hot_key_function_by_type(std::map<shortcutType, bool> hot_key_list, reshade::api::effect_runtime* runtime) {
    std::map<shortcutType, bool>::iterator i;
    vector<reshade::api::effect_technique> togglable_3D_effects = {};
    map<shortcutType, bool> toggle_map;

    for (i = hot_key_list.begin(); i != hot_key_list.end(); i++) {
        switch (i->first) {
        case shortcutType::toggle_SR:
            //Here we want to completely disable all SR related functions including the eye tracker, weaver, context etc.
            break;
        case shortcutType::toggle_lens:
            //Here we want to toggle to the lens and toggle weaving
            if (i->second) {
                lens_hint->enable();
                //Bypass weave() call
                weaver_implementation->do_weave(true);
            }
            else {
                lens_hint->disable();
                //Bypass weave() call
                weaver_implementation->do_weave(false);
            }
            break;
        case shortcutType::toggle_3D:
            //Here we want to toggle Depth3D or any other 3D effect we use to create our second eye image.
            enumerate_techniques(runtime, [&togglable_3D_effects](reshade::api::effect_runtime *runtime,
                                                                  reshade::api::effect_technique technique,
                                                                  string &name) {
                if (!name.compare(depth_3D_shader_name)) {
                    togglable_3D_effects.push_back(technique);
                }
            });

            for (int effect_iterator = 0; effect_iterator < togglable_3D_effects.size(); effect_iterator++) {
                if (i->second) {
                    runtime->set_technique_state(togglable_3D_effects[effect_iterator], true);
                }
                else {
                    runtime->set_technique_state(togglable_3D_effects[effect_iterator], false);
                }
            }
            break;
        case shortcutType::toggle_lens_and_3D:
            //Todo: This should look at the current state of the lens toggle and 3D toggle, then, flip those.This toggle having its own state isn't great.
            if (i->second) {
                toggle_map = {{shortcutType::toggle_lens, true}, {shortcutType::toggle_3D, true} };
            }
            else {
                toggle_map = {{shortcutType::toggle_lens, false}, {shortcutType::toggle_3D, false} };
            }
                execute_hot_key_function_by_type(toggle_map, runtime);
            break;
        case shortcutType::toggle_latency_mode:
            //Here we want to toggle the eye tracker latency mode between framerate-adaptive and latency-in-frames.
            if (i->second) {
                // Set the latency in frames to -1 to use ReShade's swap_chain buffer count.
                weaver_implementation->set_latency_in_frames(-1);

                // Log the current mode:
                reshade::log_message(reshade::log_level::info, "Current latency mode set to: LATENCY_IN_FRAMES");
            }
            else {
                // Set the latency to the SR default of 40000 microseconds (Tuned for 60Hz)
                weaver_implementation->set_latency_framerate_adaptive(DEFAULT_WEAVER_LATENCY);

                //Log the current mode:
                reshade::log_message(reshade::log_level::info, "Current latency mode set to: STATIC 40000 Microseconds");
            }

        default:
            break;
        }
    }
}

static void draw_status_overlay(reshade::api::effect_runtime* runtime) {
    bool printStatusInWeaver = true;
    std::string status_string = "Status: \n";
    if (sr_context == nullptr) {
        // Unable to connect to the SR Service. Fall back to drawing the overlay UI ourselves.
        status_string += "INACTIVE - NO SR SERVICE DETECTED, MAKE SURE THE SR PLATFORM IS INSTALLED AND RUNNING\nwww.srappstore.com\n";
        printStatusInWeaver = false;
    } else if (weaver_implementation) {
        weaver_implementation->draw_status_overlay(runtime);
    } else {
        // Unable to create weaver implementation. Fall back to drawing the overlay UI ourselves.
        status_string += "INACTIVE - UNSUPPORTED GRAPHICS API\n";
        printStatusInWeaver = false;
    }
    if (!printStatusInWeaver) {
        ImGui::TextUnformatted(status_string.c_str());
    }
}

static void draw_debug_overlay(reshade::api::effect_runtime* runtime) {
    //weaver_implementation->draw_debug_overlay(runtime);
}

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaver_implementation->draw_sr_settings_overlay(runtime);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaver_implementation->draw_settings_overlay(runtime);
}

static void on_reshade_reload_effects(reshade::api::effect_runtime* runtime) {
    vector<reshade::api::effect_technique> fxaaTechnique = {};

    // Todo: This is not a nice way of forcing on_finish_effects to trigger. Maybe make a dummy shader that you always turn on instead (or use a different callback)
    // Toggle FXAA.fx on
    enumerate_techniques(runtime, [&fxaaTechnique](reshade::api::effect_runtime* runtime, reshade::api::effect_technique technique, string& name) {
        if (!name.compare(fxaa_shader_name)) {
            reshade::log_message(reshade::log_level::info, "Found FXAA.fx shader!");
            fxaaTechnique.push_back(technique);
        }
        });

    for (int effectIterator = 0; effectIterator < fxaaTechnique.size(); effectIterator++) {
        runtime->set_technique_state(fxaaTechnique[effectIterator], true);
        reshade::log_message(reshade::log_level::info, "Toggled FXAA to ensure on_finish_effects gets called.");
    }
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    std::map<shortcutType, bool> hot_key_list;

    //Check if certain hotkeys are being pressed
    if (hotKey_manager != nullptr) {
        //Find out which hotkeys have changed their toggled state, then execute their respective code.
        hot_key_list = hotKey_manager->check_hot_keys(runtime, sr_context);
        execute_hot_key_function_by_type(hot_key_list, runtime);
    }

    weaver_implementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static bool init_sr() {
    //Construct SR Context and senses
    if (sr_context == nullptr) {
        try {
            sr_context = new SR::SRContext;
        }
        catch (SR::ServerNotAvailableException& ex) {
            // Unable to construct SR Context.
            reshade::log_message(reshade::log_level::error, "Unable to connect to the SR Service, make sure the SR Platform is installed and running.");
            return false;
        }
        lens_hint = SR::SwitchableLensHint::create(*sr_context);
        sr_context->initialize();
    }

    // Only register these ReShade event callbacks when we have a valid SR Service, otherwise the app may crash.
    reshade::register_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);
    reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(&on_reshade_reload_effects);

    return true;
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    if (!init_sr()) {
        return;
    }

    //Todo: Move these hard-coded hotkeys to user-definable hotkeys in the .ini file
    //Register some standard hotkeys
    if (hotKey_manager == nullptr) {
        hotKey_manager = new HotKeyManager();
    }

    //Then, check the active graphics API and pass it a new context.
    if (weaver_implementation == nullptr) {
        switch (runtime->get_device()->get_api()) {
            case reshade::api::device_api::d3d9:
                weaver_implementation = new DirectX9Weaver(sr_context);
                break;
            case reshade::api::device_api::d3d10:
                weaver_implementation = new DirectX10Weaver(sr_context);
                break;
            case reshade::api::device_api::d3d11:
                weaver_implementation = new DirectX11Weaver(sr_context);
                break;
            case reshade::api::device_api::d3d12:
                weaver_implementation = new DirectX12Weaver(sr_context);
                break;
            default:
                reshade::log_message(reshade::log_level::warning, "Unable to determine graphics API, it may not be supported. Becoming inactive.");
                break;
        }
    }

    // Get ReShade version number
    for (int i = 0; i < reshade_dll_names.size(); i++) {
        std::vector<size_t> version_nrs = get_module_version_info(reshade_dll_names[i]);
        if (version_nrs.size() == 3) {
            weaver_implementation->reshade_version_nr_major = version_nrs[0];
            weaver_implementation->reshade_version_nr_minor = version_nrs[1];
            weaver_implementation->reshade_version_nr_patch = version_nrs[2];
            break;
        }
    }

    weaver_implementation->on_init_effect_runtime(runtime);
}

static void on_destroy_swapchain(reshade::api::swapchain *swapchain) {
    if(weaver_implementation != nullptr) {
        weaver_implementation->on_destroy_swapchain(swapchain);
        reshade::log_message(reshade::log_level::warning, "SWAPCHAIN DESTROYED!");
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

        if (!reshade::register_addon(hModule))
            return FALSE;

        reshade::register_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);
        reshade::register_event<reshade::addon_event::destroy_swapchain>(&on_destroy_swapchain);

        reshade::register_overlay(nullptr, &draw_status_overlay);

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
