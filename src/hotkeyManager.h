#include "hotkey.h"
#include "pch.h"

#include <vector>
#include <iostream>

class HotKeyManager {
public:
    HotKeyManager::HotKeyManager();
    void checkHotKeys(reshade::api::effect_runtime* runtime, SR::SRContext* context);
    //Todo: Implement a way to edit and create now hotkeys?
    void editHotKey(uint8_t hotKeyId);

private:
    //Todo: We should define these in a .ini file eventually.
    std::vector<HotKey> registeredHotkeys;
};