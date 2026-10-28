/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "displayxrGraphics.h"

#include <d3d11.h>
#include <vector>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

class DisplayXRGraphicsD3D11 : public DisplayXRGraphics {
    reshade::api::device* device;
    XrGraphicsBindingD3D11KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    std::vector<XrSwapchainImageD3D11KHR> images;

public:
    explicit DisplayXRGraphicsD3D11(reshade::api::device* device) : device(device) {}

    const char* extension_name() const override {
        return XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
    }

    bool check_requirements(XrInstance instance, XrSystemId system, PFN_xrGetInstanceProcAddr get_proc_addr) override {
        PFN_xrGetD3D11GraphicsRequirementsKHR get_requirements = nullptr;
        get_proc_addr(instance, "xrGetD3D11GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&get_requirements));
        if (get_requirements == nullptr) {
            return false;
        }
        XrGraphicsRequirementsD3D11KHR requirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        return XR_SUCCEEDED(get_requirements(instance, system, &requirements));
    }

    const void* session_binding(const void* next) override {
        binding.next = next;
        binding.device = reinterpret_cast<ID3D11Device*>(device->get_native());
        return &binding;
    }

    int64_t native_format(reshade::api::format format) const override {
        // ReShade format values are DXGI_FORMAT values
        return static_cast<int64_t>(format);
    }

    bool enumerate_images(XrSwapchain swapchain, PFN_xrGetInstanceProcAddr get_proc_addr, XrInstance instance) override {
        PFN_xrEnumerateSwapchainImages enumerate = nullptr;
        get_proc_addr(instance, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction*>(&enumerate));
        uint32_t count = 0;
        if (enumerate == nullptr || XR_FAILED(enumerate(swapchain, 0, &count, nullptr)) || count == 0) {
            return false;
        }
        images.assign(count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        return XR_SUCCEEDED(enumerate(swapchain, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
    }

    uint64_t image_handle(uint32_t image_index) const override {
        return image_index < images.size() ? reinterpret_cast<uint64_t>(images[image_index].texture) : 0;
    }

    void release_images() override {
        images.clear();
    }

    bool copy_to_image(reshade::api::effect_runtime*, reshade::api::command_list* cmd_list, reshade::api::resource back_buffer, uint32_t image_index) override {
        if (image_index >= images.size()) {
            return false;
        }
        // The immediate context executes in order, so the runtime sees the copy when it reads the image.
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(cmd_list->get_native());
        auto* source = reinterpret_cast<ID3D11Resource*>(back_buffer.handle);
        context->CopySubresourceRegion(images[image_index].texture, 0, 0, 0, 0, source, 0, nullptr);
        return true;
    }
};

DisplayXRGraphics* create_displayxr_graphics_d3d11(reshade::api::device* device) {
    return new DisplayXRGraphicsD3D11(device);
}
