/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "igraphicsapi.h"

#include "configManager.h"

// Values of the anti_crosstalk_mode config key, in the same order as the AntiCrosstalkSetting enum.
static const std::vector<std::string> anti_crosstalk_config_values = { "sr_default", "off", "static", "dynamic" };
// Display names of WeaverACTMode, in the same order as the enum.
static const char* act_mode_names[] = { "Off", "Static", "Dynamic" };

IGraphicsApi::IGraphicsApi() {
    user_presence_3d_toggle_checked = ConfigManager::read_from_config(gb_config_disable_3d_when_no_user).bool_value;
    enable_overlay_workaround = ConfigManager::read_from_config(gb_config_enable_overlay_workaround).bool_value;

    // Read the anti-crosstalk setting, unknown values keep the SR Platform default.
    std::string anti_crosstalk_value = ConfigManager::read_from_config(gb_config_anti_crosstalk_mode).string_value;
    for (int i = 0; i < anti_crosstalk_config_values.size(); i++) {
        if (anti_crosstalk_config_values[i] == anti_crosstalk_value) {
            anti_crosstalk_setting = i;
        }
    }
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
    ImGui::Text("SR Platform version: %s", getSRPlatformVersion());

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
            reshade::log::message(reshade::log::level::info, "User based 3D checkbox enabled");
        }
        else
        {
            reshade::log::message(reshade::log::level::info, "User based 3D checkbox disabled");
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
            reshade::log::message(reshade::log::level::info, "Weaving overlay workaround enabled");
        }
        else
        {
            reshade::log::message(reshade::log::level::info, "Weaving overlay workaround disabled");
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

    draw_anti_crosstalk_option();
}

void IGraphicsApi::draw_anti_crosstalk_option() {
    if (anti_crosstalk_support == AntiCrosstalkSupport::NOT_CHECKED) {
        // The weaver isn't initialized yet.
        return;
    }
    if (anti_crosstalk_support == AntiCrosstalkSupport::NOT_SUPPORTED_BY_GRAPHICS_API) {
        ImGui::TextDisabled("Anti-crosstalk: not available for this graphics API");
        return;
    }
    if (anti_crosstalk_support == AntiCrosstalkSupport::REQUIRES_NEWER_SR_PLATFORM) {
        ImGui::TextDisabled("Anti-crosstalk: not available, update the SR Platform to use it");
        return;
    }

    // Show which mode the SR Platform default resolves to, so the user knows what they get.
    std::string sr_default_option = "SR Platform default (" + std::string(act_mode_names[static_cast<int>(sr_platform_act_mode)]) + ")";
    const char* options[] = { sr_default_option.c_str(), "Off", "Static", "Dynamic" };

    if (ImGui::Combo("Anti-crosstalk##act", &anti_crosstalk_setting, options, IM_ARRAYSIZE(options))) {
        apply_anti_crosstalk_setting();

        // Write to config file
        ConfigManager::ConfigValue val;
        val.key = gb_config_anti_crosstalk_mode;
        val.string_value = anti_crosstalk_config_values[anti_crosstalk_setting];
        val.value_type = ConfigManager::ConfigValue::Type::String;
        ConfigManager::write_config_value(val);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Reduces ghosting (crosstalk), where one eye faintly sees the image meant for the other eye.");
        ImGui::Text("Static: applies a fixed correction.");
        ImGui::Text("Dynamic: adapts the correction to your head position.");
        ImGui::EndTooltip();
    }
}

void IGraphicsApi::init_anti_crosstalk() {
    // Remember the mode the SR Platform picked, this is what SR_PLATFORM_DEFAULT restores to.
    anti_crosstalk_support = get_weaver_act_mode(sr_platform_act_mode);

    if (anti_crosstalk_support == AntiCrosstalkSupport::SUPPORTED) {
        apply_anti_crosstalk_setting();
    }
    else if (anti_crosstalk_support == AntiCrosstalkSupport::REQUIRES_NEWER_SR_PLATFORM) {
        reshade::log::message(reshade::log::level::warning, "Anti-crosstalk is not supported by this SR Platform version.");
    }
}

void IGraphicsApi::apply_anti_crosstalk_setting() {
    WeaverACTMode mode;
    switch (anti_crosstalk_setting) {
        case AntiCrosstalkSetting::ACT_OFF:
            mode = WeaverACTMode::Off;
            break;
        case AntiCrosstalkSetting::ACT_STATIC:
            mode = WeaverACTMode::Static;
            break;
        case AntiCrosstalkSetting::ACT_DYNAMIC:
            mode = WeaverACTMode::Dynamic;
            break;
        default:
            mode = sr_platform_act_mode;
            break;
    }

    if (set_weaver_act_mode(mode)) {
        std::string message = "Anti-crosstalk mode set to: " + std::string(act_mode_names[static_cast<int>(mode)]);
        reshade::log::message(reshade::log::level::info, message.c_str());
    }
    else {
        anti_crosstalk_support = AntiCrosstalkSupport::REQUIRES_NEWER_SR_PLATFORM;
        reshade::log::message(reshade::log::level::warning, "Unable to set the anti-crosstalk mode, this SR Platform version doesn't support it.");
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

    reshade::log::message(reshade::log::level::info, latency_log.c_str());
}
