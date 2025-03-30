/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "igraphicsapi.h"

#include "configManager.h"

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

void IGraphicsApi::draw_status_overlay(reshade::api::effect_runtime *runtime) {
    // Log activity status
    ImGui::TextUnformatted("Status: ACTIVE");

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
    // This block is executed when the checkbox value is toggled
    if (ImGui::Checkbox("User presence based 3D toggle", &user_presence_3d_toggle_checked))
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
}
