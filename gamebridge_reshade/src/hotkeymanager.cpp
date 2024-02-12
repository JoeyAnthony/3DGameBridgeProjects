/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkeyManager.h"

HotKeyManager::HotKeyManager()
{
    //Todo: Could be prettier
    //Create some default hotkeys.
    HotKey toggleSRKey = HotKey(true, shortcutType::toggle_SR, 0x31, false, false, true);
    HotKey toggleLensKey = HotKey(true, shortcutType::toggle_lens, 0x32, false, false, true);
    HotKey toggle3D = HotKey(false, shortcutType::toggle_3D, 0x33, false, false, true);
    HotKey toggleLensAnd3D = HotKey(false, shortcutType::toggle_lens_and_3D, 0x34, false, false, true);
    HotKey toggleLatencyMode = HotKey(false, shortcutType::toggle_latency_mode, 0x35, false, false, true);
    HotKey toggleWeaveOnShader = HotKey(false, shortcutType::toggle_weave_on_shader, 0x36, false, false, true);
    registered_hot_keys.push_back(toggleSRKey);
    registered_hot_keys.push_back(toggleLensKey);
    registered_hot_keys.push_back(toggle3D);
    registered_hot_keys.push_back(toggleLensAnd3D);
    registered_hot_keys.push_back(toggleLatencyMode);
    registered_hot_keys.push_back(toggleWeaveOnShader);
}

bool checkModifierKeys(HotKey hotKey, reshade::api::effect_runtime* runtime) {
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

//Returns a list of toggled hotkeys.
std::map<shortcutType, bool> HotKeyManager::check_hot_keys(reshade::api::effect_runtime* runtime, SR::SRContext* context) {
    std::map<shortcutType, bool> toggledHotKeys;

    //This might be pretty slow but for now it will do since we only ever have 3 hotkeys.
    for (int i = 0; i < registered_hot_keys.size(); i++) {
        if (runtime->is_key_pressed(registered_hot_keys[i].get_key())) {
            //Hotkey is being pressed, now check for the modifier keys.
            if (checkModifierKeys(registered_hot_keys[i], runtime)) {
                //All conditions are met, execute hotkey logic.
                registered_hot_keys[i].toggle_hot_key();
                toggledHotKeys[registered_hot_keys[i].get_type()] = registered_hot_keys[i].get_enabled();
            }
        }
    }

    return toggledHotKeys;
}

void HotKeyManager::edit_hot_key(uint8_t hotKeyId) {

}
