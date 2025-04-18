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

#include "configManager.h"
#include "gbConstants.h"

void ConfigManager::reload_config() {
    try {
        // Attempt to read config values from ReShade.ini
        registered_config_values.push_back(ConfigManager::read_from_config(
                gb_config_disable_3d_when_no_user));
        registered_config_values.push_back(ConfigManager::read_from_config(
                gb_config_disable_3d_when_no_user_grace_duration));
        registered_config_values.push_back(ConfigManager::read_from_config(
                gb_config_enable_overlay_workaround));
    } catch (std::runtime_error &e) {
        // Couldn't find the config values, let's write the defaults in the .ini
        std::string error_msg = "Unable to find config value in ReShade.ini: Now writing/loading defaults...";
        reshade::log_message(reshade::log_level::warning, error_msg.c_str());
        write_missing_config_values();
        load_default_config();
    }
}

ConfigManager::ConfigManager() {
    // Check if we can read the config values we expect. Otherwise, write defaults.
    reload_config();
}

void ConfigManager::write_missing_config_values() {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_disable_3d_when_no_user.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_disable_3d_when_no_user.c_str(), "false");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_disable_3d_when_no_user_grace_duration.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_disable_3d_when_no_user_grace_duration.c_str(), "0");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_enable_overlay_workaround.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_enable_overlay_workaround.c_str(), "true");
    }
}

void remove_unwanted_nulls(std::vector<char>& vec) {
    // Find the first occurrence of '\0'
    vec.erase(std::remove(vec.begin(), vec.end(), '\0'), vec.end());
}

ConfigManager::ConfigValue ConfigManager::read_from_config(const std::string &key) {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), key.c_str(), nullptr, &value_size);
    if (value_size > 0) {
        try {
            // Get the value from the config
            std::vector<char> value(value_size);
            reshade::get_config_value(nullptr, gb_config_section_name.c_str(), key.c_str(), value.data(), &value_size);
            // Remove unwanted '\0' characters as they confuse the string conversion.
            remove_unwanted_nulls(value);
            // Convert read value to lower case string
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            std::string str(value.begin(), value.end());
            // Try parsing as boolean
            if (str == "true") {
                return ConfigValue(key, true);
            }
            if (str == "false") {
                return ConfigValue(key, false);
            }

            // Try parsing as integer
            size_t idx = 0;
            int int_value = std::stoi(str, &idx);
            if (idx == str.length()) { // Entire string was a valid integer
                return ConfigValue(key, int_value);
            }

            return ConfigValue(key, str);

        }
        catch (...)
        {
            std::string error_msg = "Unable to read value from config file for config value: ";
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
    registered_config_values.push_back(ConfigValue(gb_config_disable_3d_when_no_user, false));
    registered_config_values.push_back(ConfigValue(gb_config_disable_3d_when_no_user_grace_duration, 0));
    registered_config_values.push_back(ConfigValue(gb_config_enable_overlay_workaround, true));
}

bool ConfigManager::write_config_value(ConfigManager::ConfigValue value) {
    try {
        switch (value.value_type) {
            case ConfigValue::Type::String:
                reshade::set_config_value(nullptr, gb_config_section_name.c_str(), value.key.c_str(), value.string_value.c_str());
                break;
            case ConfigValue::Type::Bool:
                reshade::set_config_value(nullptr, gb_config_section_name.c_str(), value.key.c_str(), (value.bool_value) ? "true" : "false");
                break;
            case ConfigValue::Type::Int:
                reshade::set_config_value(nullptr, gb_config_section_name.c_str(), value.key.c_str(), std::to_string(value.int_value).c_str());
                break;
            default:
                break;
        }
    } catch (std::runtime_error &e) {
        std::string error_msg = "Unable to write config value in ReShade.ini, your config changed will not be saved for this session.";
        reshade::log_message(reshade::log_level::warning, error_msg.c_str());
        return false;
    }

    return true;
}
