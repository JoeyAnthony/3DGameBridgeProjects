#include "hotkey.h"

HotKey::HotKey() {

}

void HotKey::set_toggle_key(uint8_t newKeyValue, shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->shortcut_key = newKeyValue;
    this->type = type;
    this->shift_required = shiftRequired;
    this->alt_required = altRequired;
    this->ctrl_required = ctrlRequired;
}

HotKey::HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->is_enabled = isEnabled;
    this->type = type;
    this->shortcut_key = key;
    this->shift_required = shiftRequired;
    this->alt_required = altRequired;
    this->ctrl_required = ctrlRequired;
}

bool HotKey::get_enabled()
{
    return is_enabled;
}

bool HotKey::get_shift_required()
{
    return shift_required;
}

bool HotKey::get_alt_required()
{
    return alt_required;
}

bool HotKey::get_ctrl_required()
{
    return ctrl_required;
}

shortcutType HotKey::get_type()
{
    return type;
}

uint8_t HotKey::get_key()
{
    return shortcut_key;
}

uint8_t HotKey::get_id()
{
    //Returns a "unique" integer.
    //Todo: A check should be performed when a new shortcut is added to ensure every id is indeed unique.
    return type + shortcut_key;
}
