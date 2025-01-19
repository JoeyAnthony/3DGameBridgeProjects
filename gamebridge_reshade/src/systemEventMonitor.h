/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#ifndef SYSTEMEVENTMONITOR_H
#define SYSTEMEVENTMONITOR_H
#include <sr/sense/core/inputstream.h>
#include <sr/sense/system/systemevent.h>
#include <sr/sense/system/systemeventlistener.h>
#include <sr/sense/system/systemeventstream.h>

class SystemEventMonitor : public SR::SystemEventListener {
public:
    virtual ~SystemEventMonitor() = default;

    // Ensures SystemEventStream is cleaned up when Listener object is out of scope
    SR::InputStream<SR::SystemEventStream> stream;

    // Flag to indicate that context is no longer valid
    bool contextValid = true;
    bool isUserLost = false;

    void accept(const SR::SystemEvent& frame) override;
};

#endif //SYSTEMEVENTMONITOR_H
