/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include "hotkey.h"
#include "pch.h"

#include <vector>

class HotKeyManager {
public:
    HotKeyManager();
    std::map<shortcutType, bool> check_hot_keys(reshade::api::effect_runtime* runtime, SR::SRContext* context);
    // Todo: Implement a way to edit and create now hotkeys?
    void edit_hot_key(uint8_t hot_key_id);

private:
    // Todo: We should define these in a .ini file eventually.
    std::vector<HotKey> registered_hot_keys;
};
