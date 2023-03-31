// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID unsigned long long // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle

// add headers that you want to pre-compile here
#include "framework.h"

// Reshade
#include <imgui/imgui.h>
#include <reshade/include/reshade.hpp>

// SR
#include "sr/sense/eyetracker/eyetracker.h"
#include "sr/sense/core/inputstream.h"
//#include "sr/sense/system/systemsense.h"
//#include "sr/sense/system/systemevent.h" //Systemsense causes many warnings
#include "sr/sense/display/switchablehint.h"
#include "sr/utility/exception.h"
#include "sr/types.h"

// Global shortcut definition
enum shortcutType { toggleSR, toggleLens, toggle3D, toggleLensAnd3D, toggleLatencyMode };

#endif //PCH_H
