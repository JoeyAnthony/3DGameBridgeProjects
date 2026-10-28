/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include "pch.h"

#include <openxr/openxr.h>

// The graphics-API half of the DisplayXR backend.
// DisplayXRBackend owns the OpenXR instance, session and frame loop, which are the same for every graphics API.
// Everything that differs per API (the session graphics binding, the swapchain image type and the copy of the
// game's frame into a swapchain image) lives behind this interface, so a Vulkan path (XR_KHR_vulkan_enable2)
// only needs one more implementation.
class DisplayXRGraphics {
public:
    virtual ~DisplayXRGraphics() = default;

    /// \brief The OpenXR graphics extension this binding needs, e.g. XR_KHR_D3D11_enable
    virtual const char* extension_name() const = 0;

    /// \brief Calls the xrGet*GraphicsRequirementsKHR function of the extension. OpenXR requires this before xrCreateSession.
    virtual bool check_requirements(XrInstance instance, XrSystemId system, PFN_xrGetInstanceProcAddr get_proc_addr) = 0;

    /// \brief Returns the graphics binding to chain on XrSessionCreateInfo::next. The binding chains `next` behind itself.
    virtual const void* session_binding(const void* next) = 0;

    /// \brief Converts a ReShade format to the native format value used for swapchain formats (DXGI_FORMAT, VkFormat)
    virtual int64_t native_format(reshade::api::format format) const = 0;

    /// \brief Enumerates the images of a newly created swapchain
    virtual bool enumerate_images(XrSwapchain swapchain, PFN_xrGetInstanceProcAddr get_proc_addr, XrInstance instance) = 0;

    /// \brief The native handle of a swapchain image (ID3D11Texture2D*, ID3D12Resource*), usable as a ReShade resource
    virtual uint64_t image_handle(uint32_t image_index) const = 0;

    /// \brief Forgets the images of the current swapchain, before it is destroyed
    virtual void release_images() = 0;

    /// \brief Copies the game's back buffer into a swapchain image.
    /// The copy must be submitted to the GPU queue before this returns, since the runtime reads the image on that
    /// queue right after xrReleaseSwapchainImage.
    /// \param runtime Represents the reshade effect runtime
    /// \param cmd_list Represents the current command list from ReShade
    /// \param back_buffer The back buffer holding the side-by-side frame
    /// \param image_index Index of the acquired swapchain image
    virtual bool copy_to_image(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource back_buffer, uint32_t image_index) = 0;
};

DisplayXRGraphics* create_displayxr_graphics_d3d11(reshade::api::device* device);
DisplayXRGraphics* create_displayxr_graphics_d3d12(reshade::api::device* device, reshade::api::command_queue* queue);
