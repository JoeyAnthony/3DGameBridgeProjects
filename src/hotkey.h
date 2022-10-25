#include <cstdint>
#include <windows.h>

class HotKey {
public:
    enum shortcutType { toggleSR, toggleLens, flattenDepthMap };

    HotKey::HotKey();
    HotKey::HotKey(bool isEnabled, HotKey::shortcutType type, uint8_t key, bool shiftRequired, bool altRequired, bool ctrlRequired);
    void toggleHotKey() { isEnabled = !isEnabled; }
    //Unused for now, can make the key editable with this later.
    void setToggleKey(uint8_t newKeyValue, HotKey::shortcutType type, bool shiftRequired, bool altRequired, bool ctrlRequired);

    bool getHotKeyEnabled();
    bool getShiftRequired();
    bool getAltRequired();
    bool getCtrlRequired();
    HotKey::shortcutType getHotKeyType();
    uint8_t getKey();
    uint8_t getId();

private:
    bool isEnabled = true;
    bool shiftRequired = false;
    bool altRequired = false;
    bool ctrlRequired = true;
    HotKey::shortcutType type = HotKey::shortcutType::toggleSR;
    uint8_t shortcutKey = 0x00;
};