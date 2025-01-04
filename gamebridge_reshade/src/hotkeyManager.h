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
    explicit HotKeyManager(reshade::api::effect_runtime* runtime);
    std::map<shortcutType, bool> check_hot_keys(reshade::api::effect_runtime* runtime, SR::SRContext* context);
    // Todo: Implement a way to edit and create new hotkeys?
    void edit_hot_key(uint8_t hot_key_id);

private:
    std::vector<HotKey> registered_hot_keys;

    // Writes hotkeys if they are not present in the ReShade.ini file
    static void write_missing_hotkeys();

    // Helper function to split off unwanted '\0' characters from vector arrays for easy processing.
    static void removeUnwantedNulls(std::vector<char>& vec);

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
    static HotKey read_from_config(bool default_enabled, const std::string& key, shortcutType shortcut);
};
