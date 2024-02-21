/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include "pch.h"

#include <cstdint>

class HotKey {
public:
    HotKey();
    HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shift_required, bool alt_required, bool ctrl_required);
    void toggle_hot_key() { is_enabled = !is_enabled; }
    // Unused for now, can make the key editable with this later.
    void set_toggle_key(uint8_t new_key_value, shortcutType type, bool shift_required, bool alt_required, bool ctrl_required);

    bool get_enabled();
    bool get_shift_required();
    bool get_alt_required();
    bool get_ctrl_required();
    shortcutType get_type();
    uint8_t get_key();
    uint8_t get_id();

private:
    bool is_enabled = true;
    bool shift_required = false;
    bool alt_required = false;
    bool ctrl_required = true;
    shortcutType type = shortcutType::TOGGLE_SR;
    uint8_t shortcut_key = 0x00;
};
