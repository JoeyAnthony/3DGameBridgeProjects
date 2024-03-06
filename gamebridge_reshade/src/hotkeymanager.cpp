/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkeyManager.h"

HotKeyManager::HotKeyManager() {
    // Todo: Could be prettier
    // Create some default hotkeys.
    HotKey toggle_sr_key = HotKey(true, shortcutType::TOGGLE_SR, 0x31, false, false, true);
    HotKey toggle_lens_key = HotKey(true, shortcutType::TOGGLE_LENS, 0x32, false, false, true);
    HotKey toggle_3d = HotKey(false, shortcutType::TOGGLE_3D, 0x33, false, false, true);
    HotKey toggle_lens_and_3d = HotKey(false, shortcutType::TOGGLE_LENS_AND_3D, 0x34, false, false, true);
    HotKey toggle_latency_mode = HotKey(false, shortcutType::TOGGLE_LATENCY_MODE, 0x35, false, false, true);
    registered_hot_keys.push_back(toggle_sr_key);
    registered_hot_keys.push_back(toggle_lens_key);
    registered_hot_keys.push_back(toggle_3d);
    registered_hot_keys.push_back(toggle_lens_and_3d);
    registered_hot_keys.push_back(toggle_latency_mode);
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
