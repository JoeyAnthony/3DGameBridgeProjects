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
#include "systemEventMonitor.h"
#include "openglweaver.h"
#include "delayLoader.h"
#include "configManager.h"
#include "gbConstants.h"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>
#include <mutex>
#include <sr/sense/system/systemsense.h>
#ifdef ENABLE_GLAD
#include <glad/gl.h>
#endif


#define CHAR_BUFFER_SIZE 256

using namespace std;

IGraphicsApi* weaver_implementation = nullptr;
SR::SRContext* sr_context = nullptr;
SR::SwitchableLensHint* lens_hint = nullptr;
SR::SystemSense* system_sense;
SystemEventMonitor sense_listener;
HotKeyManager* hotKey_manager = nullptr;
ConfigManager* config_manager = nullptr;

// Currently we use this string to determine if we should toggle this shader on press of the shortcut. We can expand this to a list later.
static bool dll_failed_to_load = false;
static const std::string depth_3D_shader_name = "SuperDepth3D";
static const std::string sr_shader_name = "SR";
static char char_buffer[CHAR_BUFFER_SIZE];
static size_t char_buffer_size = CHAR_BUFFER_SIZE;
static bool sr_initialized = false;
static bool user_lost_grace_period_active = false;
static bool user_lost_logic_enabled = false;
static bool is_potentially_unstable_opengl_version = false;
static int user_lost_additional_grace_period_duration_in_ms = 0;
static chrono::steady_clock::time_point user_lost_timestamp;

// Runtime & device lifecycle tracking
static std::atomic<int> active_effect_runtimes{0};
static std::once_flag exit_cleanup_once;
static std::atomic<bool> device_destroyed{false};

std::vector<LPCWSTR> reshade_dll_names =  { L"dxgi.dll", L"ReShade.dll", L"ReShade64.dll", L"ReShade32.dll", L"d3d9.dll", L"d3d10.dll", L"d3d11.dll", L"d3d12.dll", L"opengl32.dll" };

void deregisterCallbacksOnDllLoadFailure();

// Forward declare new helpers
static void perform_exit_cleanup();

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
        char_buffer_size = CHAR_BUFFER_SIZE;
        rt->get_technique_name(technique, char_buffer, &char_buffer_size);
        string name(char_buffer);
        func(rt, technique, name);
        });
}

// Todo: Move this function outside of the dllmain.
// It was placed here because it needed access to the SRContext but it should be moved to another class for cleanliness sake.
static void execute_hot_key_function_by_type(std::map<shortcutType, bool> hot_key_list, reshade::api::effect_runtime* runtime) {
    std::map<shortcutType, bool>::iterator i;
    vector<reshade::api::effect_technique> togglable_3D_effects = {};
    map<shortcutType, bool> toggle_map;

    for (i = hot_key_list.begin(); i != hot_key_list.end(); i++) {
        switch (i->first) {
        case shortcutType::TOGGLE_SR:
            // Here we want to completely disable all SR related functions including the eye tracker, weaver, context etc.
            break;
        case shortcutType::TOGGLE_LENS:
            // Here we want to toggle to the lens and toggle weaving
            if (i->second) {
                lens_hint->enable();
                // Bypass weave() call
                weaver_implementation->do_weave(true);
            }
            else {
                lens_hint->disable();
                // Bypass weave() call
                weaver_implementation->do_weave(false);
            }
            break;
        case shortcutType::TOGGLE_3D:
            // Here we want to toggle Depth3D or any other 3D effect we use to create our second eye image.
            enumerate_techniques(runtime, [&togglable_3D_effects](reshade::api::effect_runtime *runtime,
                                                                  reshade::api::effect_technique technique,
                                                                  string &name) {
                if (!name.compare(depth_3D_shader_name)) {
                    togglable_3D_effects.push_back(technique);
                }
            });

            for (size_t effect_iterator = 0; effect_iterator < togglable_3D_effects.size(); effect_iterator++) {
                runtime->set_technique_state(togglable_3D_effects[effect_iterator], i->second);
            }
            break;
        case shortcutType::TOGGLE_LENS_AND_3D:
            // This looks at the current state of the lens and toggles it.
            if (lens_hint != nullptr && lens_hint->isEnabled()) {
                toggle_map = {{shortcutType::TOGGLE_LENS, false}, {shortcutType::TOGGLE_3D, false} };
            }
            else {
                toggle_map = {{shortcutType::TOGGLE_LENS, true}, {shortcutType::TOGGLE_3D, true} };
            }
            execute_hot_key_function_by_type(toggle_map, runtime);
            break;
        case shortcutType::TOGGLE_LATENCY_MODE:
            // Here we want to toggle the eye tracker latency mode between framerate-adaptive and latency-in-frames.
            if (i->second) {
                // Set the latency in frames to -1 to use ReShade's swap_chain buffer count.
                weaver_implementation->set_latency_in_frames(-1);

                // Log the current mode:
                reshade::log_message(reshade::log_level::info, "Current latency mode set to: LATENCY_IN_FRAMES");
            }
            else {
                // Set the latency to the SR default of 40000 microseconds (Tuned for 60Hz)
                weaver_implementation->set_latency_frametime_adaptive(weaver_implementation->weaver_latency_in_us);

                // Log the current mode:
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
    if (dll_failed_to_load) {
        // Unable to load at least one of the SR DLLs
        status_string += "INACTIVE - UNABLE TO LOAD ALL SR DLLS, MAKE SURE THE SR PLATFORM IS INSTALLED AND RUNNING\nhttps://github.com/LeiaInc/leiainc.github.io/tree/master/SRSDK\n";
        printStatusInWeaver = false;
    }
    else if (sr_context == nullptr) {
        // Unable to connect to the SR Service. Fall back to drawing the overlay UI ourselves.
        status_string += "INACTIVE - NO SR SERVICE DETECTED, MAKE SURE THE SR PLATFORM IS INSTALLED AND RUNNING\nhttps://github.com/LeiaInc/leiainc.github.io/tree/master/SRSDK\n";
        printStatusInWeaver = false;
    }
    else if (weaver_implementation) {
        weaver_implementation->draw_status_overlay(runtime);
    }
    else {
        // Unable to create weaver implementation. Fall back to drawing the overlay UI ourselves.
        status_string += "INACTIVE - UNSUPPORTED GRAPHICS API\n";
        printStatusInWeaver = false;
    }

    if (!printStatusInWeaver) {
        ImGui::TextColored(ImColor(240,100,100), "%s", status_string.c_str());
    }
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    if (!sr_initialized) {
        return;
    }

    std::map<shortcutType, bool> hot_key_list;

    // Check if certain hotkeys are being pressed
    if (hotKey_manager != nullptr) {
        // Find out which hotkeys have changed their toggled state, then execute their respective code.
        hot_key_list = hotKey_manager->check_hot_keys(runtime, sr_context);
        execute_hot_key_function_by_type(hot_key_list, runtime);
    }

    // Check if certain hotkeys are being pressed
    // if (config_manager != nullptr) {
        // Todo: Update the state of the config based on if the user changed the setting in the UI
        // write_config_value();
    // }

    // Check if user is still within view of the camera
    user_lost_logic_enabled = weaver_implementation->is_user_presence_3d_toggle_checked();
    if (user_lost_logic_enabled) {
        if (sense_listener.isUserLost) {
            if (!user_lost_grace_period_active) {
                // Start the grace period timer
                user_lost_timestamp = chrono::high_resolution_clock::now();
            }
            user_lost_grace_period_active = true;
            // Compare current timeout with grace period from config file
            if (std::chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - user_lost_timestamp) > std::chrono::milliseconds(user_lost_additional_grace_period_duration_in_ms)) {
                // Skip the weaving step by returning here
                return;
            }
        } else {
            user_lost_grace_period_active = false;
        }
    }

    if (weaver_implementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb) == DLL_NOT_LOADED) {
        deregisterCallbacksOnDllLoadFailure();
    }
}

static bool init_sr() {
    // Construct SR Context and senses
    if (sr_context == nullptr) {
        try {
            sr_context = new SR::SRContext;
        }
        catch (std::runtime_error &e) {
            if (std::strcmp(e.what(), "Failed to load library") == 0) {
                deregisterCallbacksOnDllLoadFailure();
                return false;
            }
        }
        catch (SR::ServerNotAvailableException&) {
            // Unable to construct SR Context.
            reshade::log_message(reshade::log_level::error, "Unable to connect to the SR Service, make sure the SR Platform is installed and running.");
            sr_initialized = false;
            return false;
        }
        try {
            lens_hint = SR::SwitchableLensHint::create(*sr_context);
            system_sense = SR::SystemSense::create(*sr_context);
            sense_listener.stream.set(system_sense->openSystemEventStream(&sense_listener));
        }
        catch (std::runtime_error &e) {
            if (std::strcmp(e.what(), "Failed to load library") == 0) {
                deregisterCallbacksOnDllLoadFailure();
                return false;
            }
        }
        sr_context->initialize();
    }

    // Only register these ReShade event callbacks when we have a valid SR Service, otherwise the app may crash.
    sr_initialized = true;
    return true;
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    active_effect_runtimes.fetch_add(1, std::memory_order_relaxed);

    // Catch any runtime_error which points to not all required DLLs being present.
    if (!init_sr()) {
        return;
    }

    // These hotkeys are reconfigurable from the ReShade.ini file
    // Register some standard hotkeys
    if (hotKey_manager == nullptr) {
        hotKey_manager = new HotKeyManager();
    }
    // Read config values from .ini
    if (config_manager == nullptr) {
        config_manager = new ConfigManager();
        for (size_t i = 0; i < config_manager->registered_config_values.size(); i++) {
            if (config_manager->registered_config_values[i].key == gb_config_disable_3d_when_no_user) {
                user_lost_logic_enabled = config_manager->registered_config_values[i].bool_value;
            }
            if (config_manager->registered_config_values[i].key == gb_config_disable_3d_when_no_user_grace_duration) {
                user_lost_additional_grace_period_duration_in_ms = config_manager->registered_config_values[i].int_value;
            }
        }
    }

    try {
        // Then, check the active graphics API and pass it a new context.
        if (weaver_implementation == nullptr) {
            switch (runtime->get_device()->get_api()) {
                case reshade::api::device_api::opengl:
#ifdef ENABLE_GLAD
                    if (!gladLoaderLoadGL()) {
                        reshade::log_message(reshade::log_level::error, "Unable to load GLAD. Weaving may be incorrect between SR versions 1.30.x and 1.33.2");
                    }
#endif

                    weaver_implementation = new OpenGLWeaver(sr_context);
                    // Check if SR version > 1.30.x. If so, OpenGL is potentially bugged.
                    // Todo: Use this version check to enable/disable our OpenGL fix once Leia includes a fix in their official platform.
                    is_potentially_unstable_opengl_version = VersionComparer::is_version_newer(getSRPlatformVersion(), 1, 30, 999);
                    break;
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
                    reshade::log_message(reshade::log_level::warning,"Unable to determine graphics API, it may not be supported. Becoming inactive.");
                    break;
            }
        }
    }
    catch (std::runtime_error &e) {
        if (std::strcmp(e.what(), "Failed to load library") == 0) {
            deregisterCallbacksOnDllLoadFailure();
            return;
        }
    }

    // Get ReShade version number
    for (size_t i = 0; i < reshade_dll_names.size(); i++) {
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

// Called when an effect runtime is destroyed (can happen multiple times)
static void on_destroy_effect_runtime(reshade::api::effect_runtime* runtime) {
    int remaining = active_effect_runtimes.fetch_sub(1, std::memory_order_acq_rel) - 1;
    // Only consider triggering cleanup if device already gone or this was last runtime
    if (remaining <= 0 && device_destroyed.load(std::memory_order_acquire)) {
        std::call_once(exit_cleanup_once, []() {
            perform_exit_cleanup();
        });
    }
}

// Track device destruction as a stronger signal of process exit or teardown
static void on_destroy_device(reshade::api::device* device) {
    device_destroyed.store(true, std::memory_order_release);
    if (active_effect_runtimes.load(std::memory_order_acquire) == 0) {
        std::call_once(exit_cleanup_once, []() {
            perform_exit_cleanup();
        });
    }
}

static void perform_exit_cleanup() {
    reshade::log_message(reshade::log_level::info, "Performing exit cleanup.");

    // Stop using weaver_implementation with ReShade now
    if (weaver_implementation) {
        // If there is a custom shutdown method, call it (example):
        // weaver_implementation->shutdown();
        delete weaver_implementation;
        weaver_implementation = nullptr;
    }
    reshade::log_message(reshade::log_level::info, "Weaver destructed.");

    if (sr_context) {
        delete sr_context;
        sr_context = nullptr;
    }

    reshade::log_message(reshade::log_level::info, "Exit cleanup complete.");
}

void deregisterCallbacksOnDllLoadFailure() {
    // Mark the dll_failed_to_load status
    dll_failed_to_load = true;

    // Unregister all events
    reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);
    reshade::unregister_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);
    reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(&on_destroy_effect_runtime);
    reshade::unregister_event<reshade::addon_event::destroy_device>(&on_destroy_device);
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
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&on_destroy_effect_runtime);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);
        reshade::register_event<reshade::addon_event::destroy_device>(&on_destroy_device);
        reshade::register_overlay(nullptr, &draw_status_overlay);
        break;
    case DLL_PROCESS_DETACH:
        // Fallback: ensure cleanup ran (avoid heavy operations here).
        std::call_once(exit_cleanup_once, []() {
            perform_exit_cleanup();
        });
        reshade::unregister_addon(hModule);
        break;
    }
    return TRUE;
}
