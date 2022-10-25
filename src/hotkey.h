#include "pch.h"

#include <cstdint>
#include <windows.h>

class HotKey {
public:
    HotKey::HotKey();
    HotKey::HotKey(bool isEnabled, shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired);
    void toggleHotKey() { isEnabled = !isEnabled; }
    //Unused for now, can make the key editable with this later.
    void setToggleKey(uint8_t newKeyValue, shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired);

    bool getEnabled();
    bool getShiftRequired();
    bool getAltRequired();
    bool getCtrlRequired();
    shortcutType getType();
    uint8_t getKey();
    uint8_t getId();

private:
    bool isEnabled = true;
    bool shiftRequired = false;
    bool altRequired = false;
    bool ctrlRequired = true;
    shortcutType type = shortcutType::toggleSR;
    uint8_t shortcutKey = 0x00;
};