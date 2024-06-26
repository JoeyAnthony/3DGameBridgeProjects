/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkey.h"

HotKey::HotKey() {

}

void HotKey::set_toggle_key(uint8_t new_key_value, shortcutType type, bool shift_required, bool alt_required, bool ctrl_required) {
    this->shortcut_key = new_key_value;
    this->type = type;
    this->shift_required = shift_required;
    this->alt_required = alt_required;
    this->ctrl_required = ctrl_required;
}

HotKey::HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shift_required, bool alt_required, bool ctrl_required) {
    this->is_enabled = isEnabled;
    this->type = type;
    this->shortcut_key = key;
    this->shift_required = shift_required;
    this->alt_required = alt_required;
    this->ctrl_required = ctrl_required;
}

bool HotKey::get_enabled() {
    return is_enabled;
}

bool HotKey::get_shift_required() {
    return shift_required;
}

bool HotKey::get_alt_required() {
    return alt_required;
}

bool HotKey::get_ctrl_required() {
    return ctrl_required;
}

shortcutType HotKey::get_type() {
    return type;
}

uint8_t HotKey::get_key() {
    return shortcut_key;
}

uint8_t HotKey::get_id() {
    // Returns a "unique" integer.
    // Todo: A check should be performed when a new shortcut is added to ensure every id is indeed unique.
    return type + shortcut_key;
}
