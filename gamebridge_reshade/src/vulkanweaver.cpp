/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "vulkanweaver.h"

#include <vector>

VulkanWeaver::VulkanWeaver(SR::SRContext* context) {
    // Set context here.
    sr_context = context;
    weaving_enabled = true;
}

VkFormat VulkanWeaver::to_vk_format(reshade::api::format format) {
    // Typeless formats are created as UNORM by ReShade's Vulkan backend.
    switch (format) {
        case reshade::api::format::r8g8b8a8_typeless:
        case reshade::api::format::r8g8b8a8_unorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case reshade::api::format::r8g8b8a8_unorm_srgb:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case reshade::api::format::b8g8r8a8_typeless:
        case reshade::api::format::b8g8r8a8_unorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case reshade::api::format::b8g8r8a8_unorm_srgb:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case reshade::api::format::r10g10b10a2_typeless:
        case reshade::api::format::r10g10b10a2_unorm:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case reshade::api::format::b10g10r10a2_typeless:
        case reshade::api::format::b10g10r10a2_unorm:
            return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case reshade::api::format::r16g16b16a16_typeless:
        case reshade::api::format::r16g16b16a16_float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

bool VulkanWeaver::find_physical_device() {
    if (vk_physical_device != VK_NULL_HANDLE) {
        return true;
    }

    // Get the identifiers of the GPU the game is rendering with
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    char device_name[256] = {};
    vk_device->get_property(reshade::api::device_properties::vendor_id, &vendor_id);
    vk_device->get_property(reshade::api::device_properties::device_id, &device_id);
    vk_device->get_property(reshade::api::device_properties::description, device_name);

    // The adapter LUID also tells apart identical GPUs. Not every driver reports it, in which case we match on the IDs and name above.
    uint8_t adapter_luid[VK_LUID_SIZE] = {};
    const bool has_adapter_luid = vk_device->get_property(reshade::api::device_properties::adapter_luid, adapter_luid);

    // Create our own instance to enumerate the GPUs with, this is kept alive for as long as the weaver uses the physical device.
    if (vk_instance == VK_NULL_HANDLE) {
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "3DGameBridge";
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo instance_info = {};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;

        if (vkCreateInstance(&instance_info, nullptr, &vk_instance) != VK_SUCCESS) {
            vk_instance = VK_NULL_HANDLE;
            reshade::log::message(reshade::log::level::error, "Couldn't create a Vulkan instance to find the physical device with");
            return false;
        }
    }

    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());

    for (VkPhysicalDevice physical_device : physical_devices) {
        VkPhysicalDeviceIDProperties id_properties = {};
        id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &id_properties;
        vkGetPhysicalDeviceProperties2(physical_device, &properties);

        bool is_game_device;
        if (has_adapter_luid && id_properties.deviceLUIDValid) {
            is_game_device = std::memcmp(id_properties.deviceLUID, adapter_luid, VK_LUID_SIZE) == 0;
        } else {
            is_game_device = properties.properties.vendorID == vendor_id && properties.properties.deviceID == device_id && std::strcmp(properties.properties.deviceName, device_name) == 0;
        }

        if (is_game_device) {
            vk_physical_device = physical_device;
            break;
        }
    }

    if (vk_physical_device == VK_NULL_HANDLE) {
        reshade::log::message(reshade::log::level::error, "Couldn't find the Vulkan physical device used by the game");
        return false;
    }

    // Use the first queue family with graphics support, this is the same queue family ReShade uses for its command queue.
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, queue_families.data());
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vk_graphics_queue_family_index = i;
            break;
        }
    }

    return true;
}

GbResult VulkanWeaver::init_weaver(reshade::api::effect_runtime* runtime) {
    if (weaver_initialized) {
        return SUCCESS;
    }

    // Destroy the previous weaver when it is being re-initialized, wait for the GPU to finish using it first.
    if (weaver) {
        runtime->get_command_queue()->wait_idle();
        weaver->destroy();
        weaver = nullptr;
    }

    if (!find_physical_device()) {
        return GENERAL_FAIL;
    }

    VkDevice device = (VkDevice)vk_device->get_native();
    VkQueue graphics_queue = (VkQueue)runtime->get_command_queue()->get_native();

    // The weaver requires a command pool on the graphics queue family
    if (vk_command_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo command_pool_info = {};
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = vk_graphics_queue_family_index;
        if (vkCreateCommandPool(device, &command_pool_info, nullptr, &vk_command_pool) != VK_SUCCESS) {
            vk_command_pool = VK_NULL_HANDLE;
            reshade::log::message(reshade::log::level::info, "Couldn't create command pool");
            return GENERAL_FAIL;
        }
    }

    try {
        WeaverErrorCode result;
        if (is_overlay_workaround_enabled()) {
            result = SR::CreateVulkanWeaver(*sr_context, device, vk_physical_device, graphics_queue, vk_command_pool, nullptr, &weaver);
            if (result == WeaverSuccess) {
                weaver->setWindowHandle((HWND)runtime->get_hwnd());
            }
        } else {
            result = SR::CreateVulkanWeaver(*sr_context, device, vk_physical_device, graphics_queue, vk_command_pool, (HWND)runtime->get_hwnd(), &weaver);
        }

        if (result != WeaverSuccess) {
            weaver = nullptr;
            std::string message = "Couldn't create weaver, error code: " + std::to_string(result);
            reshade::log::message(reshade::log::level::error, message.c_str());
            return GENERAL_FAIL;
        }

        sr_context->initialize();
        reshade::log::message(reshade::log::level::info, "Initialized weaver");
    }
    catch (std::runtime_error &e) {
        if (std::strcmp(e.what(), "Failed to load library") == 0) {
            return DLL_NOT_LOADED;
        }
        return GENERAL_FAIL;
    }
    catch (std::exception &e) {
        reshade::log::message(reshade::log::level::info, e.what());
        return GENERAL_FAIL;
    }
    catch (...) {
        reshade::log::message(reshade::log::level::info, "Couldn't initialize weaver");
        return GENERAL_FAIL;
    }

    weaver_initialized = true;

    // Determine the default latency mode for the weaver
    determine_default_latency_mode();

    // The SR Vulkan weaver has no anti-crosstalk settings, this marks the option as unavailable in the overlay
    init_anti_crosstalk();

    return SUCCESS;
}

bool VulkanWeaver::create_effect_copy_buffer(const reshade::api::resource_desc& effect_resource_desc, reshade::api::format view_format) {
    // Create the copy as typeless so ReShade allows views with both the UNORM and SRGB variant of the format.
    reshade::api::resource_desc copy_rsc_desc(effect_resource_desc.texture.width, effect_resource_desc.texture.height, 1, 1, reshade::api::format_to_typeless(effect_resource_desc.texture.format), 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::shader_resource);
    if (!vk_device->create_resource(copy_rsc_desc, nullptr, reshade::api::resource_usage::shader_resource, &effect_frame_copy)) {
        destroy_effect_copy_buffer();
        reshade::log::message(reshade::log::level::warning, "Failed creating the effect frame copy");
        return false;
    }

    // Make shader resource view for the effect frame copy, the weaver reads from this view.
    reshade::api::resource_view_desc srv_desc(view_format);
    if (!vk_device->create_resource_view(effect_frame_copy, reshade::api::resource_usage::shader_resource, srv_desc, &effect_frame_copy_srv)) {
        destroy_effect_copy_buffer();
        reshade::log::message(reshade::log::level::warning, "Failed creating the effect frame copy's resource view");
        return false;
    }

    effect_frame_copy_x = effect_resource_desc.texture.width;
    effect_frame_copy_y = effect_resource_desc.texture.height;

    return true;
}

void VulkanWeaver::destroy_effect_copy_buffer() {
    if (effect_frame_copy_srv.handle != 0) {
        vk_device->destroy_resource_view(effect_frame_copy_srv);
        effect_frame_copy_srv = {};
    }
    if (effect_frame_copy.handle != 0) {
        vk_device->destroy_resource(effect_frame_copy);
        effect_frame_copy = {};
    }

    effect_frame_copy_x = 0;
    effect_frame_copy_y = 0;
}

VkFramebuffer VulkanWeaver::get_output_framebuffer(reshade::api::resource_view back_buffer_rtv, uint32_t width, uint32_t height) {
    VkDevice device = (VkDevice)vk_device->get_native();
    const uint64_t back_buffer_image = vk_device->get_resource_from_view(back_buffer_rtv).handle;

    // The weaver begins its own render pass on this framebuffer, the render pass we create it with only has to be compatible with that one.
    if (vk_output_render_pass == VK_NULL_HANDLE) {
        vk_output_format = to_vk_format(vk_device->get_resource_view_desc(back_buffer_rtv).format);
        if (vk_output_format == VK_FORMAT_UNDEFINED) {
            reshade::log::message(reshade::log::level::error, "Unsupported back buffer format for Vulkan weaving");
            return VK_NULL_HANDLE;
        }

        VkAttachmentDescription color_attachment = {};
        color_attachment.format = vk_output_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &render_pass_info, nullptr, &vk_output_render_pass) != VK_SUCCESS) {
            vk_output_render_pass = VK_NULL_HANDLE;
            reshade::log::message(reshade::log::level::error, "Couldn't create the render pass for the weaver output");
            return VK_NULL_HANDLE;
        }
    }

    // There is one framebuffer per swap chain image, reuse it when we already created one for this view.
    auto cached = output_framebuffers.find(back_buffer_rtv.handle);
    if (cached != output_framebuffers.end()) {
        if (cached->second.back_buffer_image == back_buffer_image) {
            return cached->second.framebuffer;
        }

        // The swap chain was recreated and the view handle was reused for a new image. ReShade already waited for the GPU before destroying the old view.
        vkDestroyFramebuffer(device, cached->second.framebuffer, nullptr);
        output_framebuffers.erase(cached);
    }

    VkImageView attachment = (VkImageView)back_buffer_rtv.handle;
    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = vk_output_render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = &attachment;
    framebuffer_info.width = width;
    framebuffer_info.height = height;
    framebuffer_info.layers = 1;

    Output_Framebuffer output_framebuffer;
    output_framebuffer.back_buffer_image = back_buffer_image;
    if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &output_framebuffer.framebuffer) != VK_SUCCESS) {
        reshade::log::message(reshade::log::level::error, "Couldn't create the framebuffer for the weaver output");
        return VK_NULL_HANDLE;
    }

    output_framebuffers[back_buffer_rtv.handle] = output_framebuffer;
    return output_framebuffer.framebuffer;
}

void VulkanWeaver::destroy_output_framebuffers() {
    VkDevice device = (VkDevice)vk_device->get_native();

    for (auto& entry : output_framebuffers) {
        vkDestroyFramebuffer(device, entry.second.framebuffer, nullptr);
    }
    output_framebuffers.clear();

    if (vk_output_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, vk_output_render_pass, nullptr);
        vk_output_render_pass = VK_NULL_HANDLE;
    }
}

GbResult VulkanWeaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    // Both views point to the same back buffer image.
    reshade::api::resource rtv_resource = vk_device->get_resource_from_view(rtv);
    reshade::api::resource_desc desc = vk_device->get_resource_desc(rtv_resource);
    const bool back_buffer_changed = desc.texture.width != effect_frame_copy_x || desc.texture.height != effect_frame_copy_y || desc.texture.format != current_buffer_format;

    if (!weaver_initialized || back_buffer_changed) {
        // Update current buffer format
        current_buffer_format = desc.texture.format;

        // Update color format settings
        check_color_format(desc);
    }

    reshade::api::resource_view chosen_rtv;

    if (use_srgb_rtv) {
        chosen_rtv = rtv_srgb;
    }
    else {
        chosen_rtv = rtv;
    }

    const reshade::api::format view_format = vk_device->get_resource_view_desc(chosen_rtv).format;

    if (weaver_initialized) {
        // Check if we need to set the latency in frames.
        if (get_latency_mode() == LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC) {
            weaver->setLatencyInFrames(runtime->get_back_buffer_count()); // Set the latency with which the weaver should do prediction.
        }

        // Check texture size and format
        if (back_buffer_changed) {
            // Destroy the resources only when the GPU is finished drawing.
            runtime->get_command_queue()->wait_idle();
            destroy_output_framebuffers();
            destroy_effect_copy_buffer();
            if (!create_effect_copy_buffer(desc, view_format) && !resize_buffer_failed) {
                reshade::log::message(reshade::log::level::warning, "Couldn't create effect copy buffer, trying again next frame");
                resize_buffer_failed = true;
            }
            reshade::log::message(reshade::log::level::info, "Buffer size changed");
        }
        else {
            resize_buffer_failed = false;

            if (weaving_enabled) {
                VkFramebuffer output_framebuffer = get_output_framebuffer(chosen_rtv, desc.texture.width, desc.texture.height);
                if (output_framebuffer == VK_NULL_HANDLE) {
                    return GENERAL_FAIL;
                }

                // Create copy of the effect buffer
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
                cmd_list->copy_resource(rtv_resource, effect_frame_copy);
                cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);
                cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);

                // Weave to back buffer, the input is a side-by-side image so a single view is half the width.
                const RECT weave_rect = { 0, 0, static_cast<LONG>(desc.texture.width), static_cast<LONG>(desc.texture.height) };
                const VkFormat vk_view_format = to_vk_format(view_format);
                weaver->setCommandBuffer((VkCommandBuffer)cmd_list->get_native());
                weaver->setViewport(weave_rect);
                weaver->setScissorRect(weave_rect);
                weaver->setInputViewTexture((VkImageView)effect_frame_copy_srv.handle, static_cast<int>(desc.texture.width / 2), static_cast<int>(desc.texture.height), vk_view_format);
                weaver->setOutputFrameBuffer(output_framebuffer, static_cast<int>(desc.texture.width), static_cast<int>(desc.texture.height), vk_view_format);
                weaver->weave();

                // Todo: weaveSubmitted() is only required for late latching, which we don't enable. ReShade submits the command list itself after presenting, so we can't call it at the right moment yet.
            }
        }
    }
    else {
        if (!create_effect_copy_buffer(desc, view_format)) {
            return GENERAL_FAIL;
        }

        GbResult result = init_weaver(runtime);
        if (result == DLL_NOT_LOADED) {
            return DLL_NOT_LOADED;
        }
        else if (result != SUCCESS) {
            // When buffer creation succeeds and this fails, delete the created buffer after waiting for the GPU to finish.
            runtime->get_command_queue()->wait_idle();
            destroy_effect_copy_buffer();
            reshade::log::message(reshade::log::level::info, "Failed to initialize weaver");
            return GENERAL_FAIL;
        }
    }
    return SUCCESS;
}

void VulkanWeaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    vk_device = runtime->get_device();
}

void VulkanWeaver::do_weave(bool do_weave) {
    weaving_enabled = do_weave;
}

bool VulkanWeaver::set_latency_in_frames(int32_t number_of_frames) {
    if (weaver_initialized) {
        if (number_of_frames < 0) {
            set_latency_mode(LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC);
        }
        else {
            set_latency_mode(LatencyModes::LATENCY_IN_FRAMES);
            weaver->setLatencyInFrames(number_of_frames);
        }
        return true;
    }
    return false;
}

bool VulkanWeaver::set_latency_frametime_adaptive(uint32_t frametime_in_microseconds) {
    if (weaver_initialized) {
        set_latency_mode(LatencyModes::FRAMERATE_ADAPTIVE);
        weaver->setLatency(frametime_in_microseconds);
        weaver_latency_in_us = frametime_in_microseconds;
        return true;
    }
    return false;
}

void VulkanWeaver::set_latency_mode(LatencyModes mode) {
    current_latency_mode = mode;
}

void VulkanWeaver::check_color_format(reshade::api::resource_desc desc) {
    // Check buffer format and see if it's in the list of known SRGB ones. Change to SRGB rtv if so.
    if ((std::find(srgb_color_formats.begin(), srgb_color_formats.end(), desc.texture.format) != srgb_color_formats.end())) {
        // SRGB format detected, switch to SRGB buffer.
        use_srgb_rtv = true;
    }
    else {
        use_srgb_rtv = false;
    }
}

LatencyModes VulkanWeaver::get_latency_mode() {
    return current_latency_mode;
}
