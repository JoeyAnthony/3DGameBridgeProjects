/*
This file (Plugin.cpp) is licensed under the MIT license and is separate from the rest of the UEVR codebase.

Copyright (c) 2023 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <sstream>
#include <mutex>
#include <memory>

#include <Windows.h>

#include <dxgi1_4.h>
#include <d3d12.h>

// only really necessary if you want to render to the screen
//#include "imgui/imgui_impl_dx11.h"
//#include "imgui/imgui_impl_dx12.h"
//#include "imgui/imgui_impl_win32.h"

//#include "rendering/d3d11.hpp"
#include "rendering/d3d12.hpp"

#include "uevr/Plugin.hpp"

#include <opencv2/core/cvstd.hpp>
#include <wrl/client.h>

#include <openxr/openxr.h>

using namespace uevr;

#define PLUGIN_LOG_ONCE(...) \
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_info(__VA_ARGS__); \
    }

class ExamplePlugin : public uevr::Plugin {
public:
    ExamplePlugin() = default;

    void on_dllmain() override {}

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmd_allocator{};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd_list{};

    enum class RTV : int {
        BACKBUFFER_0,
        BACKBUFFER_1,
        BACKBUFFER_2,
        BACKBUFFER_3,
        IMGUI,
        BLANK,
        COUNT,
    };

    enum class SRV : int {
        IMGUI_FONT,
        IMGUI,
        BLANK,
        COUNT
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_desc_heap{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_desc_heap{};
    Microsoft::WRL::ComPtr<ID3D12Resource> rts[(int)RTV::COUNT]{};

    auto& get_rt(RTV rtv) { return rts[(int)rtv]; }

    uint32_t rt_width{};
    uint32_t rt_height{};

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_rtv(ID3D12Device* device, RTV rtv) {
        return { rtv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                (int)rtv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
    }

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_srv(ID3D12Device* device, SRV srv) {
        return { srv_desc_heap->GetCPUDescriptorHandleForHeapStart().ptr +
                (int)srv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
    }

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_srv(ID3D12Device* device, SRV srv) {
        return { srv_desc_heap->GetGPUDescriptorHandleForHeapStart().ptr +
                (int)srv * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
    }

    bool initialize_dx12_members()
    {
        const auto renderer_data = uevr::API::get()->param()->renderer;
        auto device = (ID3D12Device*)renderer_data->device;
        auto swapchain = (IDXGISwapChain3*)renderer_data->swapchain;

        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&this->cmd_allocator)))) {
            return false;
        }

        if (FAILED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, this->cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&this->cmd_list)))) {
            return false;
        }

        if (FAILED(this->cmd_list->Close())) {
            return false;
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};

            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = (int)RTV::COUNT;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;

            if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->rtv_desc_heap)))) {
                return false;
            }
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};

            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = (int)SRV::COUNT;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->srv_desc_heap)))) {
                return false;
            }
        }

        {
            for (auto i = 0; i <= (int)RTV::BACKBUFFER_3; ++i) {
                if (SUCCEEDED(swapchain->GetBuffer(i, IID_PPV_ARGS(&this->rts[i])))) {
                    device->CreateRenderTargetView(this->rts[i].Get(), nullptr, this->get_cpu_rtv(device, (RTV)i));
                }
            }

            //// Create our imgui and blank rts.
            //auto& backbuffer = this->get_rt(RTV::BACKBUFFER_0);
            //auto desc = backbuffer->GetDesc();

            //D3D12_HEAP_PROPERTIES props{};
            //props.Type = D3D12_HEAP_TYPE_DEFAULT;
            //props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            //props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            //D3D12_CLEAR_VALUE clear_value{};
            //clear_value.Format = desc.Format;

            //if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
            //    IID_PPV_ARGS(&this->get_rt(RTV::IMGUI))))) {
            //    return false;
            //}

            //if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
            //    IID_PPV_ARGS(&this->get_rt(RTV::BLANK))))) {
            //    return false;
            //}

            //// Create imgui and blank rtvs and srvs.
            //device->CreateRenderTargetView(this->get_rt(RTV::IMGUI).Get(), nullptr, this->get_cpu_rtv(device, RTV::IMGUI));
            //device->CreateRenderTargetView(this->get_rt(RTV::BLANK).Get(), nullptr, this->get_cpu_rtv(device, RTV::BLANK));
            //device->CreateShaderResourceView(
            //    this->get_rt(RTV::IMGUI).Get(), nullptr, this->get_cpu_srv(device, SRV::IMGUI));
            //device->CreateShaderResourceView(this->get_rt(RTV::BLANK).Get(), nullptr, this->get_cpu_srv(device, SRV::BLANK));

            //this->rt_width = (uint32_t)desc.Width;
            //this->rt_height = (uint32_t)desc.Height;
        }

        auto& backbuffer = this->get_rt(RTV::BACKBUFFER_0);
        auto desc = backbuffer->GetDesc();

        return true;
    }

    void on_initialize() override {
        API::get()->log_error("%s %s", "Hello", "error");
        API::get()->log_warn("%s %s", "Hello", "warning");
        API::get()->log_info("%s %s", "Hello", "info");

        initialize_dx12_members();

        m_initialized = true;
    }

    void on_present() override {
        if (!m_initialized) {
            // initialize var we can use
        }

        const auto renderer_data = API::get()->param()->renderer;
        API::get()->param()->openxr->

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {

            // Render dx11 stuff
            
        } else if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            auto device = (ID3D12Device*)renderer_data->device;
            auto swapchain = (IDXGISwapChain3*)renderer_data->swapchain;
            auto command_queue = (ID3D12CommandQueue*)renderer_data->command_queue;

            if (command_queue == nullptr) {
                return;
            }

            // Render dx12 stuff

            auto& backbuffer = this->get_rt(RTV::BACKBUFFER_0);
            auto desc = backbuffer->GetDesc();

            // Reset command list
            cmd_allocator->Reset();
            cmd_list->Reset(cmd_allocator.Get(), nullptr);

            // Set back buffer as render target
            auto cpu_desc_handle = get_cpu_rtv(device, (RTV)swapchain->GetCurrentBackBufferIndex());
            cmd_list->OMSetRenderTargets(1, &cpu_desc_handle, FALSE, nullptr);

            // Clear framebuffer
            const float clearColor[] = { 0.5, 0.5, 0.5, 1 };
            cmd_list->ClearRenderTargetView(cpu_desc_handle, clearColor, 0, nullptr);

            cmd_list->Close();

            command_queue->ExecuteCommandLists(1, (ID3D12CommandList* const*)cmd_list.GetAddressOf());
        }
    }

    void on_device_reset() override {
        PLUGIN_LOG_ONCE("Example Device Reset");

        const auto renderer_data = API::get()->param()->renderer;

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D11) {
            // d3d11 device reset
        }

        if (renderer_data->renderer_type == UEVR_RENDERER_D3D12) {
            // d3d12 device reset
        }

        m_initialized = false;
    }

    bool on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override { 
        // Windows message callback

        return true;
    }

    void on_pre_engine_tick(UEVR_UGameEngineHandle engine, float delta) override {
        PLUGIN_LOG_ONCE("Pre Engine Tick: %f", delta);

        static bool once = true;

        if (once) {
            once = false;
            API::get()->sdk()->functions->execute_command(L"stat fps");
        }
    }

    void on_post_engine_tick(UEVR_UGameEngineHandle engine, float delta) override {
        PLUGIN_LOG_ONCE("Post Engine Tick: %f", delta);
    }

    void on_pre_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override {
        PLUGIN_LOG_ONCE("Pre Slate Draw Window");
    }

    void on_post_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override {
        PLUGIN_LOG_ONCE("Post Slate Draw Window");
    }

    void on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, 
                                             UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) override
    {
        PLUGIN_LOG_ONCE("Pre Calculate Stereo View Offset");

        auto rotationd = (UEVR_Rotatord*)rotation;

        // Decoupled pitch.
        if (!is_double) {
            rotation->pitch = 0.0f;
        } else {
            rotationd->pitch = 0.0;
        }
    }

    void on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, 
                                              UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double)
    {
        PLUGIN_LOG_ONCE("Post Calculate Stereo View Offset");
    }

    void on_pre_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
        PLUGIN_LOG_ONCE("Pre Viewport Client Draw");
    }

    void on_post_viewport_client_draw(UEVR_UGameViewportClientHandle viewport_client, UEVR_FViewportHandle viewport, UEVR_FCanvasHandle canvas) {
        PLUGIN_LOG_ONCE("Post Viewport Client Draw");
    }

private:


private:
    HWND m_wnd{};
    bool m_initialized{false};
};

// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<ExamplePlugin> g_plugin{new ExamplePlugin()};
