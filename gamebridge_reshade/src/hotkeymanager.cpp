/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkeyManager.h"

void HotKeyManager::removeUnwantedNulls(std::vector<char>& vec) {
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
    reshade::get_config_value(nullptr, "3DGameBridge", key.c_str(), nullptr, &value_size);
    if (value_size > 0) {
        HotKey created_key;
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
            created_key = HotKey(default_enabled, shortcut, found_key, str.find("shift") != std::string::npos, str.find("alt") != std::string::npos, str.find("ctrl") != std::string::npos);
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
}

HotKeyManager::HotKeyManager(reshade::api::effect_runtime* runtime) {
    // Create some default hotkeys.
    registered_hot_keys.push_back(read_from_config(true, "toggle_sr_key", TOGGLE_SR));
    registered_hot_keys.push_back(read_from_config(false, "toggle_lens_key", TOGGLE_LENS));
    registered_hot_keys.push_back(read_from_config(false, "toggle_3d_key", TOGGLE_3D));
    registered_hot_keys.push_back(read_from_config(false, "toggle_lens_and_3d_key", TOGGLE_LENS_AND_3D));
    registered_hot_keys.push_back(read_from_config(false, "toggle_latency_mode_key", TOGGLE_LATENCY_MODE));
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
