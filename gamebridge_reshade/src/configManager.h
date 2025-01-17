/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H
#include <string>

class ConfigManager {
public:
    class ConfigValue {
    public:
        std::string key;
        enum class Type { None, Int, String, Bool } value_type = Type::None;
        std::string string_value;
        int int_value;
        bool bool_value;

        ConfigValue(std::string key, int intValue) : key(key), value_type(Type::Int), int_value(intValue) {}
        ConfigValue(std::string key, const std::string& stringValue) : key(key), value_type(Type::String), string_value(stringValue) {}
        ConfigValue(std::string key, bool boolValue) : key(key), value_type(Type::Bool), bool_value(boolValue) {}
        ConfigValue() : value_type(Type::None) {}
    };

    std::vector<ConfigValue> registered_config_values;

    ConfigManager();

    static void write_missing_config_values();

    // Keys are stored in the config like so:
    // [3DGameBridge]
    // toggle_sr_key=0x31;shift;alt;ctrl
    // toggle_sr_key=0x31;ctrl
    // toggle_sr_key=0x31;
    // toggle_sr_key=0x31
    //
    // The following keys are available: toggle_sr_key, toggle_lens_key, toggle_3d_key, toggle_lens_and_3d_key, toggle_latency_mode_key
    //
    // Method to read a specific key from the ReShade config.
    // Returns an instance of the hotkey class based on the read config values.
    static ConfigValue read_from_config(const std::string& key);

    void load_default_config();
};



#endif //CONFIGMANAGER_H
