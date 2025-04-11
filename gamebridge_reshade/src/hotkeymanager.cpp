/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkeyManager.h"
#include "gbConstants.h"

void HotKeyManager::remove_unwanted_nulls(std::vector<char>& vec) {
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

HotKey HotKeyManager::read_from_config(bool default_enabled, const std::string& key, shortcutType shortcut) {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), key.c_str(), nullptr, &value_size);
    if (value_size > 0) {
        HotKey created_key;
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
            // Filter out the actual key, it's always the first value before the ';' delimiter or the only value before string ending
            unsigned int found_key = 0;
            size_t idx = str.find(';');
            if (idx != std::string::npos) {
                std::string key_string = str.substr(0, idx);
                found_key = std::stoul(key_string, nullptr, 16);
            } else {
                idx = str.find('\0');
                if (idx != std::string::npos) {
                    std::string key_string = str.substr(0, idx);
                    found_key = std::stoul(key_string, nullptr, 16);
                }
            }
            // Create the hotkey
            created_key = HotKey(default_enabled, shortcut, found_key, str.find(gb_config_shift_mod.c_str()) != std::string::npos, str.find(gb_config_alt_mod.c_str()) != std::string::npos, str.find(gb_config_ctrl_mod.c_str()) != std::string::npos);
        }
        catch (...)
        {
            std::string error_msg = "Unable to read hotkey from config file for hotkey: ";
            error_msg += key;
            error_msg += ". Please ensure that you use the keycode table: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes";
            reshade::log_message(reshade::log_level::error, error_msg.c_str());
        }
        return created_key;
    }
    throw std::runtime_error("Unable to read key from config");
}

HotKeyManager::HotKeyManager() {
    // Create some default hotkeys.
    try {
        registered_hot_keys.push_back(read_from_config(true, gb_config_toggle_sr_key, TOGGLE_SR));
        registered_hot_keys.push_back(read_from_config(true, gb_config_toggle_lens_key, TOGGLE_LENS));
        registered_hot_keys.push_back(read_from_config(false, gb_config_toggle_3d_key, TOGGLE_3D));
        registered_hot_keys.push_back(read_from_config(false, gb_config_toggle_lens_and_3d_key, TOGGLE_LENS_AND_3D));
        registered_hot_keys.push_back(read_from_config(true, gb_config_toggle_latency_mode_key, TOGGLE_LATENCY_MODE));
    }
    catch (std::runtime_error &e) {
        // Couldn't find the config value, let's write the default in the .ini
        std::string error_msg = "Unable to find hotkey config in ReShade.ini: Now writing/loading defaults...";
        reshade::log_message(reshade::log_level::warning, error_msg.c_str());
        write_missing_hotkeys();
        load_default_hotkeys();
    }
}

bool check_modifier_keys(HotKey hotKey, reshade::api::effect_runtime* runtime) {
    if (hotKey.get_alt_required()) {
        if (!runtime->is_key_down(VK_MENU)) {
            return false;
        }
    }

    if (hotKey.get_ctrl_required()) {
        if (!runtime->is_key_down(VK_CONTROL)) {
            return false;
        }
    }

    if (hotKey.get_shift_required()) {
        if (!runtime->is_key_down(VK_SHIFT)) {
            return false;
        }
    }

    return true;
}

// Returns a list of toggled hotkeys.
std::map<shortcutType, bool> HotKeyManager::check_hot_keys(reshade::api::effect_runtime* runtime, SR::SRContext* context) {
    std::map<shortcutType, bool> toggled_hot_keys;

    // This might be pretty slow but for now it will do since we only ever have 3 hotkeys.
    for (int i = 0; i < registered_hot_keys.size(); i++) {
        if (runtime->is_key_pressed(registered_hot_keys[i].get_key())) {
            // Hotkey is being pressed, now check for the modifier keys.
            if (check_modifier_keys(registered_hot_keys[i], runtime)) {
                // All conditions are met, execute hotkey logic.
                registered_hot_keys[i].toggle_hot_key();
                toggled_hot_keys[registered_hot_keys[i].get_type()] = registered_hot_keys[i].get_enabled();
            }
        }
    }

    return toggled_hot_keys;
}

void HotKeyManager::edit_hot_key(uint8_t hot_key_id) {

}

void HotKeyManager::write_missing_hotkeys() {
    size_t value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_sr_key.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_sr_key.c_str(), "0x31\;ctrl");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_lens_key.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_lens_key.c_str(), "0x32\;ctrl");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_3d_key.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_3d_key.c_str(), "0x33\;ctrl");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_lens_and_3d_key.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_lens_and_3d_key.c_str(), "0x34\;ctrl");
    }
    value_size = 0;
    reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_latency_mode_key.c_str(), nullptr, &value_size);
    if (value_size <= 0) {
        reshade::set_config_value(nullptr, gb_config_section_name.c_str(), gb_config_toggle_latency_mode_key.c_str(), "0x35\;ctrl");
    }
}

void HotKeyManager::load_default_hotkeys() {
    registered_hot_keys.erase(registered_hot_keys.begin(), registered_hot_keys.end());
    registered_hot_keys.push_back(HotKey(true, shortcutType::TOGGLE_SR, 0x31, false, false, true));
    registered_hot_keys.push_back(HotKey(true, shortcutType::TOGGLE_LENS, 0x32, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_3D, 0x33, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_LENS_AND_3D, 0x34, false, false, true));
    registered_hot_keys.push_back(HotKey(false, shortcutType::TOGGLE_LATENCY_MODE, 0x35, false, false, true));
}
