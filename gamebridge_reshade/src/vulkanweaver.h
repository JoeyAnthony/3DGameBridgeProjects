/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

// Weaver
#include <vulkan/vulkan.h>
#include "sr/weaver/vkweaver.h"
// srReshade
#include "igraphicsapi.h"
#include "pch.h"

#include <unordered_map>

class VulkanWeaver: public IGraphicsApi {
    /// \brief A framebuffer wrapping one back buffer image view, the weaver renders into this.
    struct Output_Framebuffer {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        uint64_t back_buffer_image = 0;
    };

    uint32_t effect_frame_copy_x = 0, effect_frame_copy_y = 0;

    bool weaving_enabled = false;
    bool resize_buffer_failed = false;
    bool use_srgb_rtv = false;

    SR::SRContext* sr_context;
    SR::IVulkanWeaver1* weaver = nullptr;

    reshade::api::device* vk_device = nullptr;
    reshade::api::resource effect_frame_copy{};
    reshade::api::resource_view effect_frame_copy_srv{};

    // Vulkan objects required by the SR weaver that ReShade does not expose to add-ons.
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    uint32_t vk_graphics_queue_family_index = 0;
    VkCommandPool vk_command_pool = VK_NULL_HANDLE;
    VkRenderPass vk_output_render_pass = VK_NULL_HANDLE;
    VkFormat vk_output_format = VK_FORMAT_UNDEFINED;
    std::unordered_map<uint64_t, Output_Framebuffer> output_framebuffers;

    LatencyModes current_latency_mode = LatencyModes::FRAMERATE_ADAPTIVE;

    /// \brief Private function to set the internal latency mode of the weaver
    /// \param mode The latency mode to persist
    void set_latency_mode(LatencyModes mode);

    /// \brief Checks the current color format of the RTV's back buffer and chooses the correct buffer from ReShade for weaving based on that.
    /// \param desc Represents the current ReShade resource view
    void check_color_format(reshade::api::resource_desc desc);

    /// \brief Finds the VkPhysicalDevice and graphics queue family belonging to the game's VkDevice.
    /// ReShade does not expose these, so we enumerate the GPUs on our own VkInstance and match them on vendor ID, device ID and name.
    /// \return A bool representing if a matching physical device was found
    bool find_physical_device();

    /// \brief Returns a framebuffer for the given back buffer view that the weaver can render into, creating it when needed.
    /// \param back_buffer_rtv Represents the back buffer render target view to weave to
    /// \param width Width of the back buffer
    /// \param height Height of the back buffer
    /// \return The framebuffer to weave to or VK_NULL_HANDLE when it could not be created
    VkFramebuffer get_output_framebuffer(reshade::api::resource_view back_buffer_rtv, uint32_t width, uint32_t height);

    /// \brief Destroys all cached output framebuffers and the render pass they were created with.
    void destroy_output_framebuffers();

public:
    /// \brief Explicit constructor
    /// \param context Pointer to an already initialized SRContext
    explicit VulkanWeaver(SR::SRContext* context);

    /// \brief Initialized the SR weaver appropriate for the graphics API
    /// \param runtime Represents the reshade effect runtime
    /// \return An enum representing the return code
    GbResult init_weaver(reshade::api::effect_runtime* runtime);

    /// \brief Creates and reset the effect copy resource so it is similar to the back buffer resource, then use it as weaver input.
    /// \param effect_resource_desc ReShade resource representing the currently selected back buffer description
    /// \param view_format The format of the back buffer view that is woven to
    /// \return A bool respresenting if the effect frame copy was successful
    bool create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc, reshade::api::format view_format);

    /// \brief Destroys the effect frame copy and its shader resource view.
    void destroy_effect_copy_buffer();

    /// \brief Converts the ReShade formats commonly used for swap chains to their Vulkan counterpart.
    /// \param format The ReShade format to convert
    /// \return The matching VkFormat or VK_FORMAT_UNDEFINED if the format is not supported
    static VkFormat to_vk_format(reshade::api::format format);

    // Inherited via IGraphicsApi
    GbResult on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) override;
    void on_init_effect_runtime(reshade::api::effect_runtime* runtime) override;
    void do_weave(bool do_weave) override;
    bool set_latency_in_frames(int32_t number_of_frames) override;
    bool set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) override;
    LatencyModes get_latency_mode() override;
};
