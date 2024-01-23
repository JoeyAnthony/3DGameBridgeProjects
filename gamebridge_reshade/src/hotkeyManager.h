#include "hotkey.h"
#include "pch.h"

#include <vector>
#include <iostream>

class HotKeyManager {
public:
    HotKeyManager();
    std::map<shortcutType, bool> check_hot_keys(reshade::api::effect_runtime* runtime, SR::SRContext* context);
    //Todo: Implement a way to edit and create now hotkeys?
    void edit_hot_key(uint8_t hotKeyId);

private:
    //Todo: We should define these in a .ini file eventually.
    std::vector<HotKey> registered_hot_keys;
};
