#include "hotkey.h"

HotKey::HotKey() {

}

void HotKey::setToggleKey(uint8_t newKeyValue, HotKey::shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->shortcutKey = newKeyValue;
    this->type = type;
    this->shiftRequired = shiftRequired;
    this->altRequired = altRequired;
    this->ctrlRequired = ctrlRequired;
}

HotKey::HotKey(bool isEnabled, HotKey::shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->isEnabled = isEnabled;
    this->type = type;
    this->shortcutKey = key;
    this->shiftRequired = shiftRequired;
    this->altRequired = altRequired;
    this->ctrlRequired = ctrlRequired;
}

bool HotKey::getHotKeyEnabled()
{
    return isEnabled;
}

bool HotKey::getShiftRequired()
{
    return shiftRequired;
}

bool HotKey::getAltRequired()
{
    return altRequired;
}

bool HotKey::getCtrlRequired()
{
    return ctrlRequired;
}

HotKey::shortcutType HotKey::getHotKeyType()
{
    return type;
}

uint8_t HotKey::getKey()
{
    return shortcutKey;
}

uint8_t HotKey::getId()
{
    //Returns a "unique" integer.
    //Todo: A check should be performed when a new shortcut is added to ensure every id is indeed unique.
    return type + shortcutKey;
}
