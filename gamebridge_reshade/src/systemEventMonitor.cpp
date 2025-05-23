/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "systemEventMonitor.h"
#include <reshade/include/reshade.hpp>
#include <sr/sense/core/inputstream.h>
#include <sr/sense/system/systemeventlistener.h>
#include <sr/sense/system/systemeventstream.h>

// The accept function can process the system event data as soon as it becomes available
void SystemEventMonitor::accept(const SR::SystemEvent& frame) {
    switch (frame.eventType) {
        case SR_eventType::Info:
            reshade::log_message(reshade::log_level::info, "Info");
            break;
        case SR_eventType::ContextInvalid:
            contextValid = false;
            break;
        case SR_eventType::UserLost:
            isUserLost = true;
            break;
        case SR_eventType::UserFound:
            isUserLost = false;
            break;
        default:
            break;
    }
}
