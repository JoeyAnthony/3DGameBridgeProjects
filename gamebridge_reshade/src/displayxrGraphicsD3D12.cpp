/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "displayxrGraphics.h"

#include <d3d12.h>
#include <vector>

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

class DisplayXRGraphicsD3D12 : public DisplayXRGraphics {
    reshade::api::device* device;
    reshade::api::command_queue* queue;
    XrGraphicsBindingD3D12KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
    std::vector<XrSwapchainImageD3D12KHR> images;

    // The copy is recorded on our own command list: ReShade's command list only executes at present, which is
    // after the runtime has already read the swapchain image.
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fence_event = nullptr;
    uint64_t fence_value = 0;

    void wait_for_last_copy() {
        if (fence != nullptr && fence->GetCompletedValue() < fence_value) {
            fence->SetEventOnCompletion(fence_value, fence_event);
            WaitForSingleObject(fence_event, INFINITE);
        }
    }

    bool create_copy_objects() {
        auto* dev = reinterpret_cast<ID3D12Device*>(device->get_native());
        if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
            FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list))) ||
            FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
            return false;
        }
        list->Close();
        fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        return fence_event != nullptr;
    }

    static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        return barrier;
    }

public:
    DisplayXRGraphicsD3D12(reshade::api::device* device, reshade::api::command_queue* queue) : device(device), queue(queue) {}

    ~DisplayXRGraphicsD3D12() override {
        wait_for_last_copy();
        if (list) list->Release();
        if (allocator) allocator->Release();
        if (fence) fence->Release();
        if (fence_event) CloseHandle(fence_event);
    }

    const char* extension_name() const override {
        return XR_KHR_D3D12_ENABLE_EXTENSION_NAME;
    }

    bool check_requirements(XrInstance instance, XrSystemId system, PFN_xrGetInstanceProcAddr get_proc_addr) override {
        PFN_xrGetD3D12GraphicsRequirementsKHR get_requirements = nullptr;
        get_proc_addr(instance, "xrGetD3D12GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&get_requirements));
        if (get_requirements == nullptr) {
            return false;
        }
        XrGraphicsRequirementsD3D12KHR requirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
        return XR_SUCCEEDED(get_requirements(instance, system, &requirements)) && create_copy_objects();
    }

    const void* session_binding(const void* next) override {
        binding.next = next;
        binding.device = reinterpret_cast<ID3D12Device*>(device->get_native());
        binding.queue = reinterpret_cast<ID3D12CommandQueue*>(queue->get_native());
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
        images.assign(count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
        return XR_SUCCEEDED(enumerate(swapchain, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
    }

    uint64_t image_handle(uint32_t image_index) const override {
        return image_index < images.size() ? reinterpret_cast<uint64_t>(images[image_index].texture) : 0;
    }

    void release_images() override {
        wait_for_last_copy();
        images.clear();
    }

    bool copy_to_image(reshade::api::effect_runtime* runtime, reshade::api::command_list*, reshade::api::resource back_buffer, uint32_t image_index) override {
        if (image_index >= images.size()) {
            return false;
        }

        // Run the effects ReShade recorded so far, so the back buffer holds the finished frame when our copy executes.
        runtime->get_command_queue()->flush_immediate_command_list();

        wait_for_last_copy();
        allocator->Reset();
        list->Reset(allocator, nullptr);

        // OpenXR hands out D3D12 color swapchain images in the RENDER_TARGET state and expects them back in it.
        // ReShade keeps the back buffer in RENDER_TARGET while effects run.
        auto* source = reinterpret_cast<ID3D12Resource*>(back_buffer.handle);
        ID3D12Resource* target = images[image_index].texture;
        D3D12_RESOURCE_BARRIER before[2] = {
            transition(source, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
            transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
        };
        list->ResourceBarrier(2, before);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = target;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = source;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER after[2] = {
            transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            transition(target, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET),
        };
        list->ResourceBarrier(2, after);
        list->Close();

        auto* native_queue = reinterpret_cast<ID3D12CommandQueue*>(queue->get_native());
        ID3D12CommandList* lists[] = { list };
        native_queue->ExecuteCommandLists(1, lists);
        native_queue->Signal(fence, ++fence_value);
        return true;
    }
};

DisplayXRGraphics* create_displayxr_graphics_d3d12(reshade::api::device* device, reshade::api::command_queue* queue) {
    return new DisplayXRGraphicsD3D12(device, queue);
}
