/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include "pch.h"
#include "VersionComparer.h"
#include "configManager.h"
#include "gbConstants.h"

#include <sr/version_c.h>

// List of color formats that require color linearization in order restore the correct gamma.
const std::list<reshade::api::format> srgb_color_formats {
    reshade::api::format::r8g8b8a8_unorm_srgb,
};

// framerateAdaptive = provide an amount of time in microseconds in between weave() calls.
// latencyInFrames = provide an amount of buffers or "frames" between the application and presenting to the screen.
// latencyInFramesAutomatic = gets the amount of buffers or "frames" from the backbuffer using the ReShade api every frame.
enum LatencyModes { FRAMERATE_ADAPTIVE, LATENCY_IN_FRAMES, LATENCY_IN_FRAMES_AUTOMATIC };
// success = the method completed as expected
// failure = the method failed in an unexpected and generic way
// dllNotLoaded = the method failed due to an SR DLL not being present during delayed loading
enum GbResult { SUCCESS, GENERAL_FAIL, DLL_NOT_LOADED };

struct Destroy_Resource_Data
{
    reshade::api::resource resource;
    uint32_t frames_alive = 0;
};

class IGraphicsApi {
private:
    bool user_presence_3d_toggle_checked = false;
    bool enable_overlay_workaround = true;
public:
    /// \brief Constructor (must be called before usage of this class)
    IGraphicsApi();

    /// \brief Default destructor
    virtual ~IGraphicsApi() = default;

    /// \brief ReShade version numbers read from the appropriate DLL. useful for when certain options only work in certain ReShade versions
    /// These values are set on DLL_ATTACH
    int32_t reshade_version_nr_major = 0;
    int32_t reshade_version_nr_minor = 0;
    int32_t reshade_version_nr_patch = 0;

    /// \brief Weaver eye tracking latency in microseconds. Defaults to 40000us which is tuned for 60Hz screens.
    /// \return The current weaver latency in us, only used when LatencyMode is set to FRAMERATE_ADAPTIVE
    uint32_t weaver_latency_in_us = 40000;

    /// \brief ReShade buffer format, used for debugging if we need to use SRGB or RGB buffers for our weaver
    /// \return The current ReShade buffer format for the active game
    reshade::api::format current_buffer_format = reshade::api::format::unknown;

    /// \brief A boolean used to determine if the logic for toggling the 3D automatically based on user presence observed by the eye tracker
    /// \return Whether the automatic 3D toggle is enabled or disabled
    bool is_user_presence_3d_toggle_checked();

    /// \brief A boolean used to determine if the logic that automatically disables weaving when the SR window is obstructed (for instance by an overlay) should be enabled or not.
    /// \return Whether the window obstruction logic is enabled or not.
    bool is_overlay_workaround_enabled();

    /// \brief A boolean used to determine if the weaver is initialized, this bool is managed by the graphics API internally but can be forced to false to re-initialize the weaver.
    /// \return Whether the weaver is initialized or not.
    bool weaver_initialized = false;

    /// \brief Concatenates the reshade_version_nr_major/minor/patch into one number
    /// \return A concatenated ReShade version code without periods. Example: 580 (instead of 5.8.0)
    int32_t get_concatinated_reshade_version();

    /// \brief Responsible for drawing debug information in the ImGUI UI
    /// \param runtime Represents the reshade effect runtime
    void draw_status_overlay(reshade::api::effect_runtime* runtime, const std::string& error_message = "");

    /// \brief Checks the current used version of SR, if it is above 1.30, we use latency_in_frames. Otherwise, we use a static latency of 40000 us
    void determine_default_latency_mode();

    /// \brief The main call responsible for weaving the image once ReShade is done drawing effects
    /// \param runtime Represents the reshade effect runtime
    /// \param cmd_list Represents the current command list from ReShade
    /// \param rtv Represents the current render target view
    /// \param rtv_srgb Represents the current render target view with the srgb color format
    /// \return An enum representing the state of the method's completion
    virtual GbResult on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) = 0;

    /// \brief Method responsible for initializing SR related variables required for weaving
    /// \param runtime Represents the reshade effect runtime
    virtual void on_init_effect_runtime(reshade::api::effect_runtime* runtime) = 0;

    /// \brief Sets the boolean that decides whether we are ready to weave or not.
    /// \param do_weave Sets the interal value for do_weave to true or false.
    virtual void do_weave(bool do_weave) = 0;

    /// \brief Allows user to get the current weaver latency mode
    /// \return An enum representing the current latency mode of the weaver, see LatencyModes enum for more info
    virtual LatencyModes get_latency_mode() = 0;

    IGraphicsApi(bool userPresence3DToggleChecked, bool enableOverlayWorkaround);

    /// \brief Sets the weaver latency mode to latency in frames.
    /// This does NOT look at the current framerate or frametime, it is best used when you are able to consistently reach your monitor's VSYNC
    /// Sets the latency mode to latencyInFramesAutomatic if number_of_frames is negative.
    /// \param number_of_frames The amount of frames or "buffers" between the application and the moment they are presented on-screen. If set to -1, this will use the back buffer count taken from the ReShade API automatically
    /// \return Returns true if the latency was succesfully set, returns false if its current latency mode does not permit it to set the latency.
    virtual bool set_latency_in_frames(int32_t number_of_frames) = 0;

    /// \brief Sets the weaver latency to a static number of microseconds
    /// This method can be called every frame with the current frametime
    /// The default is 40000 microseconds which is optimized for 60FPS VSYNCED gameplay
    /// \param frametime_in_microseconds The amount of microseconds it takes for a frame to be rendered and shown on screen. This is used for eye tracking prediction so accuracy is key.
    /// \return Returns true if the latency was succesfully set, returns false if its current latency mode does not permit it to set the latency.
    virtual bool set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) = 0;
};
