#include "hotkeyManager.h"

HotKeyManager::HotKeyManager()
{
    //Todo: Could be prettier
    //Create some default hotkeys.
    HotKey toggleSRKey = HotKey(true, HotKey::shortcutType::toggleSR, 0x31, false, false, true);
    HotKey toggleLensKey = HotKey(true, HotKey::shortcutType::toggleLens, 0x32, false, false, true);
    HotKey flattenDepthMapKey = HotKey(false, HotKey::shortcutType::flattenDepthMap, 0x33, false, false, true);
    registeredHotkeys.push_back(toggleSRKey);
    registeredHotkeys.push_back(toggleLensKey);
    registeredHotkeys.push_back(flattenDepthMapKey);
}

//Todo: We might want to move this funtion to dllmain since we're passing the SRContext here for various functions.
void executeHotKeyFunctionByType(HotKey::shortcutType type, bool isBeingToggledOn, SR::SRContext* context) {
    switch (type) {
    case HotKey::shortcutType::toggleSR:
        if (isBeingToggledOn) {
            context = new SR::SRContext;
            context->initialize();
        }
        else {
            context->deleteSRContext(context);
            context->~SRContext();
        }
        break;
    case HotKey::shortcutType::toggleLens:
        //Todo: Implement lens toggling mechanism.
        break;
    case HotKey::shortcutType::flattenDepthMap:
        //Todo: Implement depth map flattening mechanism.
        break;
    default:
        break;
    }
}

bool checkModifierKeys(HotKey hotKey, reshade::api::effect_runtime* runtime) {
    if (hotKey.getAltRequired()) {
        if (!runtime->is_key_pressed(VK_MENU)) {
            return false;
        }
    }

    if (hotKey.getCtrlRequired()) {
        if (!runtime->is_key_pressed(VK_CONTROL)) {
            return false;
        }
    }

    if (hotKey.getShiftRequired()) {
        if (!runtime->is_key_pressed(VK_SHIFT)) {
            return false;
        }
    }

    return true;
}

void HotKeyManager::checkHotKeys(reshade::api::effect_runtime* runtime, SR::SRContext* srContext) {
    //This might be pretty slow but for now it will do since we only ever have 3 hotkeys.
    for (int i = 0; i < registeredHotkeys.size(); i++) {
        if (runtime->is_key_pressed(registeredHotkeys[i].getKey())) {
            //Hotkey is being pressed, now check for the modifier keys.
            if (checkModifierKeys(registeredHotkeys[i], runtime)) {
                //All conditions are met, execute hotkey logic.
                registeredHotkeys[i].toggleHotKey();
                executeHotKeyFunctionByType(registeredHotkeys[i].getHotKeyType(), registeredHotkeys[i].getHotKeyEnabled(), srContext);
            }
        }
    }
}

void HotKeyManager::editHotKey(uint8_t hotKeyId) {

}
