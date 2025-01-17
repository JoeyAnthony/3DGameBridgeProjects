/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <reshade/include/reshade.hpp>

#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    // Todo: Check if we can read the config values we expect. Otherwise write defaults.
    ConfigValue disable_3d_when_no_user_present = ConfigManager::read_from_config("disable_3d_when_no_user_present");
    ConfigValue disable_3d_when_no_user_present_grace_duration = ConfigManager::read_from_config("disable_3d_when_no_user_present_grace_duration");
    if (disable_3d_when_no_user_present.value_type == ConfigValue::Type::None || disable_3d_when_no_user_present_grace_duration.value_type == ConfigValue::Type::None) {
        write_missing_config_values();
    }
}

void ConfigManager::write_missing_config_values() {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, "3DGameBridge", "disable_3d_when_no_user_present", nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, "3DGameBridge", "disable_3d_when_no_user_present", "false");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, "3DGameBridge", "disable_3d_when_no_user_present_grace_duration", nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, "3DGameBridge", "disable_3d_when_no_user_present_grace_duration_in_seconds", 3);
    }
}

void removeUnwantedNulls(std::vector<char>& vec) {
    // Find the first occurrence of '\0'
    auto firstNullIt = std::find(vec.begin(), vec.end(), '\0');

    if (firstNullIt != vec.end()) {
        // Check if the vector ends with only '\0' characters
        if (std::all_of(firstNullIt, vec.end(), [](char c) { return c == '\0'; })) {
            // The vector ends with only '\0', so preserve it as is
            return;
        }
        // Otherwise, erase everything before the first group of '\0'
        vec.erase(vec.begin(), firstNullIt + 1);
    }
}

ConfigManager::ConfigValue ConfigManager::read_from_config(const std::string &key) {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, "3DGameBridge", key.c_str(), nullptr, &value_size);
    if (value_size > 0) {
        try {
            // Get the value from the config
            std::vector<char> value(value_size);
            reshade::get_config_value(nullptr, "3DGameBridge", key.c_str(), value.data(), &value_size);
            // Remove unwanted '\0' characters as they confuse the string conversion.
            removeUnwantedNulls(value);
            // Convert read value to lower case string
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            std::string str(value.begin(), value.end());
            // Try parsing as boolean
            if (str == "true") {
                return ConfigValue(true);
            }
            if (str == "false") {
                return ConfigValue(false);
            }

            // Try parsing as integer
            size_t idx = 0;
            int int_value = std::stoi(str, &idx);
            if (idx == str.length()) { // Entire string was a valid integer
                return ConfigValue(int_value);
            }

            return ConfigValue(str);

        }
        catch (...)
        {
            std::string error_msg = "Unable to read hotkey from config file for config value: ";
            error_msg += key;
            error_msg += ". Please read the addon documentation to find out what values are valid.";
            reshade::log_message(reshade::log_level::error, error_msg.c_str());
        }
        return ConfigValue();
    }
    throw std::runtime_error("Unable to read key from config");
}

void ConfigManager::load_default_config() {
    registered_config_values.erase(registered_config_values.begin(), registered_config_values.end());
    registered_config_values.push_back(ConfigValue());
    registered_hot_keys.erase(registered_hot_keys.begin(), registered_hot_keys.end());
    registered_hot_keys.push_back(HotKey(true, shortcutType::TOGGLE_SR, 0x31, false, false, true));
    registered_hot_keys.push_back(HotKey(true, shortcutType::TOGGLE_LENS, 0x32, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_3D, 0x33, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_LENS_AND_3D, 0x34, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_LATENCY_MODE, 0x35, false, false, true));
}
