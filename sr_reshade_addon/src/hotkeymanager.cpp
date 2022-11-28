#include "hotkeyManager.h"

HotKeyManager::HotKeyManager()
{
    //Todo: Could be prettier
    //Create some default hotkeys.
    HotKey toggleSRKey = HotKey(true, shortcutType::toggleSR, 0x31, false, false, true);
    HotKey toggleLensKey = HotKey(true, shortcutType::toggleLens, 0x32, false, false, true);
    HotKey toggle3D = HotKey(false, shortcutType::toggle3D, 0x33, false, false, true);
    HotKey toggleLensAnd3D = HotKey(false, shortcutType::toggleLensAnd3D, 0x34, false, false, true);
    registeredHotkeys.push_back(toggleSRKey);
    registeredHotkeys.push_back(toggleLensKey);
    registeredHotkeys.push_back(toggle3D);
    registeredHotkeys.push_back(toggleLensAnd3D);
}

bool checkModifierKeys(HotKey hotKey, reshade::api::effect_runtime* runtime) {
    if (hotKey.getAltRequired()) {
        if (!runtime->is_key_down(VK_MENU)) {
            return false;
        }
    }

    if (hotKey.getCtrlRequired()) {
        if (!runtime->is_key_down(VK_CONTROL)) {
            return false;
        }
    }

    if (hotKey.getShiftRequired()) {
        if (!runtime->is_key_down(VK_SHIFT)) {
            return false;
        }
    }

    return true;
}

//Returns a list of toggled hotkeys.
std::map<shortcutType, bool> HotKeyManager::checkHotKeys(reshade::api::effect_runtime* runtime, SR::SRContext* srContext) {
    std::map<shortcutType, bool> toggledHotKeys;

    //This might be pretty slow but for now it will do since we only ever have 3 hotkeys.
    for (int i = 0; i < registeredHotkeys.size(); i++) {
        if (runtime->is_key_pressed(registeredHotkeys[i].getKey())) {
            //Hotkey is being pressed, now check for the modifier keys.
            if (checkModifierKeys(registeredHotkeys[i], runtime)) {
                //All conditions are met, execute hotkey logic.
                registeredHotkeys[i].toggleHotKey();
                toggledHotKeys[registeredHotkeys[i].getType()] = registeredHotkeys[i].getEnabled();
            }
        }
    }

    return toggledHotKeys;
}

void HotKeyManager::editHotKey(uint8_t hotKeyId) {

}
