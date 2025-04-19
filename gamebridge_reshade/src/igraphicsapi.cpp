/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "igraphicsapi.h"

#include "configManager.h"

IGraphicsApi::IGraphicsApi() {
    user_presence_3d_toggle_checked = ConfigManager::read_from_config(gb_config_disable_3d_when_no_user).bool_value;
    enable_overlay_workaround = ConfigManager::read_from_config(gb_config_enable_overlay_workaround).bool_value;
}

/**
 * Takes a major, minor and patch number from ReShade and concatenates them into a single number for easier processing.
 */
int32_t IGraphicsApi::get_concatinated_reshade_version() {
    std::string result = "";
    result += std::to_string(reshade_version_nr_major);
    result += std::to_string(reshade_version_nr_minor);
    result += std::to_string(reshade_version_nr_patch);
    return std::stoi(result);
}

void IGraphicsApi::draw_status_overlay(reshade::api::effect_runtime *runtime, const std::string& error_message) {
    // Log activity status
    ImGui::TextUnformatted("Status: ACTIVE");

    if (!error_message.empty()) {
        ImGui::TextColored(ImColor(240,100,100), "%s", error_message.c_str());
    }

    // Log the latency mode
    std::string latencyModeDisplay = "Latency mode: ";
    if (get_latency_mode() == LatencyModes::FRAMERATE_ADAPTIVE) {
        latencyModeDisplay += "IN " + std::to_string(weaver_latency_in_us) + " MICROSECONDS";
    }
    else if (get_latency_mode() == LatencyModes::LATENCY_IN_FRAMES) {
        latencyModeDisplay += "IN 1 FRAME";
    }
    else {
        latencyModeDisplay += "IN " + std::to_string(runtime->get_back_buffer_count()) + " FRAME(S)";
    }
    ImGui::TextUnformatted(latencyModeDisplay.c_str());

    // Log the buffer type, this can be removed once we've tested a larger amount of games.
    std::string s = "Buffer type: " + std::to_string(static_cast<uint32_t>(current_buffer_format));
    ImGui::TextUnformatted(s.c_str());

    // Draw a checkbox and check for changes
    // These blocks are executed when the checkbox value is toggled
    if (ImGui::Checkbox("User presence based 3D toggle##presence", &user_presence_3d_toggle_checked))
    {
        ConfigManager::ConfigValue val;
        val.key = "disable_3d_when_no_user_present";
        if (user_presence_3d_toggle_checked)
        {
            reshade::log_message(reshade::log_level::info, "User based 3D checkbox enabled");
        }
        else
        {
            reshade::log_message(reshade::log_level::info, "User based 3D checkbox disabled");
        }
        // Write to config file
        val.bool_value = user_presence_3d_toggle_checked;
        val.value_type = ConfigManager::ConfigValue::Type::Bool;
        ConfigManager::write_config_value(val);
    }

    if (ImGui::Checkbox("Enable overlay workaround##overlay", &enable_overlay_workaround))
    {
        ConfigManager::ConfigValue val;
        val.key = "enable_overlay_workaround";
        if (enable_overlay_workaround)
        {
            reshade::log_message(reshade::log_level::info, "Weaving overlay workaround enabled");
        }
        else
        {
            reshade::log_message(reshade::log_level::info, "Weaving overlay workaround disabled");
        }
        // Write to config file
        val.bool_value = enable_overlay_workaround;
        val.value_type = ConfigManager::ConfigValue::Type::Bool;
        ConfigManager::write_config_value(val);

        // Set weaver_initialized to false force reconstruction of the weaver.
        weaver_initialized = false;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("This bypasses the SR SDK check for toggling 3D based on if the 3D window is obstructed by i.e. overlays.");
        ImGui::EndTooltip();
    }
}

bool IGraphicsApi::is_user_presence_3d_toggle_checked() {
    return user_presence_3d_toggle_checked;
}

bool IGraphicsApi::is_overlay_workaround_enabled() {
    return enable_overlay_workaround;
}

void IGraphicsApi::determine_default_latency_mode() {
    // Check what version of SR we're on, if we're on 1.30 or up, switch to latency in frames.
    std::string latency_log;

    if (VersionComparer::is_version_newer(getSRPlatformVersion(), 1, 29, 999)) {
        set_latency_in_frames(-1);
        latency_log = "Current latency mode set to: LATENCY_IN_FRAMES_AUTOMATIC";
    } else {
        // Set mode to latency in frames by default.
        set_latency_frametime_adaptive(weaver_latency_in_us);
        latency_log = "Current latency mode set to: STATIC " + std::to_string(weaver_latency_in_us) + " Microseconds";
    }

    reshade::log_message(reshade::log_level::info, latency_log.c_str());
}
