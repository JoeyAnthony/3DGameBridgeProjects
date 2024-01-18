/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "hotkey.h"

HotKey::HotKey() {

}

void HotKey::setToggleKey(uint8_t newKeyValue, shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->shortcutKey = newKeyValue;
    this->type = type;
    this->shiftRequired = shiftRequired;
    this->altRequired = altRequired;
    this->ctrlRequired = ctrlRequired;
}

HotKey::HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired) {
    this->isEnabled = isEnabled;
    this->type = type;
    this->shortcutKey = key;
    this->shiftRequired = shiftRequired;
    this->altRequired = altRequired;
    this->ctrlRequired = ctrlRequired;
}

bool HotKey::getEnabled()
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

shortcutType HotKey::getType()
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
