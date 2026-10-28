/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "displayxrBackend.h"
#include "configManager.h"
#include "gbConstants.h"

#include <openxr/openxr_loader_negotiation.h>
#include <openxr/XR_DXR_win32_window_binding.h>
#include <openxr/XR_DXR_display_info.h>
#include <openxr/XR_DXR_swapchain_color_encoding.h>

#include <d3dcommon.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

static std::atomic<bool> teardown_abandoned{false};

static void log_info(const std::string& message) {
    reshade::log_message(reshade::log_level::info, ("DisplayXR: " + message).c_str());
}

static void log_warning(const std::string& message) {
    reshade::log_message(reshade::log_level::warning, ("DisplayXR: " + message).c_str());
}

// Logs a failed OpenXR call once per call site and result, so a failing frame loop is visible without flooding the log.
static void log_failure_once(const char* call, XrResult result) {
    static std::vector<std::pair<const char*, XrResult>> logged;
    for (const auto& entry : logged) {
        if (entry.first == call && entry.second == result) {
            return;
        }
    }
    logged.emplace_back(call, result);
    log_warning(std::string(call) + " failed (" + std::to_string(result) + ")");
}

static const char* session_state_name(XrSessionState state) {
    switch (state) {
    case XR_SESSION_STATE_IDLE: return "IDLE";
    case XR_SESSION_STATE_READY: return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
    case XR_SESSION_STATE_STOPPING: return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING: return "EXITING";
    default: return "UNKNOWN";
    }
}

// The manifest of the active OpenXR runtime, looked up the way the OpenXR loader does it: XR_RUNTIME_JSON, then the
// per-user and the machine ActiveRuntime value. A 32-bit process reads the 32-bit registry view, so it only finds a
// runtime that was registered for 32-bit applications.
static std::string find_active_runtime_manifest() {
    char buffer[MAX_PATH * 2];
    DWORD length = GetEnvironmentVariableA("XR_RUNTIME_JSON", buffer, sizeof(buffer));
    if (length > 0 && length < sizeof(buffer)) {
        return buffer;
    }
    for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }) {
        DWORD size = sizeof(buffer);
        if (RegGetValueA(root, "SOFTWARE\\Khronos\\OpenXR\\1", "ActiveRuntime", RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS && buffer[0] != '\0') {
            return buffer;
        }
    }
    return "";
}

// Reads "library_path" from a runtime manifest and makes it absolute (relative paths are relative to the manifest).
static std::wstring read_runtime_library_path(const std::string& manifest_path) {
    std::ifstream file(manifest_path);
    if (!file) {
        return L"";
    }
    std::stringstream contents;
    contents << file.rdbuf();
    const std::string json = contents.str();

    size_t pos = json.find("\"library_path\"");
    if (pos == std::string::npos || (pos = json.find(':', pos)) == std::string::npos || (pos = json.find('"', pos)) == std::string::npos) {
        return L"";
    }
    std::string library;
    for (++pos; pos < json.size() && json[pos] != '"'; ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
        }
        library += json[pos];
    }

    const bool absolute = library.size() > 1 && (library[1] == ':' || (library[0] == '\\' && library[1] == '\\'));
    if (!absolute) {
        const size_t separator = manifest_path.find_last_of("\\/");
        library = (separator == std::string::npos ? std::string() : manifest_path.substr(0, separator + 1)) + library;
    }
    // LoadLibraryEx only accepts backslashes with LOAD_WITH_ALTERED_SEARCH_PATH
    for (char& c : library) {
        if (c == '/') {
            c = '\\';
        }
    }

    const int wide_length = MultiByteToWideChar(CP_UTF8, 0, library.c_str(), -1, nullptr, 0);
    std::wstring wide(wide_length > 0 ? wide_length - 1 : 0, L'\0');
    if (wide_length > 1) {
        MultiByteToWideChar(CP_UTF8, 0, library.c_str(), -1, &wide[0], wide_length);
    }
    return wide;
}

// Draws the back buffer into an 8-bit _SRGB swapchain image, for back buffer formats the runtime does not offer.
// The back buffer holds display-encoded values: the shader decodes them, and the _SRGB render target encodes them
// again, so the pixels keep their meaning and only lose the precision the 8-bit target cannot hold.
static const char convert_shader_source[] = R"(
Texture2D<float4> source : register(t0);

void vs_main(uint id : SV_VertexID, out float4 position : SV_Position)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 ps_main(float4 position : SV_Position) : SV_Target
{
    float4 color = source.Load(int3(position.xy, 0));
    float3 low = color.rgb / 12.92;
    float3 high = pow((color.rgb + 0.055) / 1.055, 2.4);
    return float4(color.rgb <= 0.04045 ? low : high, color.a);
}

// Mono probe: reduces each eye to a MONO_GRID_X x MONO_GRID_Y grid of 8x16-sample averages, left eye in the left half of
// the target and right eye in the right half.
float4 ps_mono_probe(float4 position : SV_Position) : SV_Target
{
    uint width, height;
    source.GetDimensions(width, height);
    const float2 grid = float2(MONO_GRID_X, MONO_GRID_Y);
    const float2 cell_size = float2(width * 0.5, height) / grid;
    float2 cell = floor(position.xy);
    float eye = cell.x >= grid.x ? 1.0 : 0.0;
    float2 origin = float2(eye * width * 0.5, 0.0) + float2(cell.x - eye * grid.x, cell.y) * cell_size;
    float4 sum = 0.0;
    [loop] for (int y = 0; y < 16; y++)
        [unroll] for (int x = 0; x < 8; x++)
            sum += source.Load(int3(origin + (float2(x, y) + 0.5) * cell_size / float2(8.0, 16.0), 0));
    return saturate(sum / 128.0);
}
)";

// Size of the per-eye grid the mono probe reduces each eye to.
// Fine horizontally, since the comparison searches for a horizontal shift between the eyes.
static const uint32_t mono_grid_x = 256;
static const uint32_t mono_grid_y = 24;

// Mean absolute luma difference (0-255) between the left eye and the right eye shifted by `shift` grid cells, with
// linear interpolation for fractional shifts. Only the overlapping part of the eyes counts.
static double eye_difference(const std::vector<float>& left, const std::vector<float>& right, double shift) {
    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t y = 0; y < mono_grid_y; y++) {
        const float* l = &left[static_cast<size_t>(y) * mono_grid_x];
        const float* r = &right[static_cast<size_t>(y) * mono_grid_x];
        for (uint32_t x = 0; x < mono_grid_x; x++) {
            const double rx = x - shift;
            if (rx < 0.0 || rx > mono_grid_x - 1.0) {
                continue;
            }
            const uint32_t x0 = static_cast<uint32_t>(rx);
            const uint32_t x1 = x0 + 1 < mono_grid_x ? x0 + 1 : x0;
            const double t = rx - x0;
            sum += std::abs(l[x] - (r[x0] * (1.0 - t) + r[x1] * t));
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

// Whether the eyes show one flat picture: identical, or identical up to a single horizontal shift (a stereo shader
// placing a flat image at a fixed depth, as SuperDepth3D does on menus and loading screens).
static bool eyes_are_flat(const std::vector<float>& left, const std::vector<float>& right) {
    // Coarse search over whole cells (up to 1/5 of the eye's width), then refine around the best one.
    const int max_shift = static_cast<int>(mono_grid_x / 5);
    const double unshifted = eye_difference(left, right, 0.0);
    double best_shift = 0.0, best = unshifted;
    for (int shift = -max_shift; shift <= max_shift; shift++) {
        const double difference = eye_difference(left, right, shift);
        if (difference < best) {
            best = difference;
            best_shift = shift;
        }
    }
    for (double shift = best_shift - 1.0; shift <= best_shift + 1.0; shift += 0.125) {
        best = std::min(best, eye_difference(left, right, shift));
    }
    // Measured: a flat menu at a fixed depth leaves about 1.1 after the best shift (sampling noise), real stereo
    // stays far above it. Requiring the shift to explain most of the difference guards low-contrast 3D scenes.
    return unshifted < 3.0 || (best < 3.0 && best < 0.3 * unshifted);
}

// Compiles the conversion shader with the system's d3dcompiler_47.dll (part of Windows since 8.1).
static ID3DBlob* compile_convert_shader(const char* entry_point, const char* target) {
    using d3dcompile_fn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
    static HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    auto compile = compiler != nullptr ? reinterpret_cast<d3dcompile_fn>(GetProcAddress(compiler, "D3DCompile")) : nullptr;
    if (compile == nullptr) {
        return nullptr;
    }
    struct Macro { const char* name; const char* definition; };
    const std::string grid_x = std::to_string(mono_grid_x), grid_y = std::to_string(mono_grid_y);
    const Macro macros[] = { { "MONO_GRID_X", grid_x.c_str() }, { "MONO_GRID_Y", grid_y.c_str() }, { nullptr, nullptr } };
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    compile(convert_shader_source, sizeof(convert_shader_source) - 1, "displayxr_convert", macros, nullptr, entry_point, target, 0, 0, &code, &errors);
    if (errors != nullptr) {
        reshade::log_message(reshade::log_level::warning, static_cast<const char*>(errors->GetBufferPointer()));
        errors->Release();
    }
    return code;
}

bool DisplayXRBackend::is_requested() {
    // Read directly: ConfigManager only parses booleans and integers.
    char value[32] = {};
    size_t size = sizeof(value);
    if (!reshade::get_config_value(nullptr, gb_config_section_name.c_str(), gb_config_backend.c_str(), value, &size)) {
        return false;
    }
    std::string backend(value);
    std::transform(backend.begin(), backend.end(), backend.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return backend == gb_config_backend_displayxr;
}

DisplayXRBackend* DisplayXRBackend::try_create(reshade::api::effect_runtime* runtime) {
    auto* backend = new DisplayXRBackend();
    if (backend->initialize(runtime)) {
        log_info("using DisplayXR runtime '" + backend->runtime_name + "' for weaving");
        return backend;
    }
    log_warning(backend->status_message + " Falling back to the SR weaver.");
    delete backend;
    return nullptr;
}

bool DisplayXRBackend::is_output_runtime(reshade::api::effect_runtime* runtime) {
    return DisplayXROverlayWindow::is_overlay_window(static_cast<HWND>(runtime->get_hwnd()));
}

void DisplayXRBackend::abandon_teardown() {
    teardown_abandoned.store(true);
}

bool DisplayXRBackend::load_runtime() {
    const std::string manifest = find_active_runtime_manifest();
    if (manifest.empty()) {
        status_message = "No active OpenXR runtime is registered.";
        return false;
    }
    const std::wstring library = read_runtime_library_path(manifest);
    if (library.empty()) {
        status_message = "Cannot read library_path from the OpenXR runtime manifest " + manifest + ".";
        return false;
    }
    // Deliberately never unloaded: the runtime may keep threads alive after the session is gone.
    runtime_module = LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (runtime_module == nullptr) {
        status_message = "Cannot load the OpenXR runtime of " + manifest + " (error " + std::to_string(GetLastError()) + ").";
        return false;
    }

    auto negotiate = reinterpret_cast<PFN_xrNegotiateLoaderRuntimeInterface>(GetProcAddress(runtime_module, "xrNegotiateLoaderRuntimeInterface"));
    if (negotiate == nullptr) {
        status_message = "The OpenXR runtime does not export xrNegotiateLoaderRuntimeInterface.";
        return false;
    }
    XrNegotiateLoaderInfo loader_info = {};
    loader_info.structType = XR_LOADER_INTERFACE_STRUCT_LOADER_INFO;
    loader_info.structVersion = XR_LOADER_INFO_STRUCT_VERSION;
    loader_info.structSize = sizeof(loader_info);
    loader_info.minInterfaceVersion = 1;
    loader_info.maxInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    loader_info.minApiVersion = XR_MAKE_VERSION(1, 0, 0);
    loader_info.maxApiVersion = XR_MAKE_VERSION(1, 0x3ff, 0xfff);
    XrNegotiateRuntimeRequest request = {};
    request.structType = XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST;
    request.structVersion = XR_RUNTIME_INFO_STRUCT_VERSION;
    request.structSize = sizeof(request);
    if (XR_FAILED(negotiate(&loader_info, &request)) || request.getInstanceProcAddr == nullptr) {
        status_message = "OpenXR runtime interface negotiation failed.";
        return false;
    }
    get_proc_addr = request.getInstanceProcAddr;
    return true;
}

template <typename T>
static void resolve(PFN_xrGetInstanceProcAddr get_proc_addr, XrInstance instance, const char* name, T& function) {
    get_proc_addr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&function));
}

void DisplayXRBackend::resolve_functions() {
    resolve(get_proc_addr, instance, "xrDestroyInstance", xrDestroyInstance);
    resolve(get_proc_addr, instance, "xrPollEvent", xrPollEvent);
    resolve(get_proc_addr, instance, "xrBeginSession", xrBeginSession);
    resolve(get_proc_addr, instance, "xrEndSession", xrEndSession);
    resolve(get_proc_addr, instance, "xrDestroySession", xrDestroySession);
    resolve(get_proc_addr, instance, "xrDestroySpace", xrDestroySpace);
    resolve(get_proc_addr, instance, "xrWaitFrame", xrWaitFrame);
    resolve(get_proc_addr, instance, "xrBeginFrame", xrBeginFrame);
    resolve(get_proc_addr, instance, "xrEndFrame", xrEndFrame);
    resolve(get_proc_addr, instance, "xrLocateViews", xrLocateViews);
    resolve(get_proc_addr, instance, "xrEnumerateSwapchainFormats", xrEnumerateSwapchainFormats);
    resolve(get_proc_addr, instance, "xrCreateSwapchain", xrCreateSwapchain);
    resolve(get_proc_addr, instance, "xrDestroySwapchain", xrDestroySwapchain);
    resolve(get_proc_addr, instance, "xrAcquireSwapchainImage", xrAcquireSwapchainImage);
    resolve(get_proc_addr, instance, "xrWaitSwapchainImage", xrWaitSwapchainImage);
    resolve(get_proc_addr, instance, "xrReleaseSwapchainImage", xrReleaseSwapchainImage);
}

bool DisplayXRBackend::initialize(reshade::api::effect_runtime* runtime) {
    device = runtime->get_device();
    queue = runtime->get_command_queue();
    switch (device->get_api()) {
    case reshade::api::device_api::d3d11:
        graphics = create_displayxr_graphics_d3d11(device);
        break;
    case reshade::api::device_api::d3d12:
        graphics = create_displayxr_graphics_d3d12(device, runtime->get_command_queue());
        break;
    default:
        status_message = "The DisplayXR backend supports D3D11 and D3D12 games only.";
        return false;
    }

    if (!load_runtime()) {
        return false;
    }

    // The window binding extension is what makes the active runtime a DisplayXR runtime we can drive.
    PFN_xrEnumerateInstanceExtensionProperties enumerate_extensions = nullptr;
    resolve(get_proc_addr, XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", enumerate_extensions);
    PFN_xrCreateInstance create_instance = nullptr;
    resolve(get_proc_addr, XR_NULL_HANDLE, "xrCreateInstance", create_instance);
    uint32_t extension_count = 0;
    if (enumerate_extensions == nullptr || create_instance == nullptr || XR_FAILED(enumerate_extensions(nullptr, 0, &extension_count, nullptr))) {
        status_message = "Cannot enumerate the OpenXR runtime's extensions.";
        return false;
    }
    std::vector<XrExtensionProperties> extensions(extension_count, { XR_TYPE_EXTENSION_PROPERTIES });
    enumerate_extensions(nullptr, extension_count, &extension_count, extensions.data());
    bool has_graphics = false, has_window_binding = false, has_display_info = false, has_color_encoding = false;
    for (const XrExtensionProperties& extension : extensions) {
        has_graphics |= std::strcmp(extension.extensionName, graphics->extension_name()) == 0;
        has_window_binding |= std::strcmp(extension.extensionName, XR_DXR_WIN32_WINDOW_BINDING_EXTENSION_NAME) == 0;
        has_display_info |= std::strcmp(extension.extensionName, XR_DXR_DISPLAY_INFO_EXTENSION_NAME) == 0;
        has_color_encoding |= std::strcmp(extension.extensionName, XR_DXR_SWAPCHAIN_COLOR_ENCODING_EXTENSION_NAME) == 0;
    }
    if (!has_window_binding) {
        status_message = "The active OpenXR runtime is not DisplayXR (no " XR_DXR_WIN32_WINDOW_BINDING_EXTENSION_NAME ").";
        return false;
    }
    if (!has_graphics) {
        status_message = std::string("The DisplayXR runtime does not support ") + graphics->extension_name() + ".";
        return false;
    }

    // Optional extensions: XR_DXR_display_info lets the backend switch the display to 2D while the game shows no
    // stereo, XR_DXR_swapchain_color_encoding lets it submit a 10-bit back buffer without converting it.
    std::vector<const char*> enabled_extensions = { graphics->extension_name(), XR_DXR_WIN32_WINDOW_BINDING_EXTENSION_NAME };
    if (has_display_info) {
        enabled_extensions.push_back(XR_DXR_DISPLAY_INFO_EXTENSION_NAME);
    }
    if (has_color_encoding) {
        enabled_extensions.push_back(XR_DXR_SWAPCHAIN_COLOR_ENCODING_EXTENSION_NAME);
    }
    color_encoding_available = has_color_encoding;
    XrInstanceCreateInfo instance_info = { XR_TYPE_INSTANCE_CREATE_INFO };
    std::strncpy(instance_info.applicationInfo.applicationName, "3DGameBridge", XR_MAX_APPLICATION_NAME_SIZE - 1);
    std::strncpy(instance_info.applicationInfo.engineName, "ReShade", XR_MAX_ENGINE_NAME_SIZE - 1);
    instance_info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    instance_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
    instance_info.enabledExtensionNames = enabled_extensions.data();
    XrResult result = create_instance(&instance_info, &instance);
    if (XR_FAILED(result)) {
        status_message = "xrCreateInstance failed (" + std::to_string(result) + ").";
        return false;
    }
    resolve_functions();
    if (has_display_info) {
        resolve(get_proc_addr, instance, "xrRequestDisplayModeDXR", xrRequestDisplayModeDXR);
    }
    try {
        const ConfigManager::ConfigValue value = ConfigManager::read_from_config(gb_config_displayxr_auto_2d);
        auto_2d_enabled = value.value_type != ConfigManager::ConfigValue::Type::Bool || value.bool_value;
    }
    catch (std::runtime_error&) {
        auto_2d_enabled = true;
    }

    PFN_xrGetInstanceProperties get_instance_properties = nullptr;
    resolve(get_proc_addr, instance, "xrGetInstanceProperties", get_instance_properties);
    XrInstanceProperties properties = { XR_TYPE_INSTANCE_PROPERTIES };
    if (get_instance_properties != nullptr && XR_SUCCEEDED(get_instance_properties(instance, &properties))) {
        runtime_name = std::string(properties.runtimeName) + " " + std::to_string(XR_VERSION_MAJOR(properties.runtimeVersion)) + "." +
                       std::to_string(XR_VERSION_MINOR(properties.runtimeVersion)) + "." + std::to_string(XR_VERSION_PATCH(properties.runtimeVersion));
    }

    PFN_xrGetSystem get_system = nullptr;
    resolve(get_proc_addr, instance, "xrGetSystem", get_system);
    XrSystemGetInfo system_info = { XR_TYPE_SYSTEM_GET_INFO };
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (get_system == nullptr || XR_FAILED(get_system(instance, &system_info, &system_id))) {
        status_message = "xrGetSystem failed: no 3D display found by the runtime.";
        return false;
    }
    if (!graphics->check_requirements(instance, system_id, get_proc_addr)) {
        status_message = std::string("The graphics requirements call of ") + graphics->extension_name() + " failed.";
        return false;
    }

    // The runtime presents into a window of its own on top of the game's: see DisplayXROverlayWindow.
    overlay = new DisplayXROverlayWindow();
    HWND overlay_hwnd = overlay->create(static_cast<HWND>(runtime->get_hwnd()));
    if (overlay_hwnd == nullptr) {
        status_message = "Cannot create the DisplayXR output window.";
        return false;
    }

    XrWin32WindowBindingCreateInfoDXR window_binding = { XR_TYPE_WIN32_WINDOW_BINDING_CREATE_INFO_DXR };
    window_binding.windowHandle = overlay_hwnd;
    XrSessionCreateInfo session_info = { XR_TYPE_SESSION_CREATE_INFO };
    session_info.next = graphics->session_binding(&window_binding);
    session_info.systemId = system_id;
    PFN_xrCreateSession create_session = nullptr;
    resolve(get_proc_addr, instance, "xrCreateSession", create_session);
    result = create_session(instance, &session_info, &session);
    if (XR_FAILED(result)) {
        status_message = "xrCreateSession failed (" + std::to_string(result) + ").";
        return false;
    }

    PFN_xrCreateReferenceSpace create_space = nullptr;
    resolve(get_proc_addr, instance, "xrCreateReferenceSpace", create_space);
    XrReferenceSpaceCreateInfo space_info = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w = 1.0f;
    if (XR_FAILED(create_space(session, &space_info, &space))) {
        status_message = "xrCreateReferenceSpace failed.";
        return false;
    }
    return true;
}

DisplayXRBackend::~DisplayXRBackend() {
    if (teardown_abandoned.load()) {
        // Under the loader lock at process exit: the runtime's and the overlay's threads are gone or cannot be
        // joined. Leave everything to the process teardown.
        return;
    }
    destroy_swapchain();
    destroy_mono_probe_resources();
    if (convert_pipeline != 0) {
        device->destroy_pipeline(convert_pipeline);
    }
    if (convert_layout != 0) {
        device->destroy_pipeline_layout(convert_layout);
    }
    if (space != XR_NULL_HANDLE) {
        xrDestroySpace(space);
    }
    if (session != XR_NULL_HANDLE) {
        xrDestroySession(session);
    }
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(instance);
    }
    if (overlay != nullptr) {
        overlay->destroy(true);
        delete overlay;
    }
    delete graphics;
}

void DisplayXRBackend::poll_events() {
    XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(instance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            session_state = reinterpret_cast<XrEventDataSessionStateChanged&>(event).state;
            log_info(std::string("session state ") + session_state_name(session_state));
            switch (session_state) {
            case XR_SESSION_STATE_READY: {
                XrSessionBeginInfo begin_info = { XR_TYPE_SESSION_BEGIN_INFO };
                begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                session_running = XR_SUCCEEDED(xrBeginSession(session, &begin_info));
                break;
            }
            case XR_SESSION_STATE_STOPPING:
                xrEndSession(session);
                session_running = false;
                break;
            case XR_SESSION_STATE_LOSS_PENDING:
            case XR_SESSION_STATE_EXITING:
                session_running = false;
                session_lost = true;
                overlay->set_visible(false);
                status_message = "The DisplayXR session ended. Restart the game to weave again.";
                break;
            default:
                break;
            }
        }
        else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            session_running = false;
            session_lost = true;
            overlay->set_visible(false);
            status_message = "The DisplayXR runtime is shutting down. Restart the game to weave again.";
        }
        event = { XR_TYPE_EVENT_DATA_BUFFER };
    }
}

bool DisplayXRBackend::create_convert_pipeline(reshade::api::format target_format) {
    if (convert_pipeline != 0 && convert_pipeline_format == target_format) {
        return true;
    }
    if (convert_pipeline != 0) {
        device->destroy_pipeline(convert_pipeline);
        convert_pipeline = {};
    }
    convert_pipeline_format = create_fullscreen_pipeline("ps_main", target_format, &convert_pipeline) ? target_format : reshade::api::format::unknown;
    return convert_pipeline != 0;
}

bool DisplayXRBackend::create_fullscreen_pipeline(const char* pixel_shader, reshade::api::format target_format, reshade::api::pipeline* out_pipeline) {
    // The shaders are HLSL compiled to DXBC, which serves D3D11 and D3D12. A Vulkan path needs SPIR-V variants.
    const reshade::api::device_api api = device->get_api();
    if (api != reshade::api::device_api::d3d11 && api != reshade::api::device_api::d3d12) {
        return false;
    }
    if (convert_layout == 0) {
        reshade::api::descriptor_range range = {};
        range.count = 1;
        range.visibility = reshade::api::shader_stage::pixel;
        range.type = reshade::api::descriptor_type::shader_resource_view;
        const reshade::api::pipeline_layout_param param(range);
        if (!device->create_pipeline_layout(1, &param, &convert_layout)) {
            return false;
        }
    }

    ID3DBlob* vs = compile_convert_shader("vs_main", "vs_5_0");
    ID3DBlob* ps = compile_convert_shader(pixel_shader, "ps_5_0");
    bool created = false;
    if (vs != nullptr && ps != nullptr) {
        reshade::api::shader_desc vs_desc = {};
        vs_desc.code = vs->GetBufferPointer();
        vs_desc.code_size = vs->GetBufferSize();
        reshade::api::shader_desc ps_desc = {};
        ps_desc.code = ps->GetBufferPointer();
        ps_desc.code_size = ps->GetBufferSize();
        reshade::api::format format = target_format;
        reshade::api::primitive_topology topology = reshade::api::primitive_topology::triangle_list;
        reshade::api::pipeline_subobject subobjects[] = {
            { reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc },
            { reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc },
            { reshade::api::pipeline_subobject_type::render_target_formats, 1, &format },
            { reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology },
        };
        created = device->create_pipeline(convert_layout, 4, subobjects, out_pipeline);
    }
    if (vs != nullptr) vs->Release();
    if (ps != nullptr) ps->Release();
    return created;
}

void DisplayXRBackend::destroy_convert_resources() {
    if (convert_source == 0 && convert_rtvs.empty()) {
        return;
    }
    // In D3D12 the copy and the draw of the previous frame may still be in flight.
    if (queue != nullptr) {
        queue->wait_idle();
    }
    for (reshade::api::resource_view rtv : convert_rtvs) {
        if (rtv != 0) {
            device->destroy_resource_view(rtv);
        }
    }
    convert_rtvs.clear();
    if (convert_source_srv != 0) {
        device->destroy_resource_view(convert_source_srv);
        convert_source_srv = {};
    }
    if (convert_source != 0) {
        device->destroy_resource(convert_source);
        convert_source = {};
    }
}

bool DisplayXRBackend::copy_to_sampled_source(reshade::api::command_list* cmd_list, reshade::api::resource back_buffer) {
    const reshade::api::resource_desc desc = device->get_resource_desc(back_buffer);
    const reshade::api::format source_format = reshade::api::format_to_default_typed(desc.texture.format, 0);

    // Back buffers usually cannot be sampled, so the frame goes through a copy that can.
    if (convert_source == 0) {
        const reshade::api::resource_desc source_desc(desc.texture.width, desc.texture.height, 1, 1, source_format, 1,
                                                      reshade::api::memory_heap::gpu_only,
                                                      reshade::api::resource_usage::shader_resource | reshade::api::resource_usage::copy_dest);
        if (!device->create_resource(source_desc, nullptr, reshade::api::resource_usage::shader_resource, &convert_source) ||
            !device->create_resource_view(convert_source, reshade::api::resource_usage::shader_resource, reshade::api::resource_view_desc(source_format), &convert_source_srv)) {
            return false;
        }
    }
    cmd_list->barrier(back_buffer, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
    cmd_list->barrier(convert_source, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_dest);
    cmd_list->copy_resource(back_buffer, convert_source);
    cmd_list->barrier(back_buffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);
    cmd_list->barrier(convert_source, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::shader_resource);
    return true;
}

bool DisplayXRBackend::convert_to_image(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource back_buffer, uint32_t image_index) {
    const reshade::api::resource_desc desc = device->get_resource_desc(back_buffer);
    if (!copy_to_sampled_source(cmd_list, back_buffer)) {
        return false;
    }
    if (image_index >= convert_rtvs.size()) {
        convert_rtvs.resize(image_index + 1, reshade::api::resource_view{ 0 });
    }
    if (convert_rtvs[image_index] == 0) {
        const reshade::api::resource image = { graphics->image_handle(image_index) };
        if (image == 0 || !device->create_resource_view(image, reshade::api::resource_usage::render_target, reshade::api::resource_view_desc(convert_pipeline_format), &convert_rtvs[image_index])) {
            return false;
        }
    }

    // OpenXR hands out color swapchain images ready to render to (D3D12: RENDER_TARGET state).
    cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_graphics, convert_pipeline);
    cmd_list->push_descriptors(reshade::api::shader_stage::pixel, convert_layout, 0,
                               reshade::api::descriptor_table_update{ {}, 0, 0, 1, reshade::api::descriptor_type::shader_resource_view, &convert_source_srv });
    const reshade::api::viewport viewport = { 0.0f, 0.0f, static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), 0.0f, 1.0f };
    const reshade::api::rect scissor = { 0, 0, static_cast<int32_t>(desc.texture.width), static_cast<int32_t>(desc.texture.height) };
    cmd_list->bind_viewports(0, 1, &viewport);
    cmd_list->bind_scissor_rects(0, 1, &scissor);
    cmd_list->bind_render_targets_and_depth_stencil(1, &convert_rtvs[image_index]);
    cmd_list->draw(3, 1, 0, 0);

    // The runtime reads the image as soon as it is released, so submit now (a no-op in D3D11).
    runtime->get_command_queue()->flush_immediate_command_list();
    return true;
}

bool DisplayXRBackend::create_mono_probe_resources() {
    if (mono_probe_pipeline != 0) {
        return true;
    }
    const reshade::api::format format = reshade::api::format::r8g8b8a8_unorm;
    const uint32_t width = mono_grid_x * 2, height = mono_grid_y;
    if (!create_fullscreen_pipeline("ps_mono_probe", format, &mono_probe_pipeline)) {
        return false;
    }
    const reshade::api::resource_desc target_desc(width, height, 1, 1, format, 1, reshade::api::memory_heap::gpu_only,
                                                  reshade::api::resource_usage::render_target | reshade::api::resource_usage::copy_source);
    if (!device->create_resource(target_desc, nullptr, reshade::api::resource_usage::render_target, &mono_probe_target) ||
        !device->create_resource_view(mono_probe_target, reshade::api::resource_usage::render_target, reshade::api::resource_view_desc(format), &mono_probe_rtv)) {
        return false;
    }
    // Read back through a ring, so the CPU only maps results the GPU finished frames ago and never waits.
    // D3D11 reads back from staging textures, D3D12 from buffers (D3D12 textures cannot live in a readback heap).
    const bool d3d12 = device->get_api() == reshade::api::device_api::d3d12;
    for (reshade::api::resource& readback : mono_probe_readback) {
        const reshade::api::resource_desc readback_desc = d3d12
            ? reshade::api::resource_desc(static_cast<uint64_t>(width) * 4 * height, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest)
            : reshade::api::resource_desc(width, height, 1, 1, format, 1, reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest);
        if (!device->create_resource(readback_desc, nullptr, reshade::api::resource_usage::copy_dest, &readback)) {
            return false;
        }
    }
    return true;
}

void DisplayXRBackend::destroy_mono_probe_resources() {
    if (queue != nullptr && mono_probe_target != 0) {
        queue->wait_idle();
    }
    for (reshade::api::resource& readback : mono_probe_readback) {
        if (readback != 0) {
            device->destroy_resource(readback);
            readback = {};
        }
    }
    if (mono_probe_rtv != 0) {
        device->destroy_resource_view(mono_probe_rtv);
        mono_probe_rtv = {};
    }
    if (mono_probe_target != 0) {
        device->destroy_resource(mono_probe_target);
        mono_probe_target = {};
    }
    if (mono_probe_pipeline != 0) {
        device->destroy_pipeline(mono_probe_pipeline);
        mono_probe_pipeline = {};
    }
}

void DisplayXRBackend::update_mono_probe(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource back_buffer) {
    // About ten probes a second: 2D/3D switching does not need more, and most frames pay nothing.
    if (xrRequestDisplayModeDXR == nullptr || !auto_2d_enabled || (++mono_probe_frame % 6) != 0) {
        return;
    }
    if (!create_mono_probe_resources()) {
        auto_2d_enabled = false;
        log_warning("cannot create the mono probe, automatic 2D for mono frames is off.");
        return;
    }
    const uint32_t width = mono_grid_x * 2, height = mono_grid_y;
    const bool d3d12 = device->get_api() == reshade::api::device_api::d3d12;

    // Evaluate the oldest result first, before its readback slot is reused below.
    const size_t slot = mono_probe_count % std::size(mono_probe_readback);
    if (mono_probe_count >= std::size(mono_probe_readback)) {
        const uint8_t* pixels = nullptr;
        uint32_t row_pitch = width * 4;
        void* mapped = nullptr;
        reshade::api::subresource_data data = {};
        if (d3d12 ? device->map_buffer_region(mono_probe_readback[slot], 0, UINT64_MAX, reshade::api::map_access::read_only, &mapped)
                  : device->map_texture_region(mono_probe_readback[slot], 0, nullptr, reshade::api::map_access::read_only, &data)) {
            pixels = static_cast<const uint8_t*>(d3d12 ? mapped : data.data);
            row_pitch = d3d12 ? row_pitch : data.row_pitch;
        }
        if (pixels != nullptr) {
            std::vector<float> left(static_cast<size_t>(mono_grid_x) * mono_grid_y), right(left.size());
            for (uint32_t y = 0; y < height; y++) {
                const uint8_t* row = pixels + static_cast<size_t>(y) * row_pitch;
                for (uint32_t x = 0; x < mono_grid_x; x++) {
                    const uint8_t* l = row + x * 4;
                    const uint8_t* r = row + (x + mono_grid_x) * 4;
                    left[static_cast<size_t>(y) * mono_grid_x + x] = (l[0] * 2 + l[1] * 5 + l[2]) / 8.0f;
                    right[static_cast<size_t>(y) * mono_grid_x + x] = (r[0] * 2 + r[1] * 5 + r[2]) / 8.0f;
                }
            }
            if (d3d12) {
                device->unmap_buffer_region(mono_probe_readback[slot]);
            } else {
                device->unmap_texture_region(mono_probe_readback[slot], 0);
            }
            apply_mono_verdict(eyes_are_flat(left, right));
        }
    }

    // Reduce this frame and queue its readback.
    if (!copy_to_sampled_source(cmd_list, back_buffer)) {
        return;
    }
    cmd_list->bind_pipeline(reshade::api::pipeline_stage::all_graphics, mono_probe_pipeline);
    cmd_list->push_descriptors(reshade::api::shader_stage::pixel, convert_layout, 0,
                               reshade::api::descriptor_table_update{ {}, 0, 0, 1, reshade::api::descriptor_type::shader_resource_view, &convert_source_srv });
    const reshade::api::viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    const reshade::api::rect scissor = { 0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height) };
    cmd_list->bind_viewports(0, 1, &viewport);
    cmd_list->bind_scissor_rects(0, 1, &scissor);
    cmd_list->bind_render_targets_and_depth_stencil(1, &mono_probe_rtv);
    cmd_list->draw(3, 1, 0, 0);
    cmd_list->barrier(mono_probe_target, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
    if (d3d12) {
        cmd_list->copy_texture_to_buffer(mono_probe_target, 0, nullptr, mono_probe_readback[slot], 0, width, height);
    } else {
        cmd_list->copy_texture_region(mono_probe_target, 0, nullptr, mono_probe_readback[slot], 0, nullptr);
    }
    cmd_list->barrier(mono_probe_target, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);
    mono_probe_count++;
    (void)runtime;
}

void DisplayXRBackend::apply_mono_verdict(bool mono) {
    // Switch to 2D only after the frame has been flat for about half a second (SuperDepth3D toggled off, menus and
    // loading screens), and back to 3D on the second probe that sees depth, so a scene cut does not flicker the lens.
    // While in 2D both views show the left eye, so a flat picture placed at a depth (shifted eyes) stays sharp.
    mono_streak = mono ? mono_streak + 1 : 0;
    stereo_streak = mono ? 0 : stereo_streak + 1;
    const bool want_2d = display_2d ? stereo_streak < 2 : mono_streak >= 5;
    if (want_2d == display_2d) {
        return;
    }
    const XrResult result = xrRequestDisplayModeDXR(session, want_2d ? XR_DISPLAY_MODE_2D_DXR : XR_DISPLAY_MODE_3D_DXR);
    if (XR_SUCCEEDED(result)) {
        display_2d = want_2d;
        log_info(want_2d ? "the eyes show a flat picture: display switched to 2D" : "the eyes show depth: display switched to 3D");
    } else {
        log_failure_once("xrRequestDisplayModeDXR", result);
    }
}

void DisplayXRBackend::destroy_swapchain() {
    destroy_convert_resources();
    if (swapchain != XR_NULL_HANDLE) {
        graphics->release_images();
        xrDestroySwapchain(swapchain);
        swapchain = XR_NULL_HANDLE;
    }
    swapchain_width = swapchain_height = 0;
    swapchain_source_format = reshade::api::format::unknown;
}

bool DisplayXRBackend::ensure_swapchain(const reshade::api::resource_desc& desc) {
    if (swapchain != XR_NULL_HANDLE && desc.texture.width == swapchain_width && desc.texture.height == swapchain_height && desc.texture.format == swapchain_source_format) {
        return true;
    }
    destroy_swapchain();
    if (swapchain_failed) {
        return false;
    }
    if (desc.texture.samples > 1) {
        status_message = "Multisampled back buffers are not supported.";
        log_warning(status_message);
        swapchain_failed = true;
        return false;
    }

    // The game's back buffer holds display-encoded (sRGB) values, and DisplayXR reads a UNORM swapchain as linear.
    // - 8-bit: the same bytes go into the _SRGB variant of the format, a raw copy (understood by every runtime).
    // - Deeper formats have no _SRGB variant: declare the swapchain display-encoded with
    //   XR_DXR_swapchain_color_encoding and copy raw, or, on a runtime without it, convert (below).
    const reshade::api::format typed = reshade::api::format_to_default_typed(desc.texture.format, 0);
    const reshade::api::format srgb = reshade::api::format_to_default_typed(desc.texture.format, 1);
    std::vector<reshade::api::format> candidates;
    if (srgb != typed) {
        candidates.push_back(srgb);
    } else if (color_encoding_available) {
        candidates.push_back(typed);
    }

    uint32_t format_count = 0;
    xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr);
    std::vector<int64_t> formats(format_count);
    xrEnumerateSwapchainFormats(session, format_count, &format_count, formats.data());
    reshade::api::format chosen = reshade::api::format::unknown;
    for (reshade::api::format candidate : candidates) {
        for (int64_t format : formats) {
            if (format == graphics->native_format(candidate)) {
                chosen = candidate;
                break;
            }
        }
        if (chosen != reshade::api::format::unknown) {
            break;
        }
    }
    convert = false;
    if (chosen == reshade::api::format::unknown) {
        for (reshade::api::format target : { reshade::api::format::r8g8b8a8_unorm_srgb, reshade::api::format::b8g8r8a8_unorm_srgb }) {
            if (chosen == reshade::api::format::unknown && std::find(formats.begin(), formats.end(), graphics->native_format(target)) != formats.end() &&
                create_convert_pipeline(target)) {
                chosen = target;
                convert = true;
            }
        }
    }
    if (chosen == reshade::api::format::unknown) {
        status_message = "The runtime offers no swapchain format matching the back buffer (" + std::to_string(static_cast<uint32_t>(desc.texture.format)) + ").";
        log_warning(status_message);
        swapchain_failed = true;
        return false;
    }

    XrSwapchainColorEncodingCreateInfoDXR encoding_info = { XR_TYPE_SWAPCHAIN_COLOR_ENCODING_CREATE_INFO_DXR };
    encoding_info.colorEncoding = XR_SWAPCHAIN_COLOR_ENCODING_SRGB_DXR;
    const bool declare_encoded = !convert && chosen == typed && srgb == typed;

    XrSwapchainCreateInfo swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchain_info.next = declare_encoded ? &encoding_info : nullptr;
    swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    swapchain_info.format = graphics->native_format(chosen);
    swapchain_info.sampleCount = 1;
    swapchain_info.width = desc.texture.width;
    swapchain_info.height = desc.texture.height;
    swapchain_info.faceCount = 1;
    swapchain_info.arraySize = 1;
    swapchain_info.mipCount = 1;
    XrResult result = xrCreateSwapchain(session, &swapchain_info, &swapchain);
    if (result == XR_ERROR_FEATURE_UNSUPPORTED && declare_encoded) {
        // This session's compositor cannot read the format as display-encoded (e.g. a service session): convert.
        log_info("the runtime cannot take this format display-encoded here, converting instead");
        color_encoding_available = false;
        return ensure_swapchain(desc);
    }
    if (XR_FAILED(result) || !graphics->enumerate_images(swapchain, get_proc_addr, instance)) {
        status_message = "xrCreateSwapchain failed (" + std::to_string(result) + ").";
        log_warning(status_message);
        swapchain_failed = true;
        destroy_swapchain();
        return false;
    }

    swapchain_width = desc.texture.width;
    swapchain_height = desc.texture.height;
    swapchain_source_format = desc.texture.format;
    format_name = std::to_string(static_cast<uint32_t>(chosen)) +
                  (convert ? " (sRGB, converted from " + std::to_string(static_cast<uint32_t>(desc.texture.format)) + ")"
                   : declare_encoded ? " (declared sRGB-encoded)"
                   : chosen == srgb && srgb != typed ? " (sRGB)" : "");
    log_info("swapchain " + std::to_string(swapchain_width) + "x" + std::to_string(swapchain_height) + " format " + format_name);
    return true;
}

GbResult DisplayXRBackend::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    if (session_lost) {
        return SUCCESS;
    }
    poll_events();
    if (!session_running) {
        return SUCCESS;
    }

    XrFrameState frame_state = { XR_TYPE_FRAME_STATE };
    XrResult result = xrWaitFrame(session, nullptr, &frame_state);
    if (XR_FAILED(result)) {
        log_failure_once("xrWaitFrame", result);
        return GENERAL_FAIL;
    }
    result = xrBeginFrame(session, nullptr);
    if (XR_FAILED(result)) {
        log_failure_once("xrBeginFrame", result);
        return GENERAL_FAIL;
    }

    XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerProjectionView projection_views[2] = { { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };
    const XrCompositionLayerBaseHeader* layers[] = { reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer) };
    uint32_t layer_count = 0;

    if (!frame_state.shouldRender || !weaving_enabled || reshade_overlay_open) {
        static bool logged_skip = false;
        if (!logged_skip) {
            logged_skip = true;
            log_info(std::string("not submitting: shouldRender=") + (frame_state.shouldRender ? "1" : "0") + " weaving=" + (weaving_enabled ? "1" : "0") + " reshade_overlay=" + (reshade_overlay_open ? "1" : "0"));
        }
    }
    else {
        const reshade::api::resource back_buffer = device->get_resource_from_view(rtv);
        const reshade::api::resource_desc desc = device->get_resource_desc(back_buffer);
        current_buffer_format = desc.texture.format;

        XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
        locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate_info.displayTime = frame_state.predictedDisplayTime;
        locate_info.space = space;
        XrViewState view_state = { XR_TYPE_VIEW_STATE };
        XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
        uint32_t view_count = 0;

        XrResult locate_result = XR_ERROR_RUNTIME_FAILURE;
        if (ensure_swapchain(desc) &&
            (XR_SUCCEEDED(locate_result = xrLocateViews(session, &locate_info, &view_state, 2, &view_count, views)) || (log_failure_once("xrLocateViews", locate_result), false)) &&
            view_count == 2) {
            uint32_t image_index = 0;
            XrSwapchainImageWaitInfo wait_info = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            wait_info.timeout = XR_INFINITE_DURATION;
            XrResult image_result = xrAcquireSwapchainImage(swapchain, nullptr, &image_index);
            if (XR_SUCCEEDED(image_result)) {
                image_result = xrWaitSwapchainImage(swapchain, &wait_info);
            }
            if (XR_FAILED(image_result)) {
                log_failure_once("xrAcquire/WaitSwapchainImage", image_result);
            }
            else {
                const bool copied = convert ? convert_to_image(runtime, cmd_list, back_buffer, image_index)
                                            : graphics->copy_to_image(runtime, cmd_list, back_buffer, image_index);
                if (copied) {
                    update_mono_probe(runtime, cmd_list, back_buffer);
                }
                xrReleaseSwapchainImage(swapchain, nullptr);

                if (copied) {
                    // The back buffer is side-by-side: the left half is the left eye, the right half the right eye.
                    // The game rendered both eyes itself, so the views only echo the runtime's poses back.
                    const int32_t half_width = static_cast<int32_t>(swapchain_width / 2);
                    for (uint32_t eye = 0; eye < 2; eye++) {
                        projection_views[eye].pose = views[eye].pose;
                        projection_views[eye].fov = views[eye].fov;
                        projection_views[eye].subImage.swapchain = swapchain;
                        const int32_t source_eye = display_2d ? 0 : static_cast<int32_t>(eye);
                        projection_views[eye].subImage.imageRect.offset = { source_eye * half_width, 0 };
                        projection_views[eye].subImage.imageRect.extent = { half_width, static_cast<int32_t>(swapchain_height) };
                        projection_views[eye].subImage.imageArrayIndex = 0;
                    }
                    layer.space = space;
                    layer.viewCount = 2;
                    layer.views = projection_views;
                    layer_count = 1;
                }
            }
        }
    }

    XrFrameEndInfo end_info = { XR_TYPE_FRAME_END_INFO };
    end_info.displayTime = frame_state.predictedDisplayTime;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end_info.layerCount = layer_count;
    end_info.layers = layers;
    result = xrEndFrame(session, &end_info);
    if (XR_FAILED(result)) {
        log_failure_once("xrEndFrame", result);
        return GENERAL_FAIL;
    }
    if (layer_count > 0) {
        frames_submitted++;
    }
    return SUCCESS;
}

void DisplayXRBackend::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    // ReShade re-creates the effect runtime when the game resizes its swapchain; the session survives that.
    // The window binding is fixed at session creation though, so a swapchain moved to another window is not followed.
    if (overlay != nullptr && GetWindow(overlay->get_hwnd(), GW_OWNER) != static_cast<HWND>(runtime->get_hwnd())) {
        log_warning("the game presents to a different window than the one DisplayXR was bound to.");
    }
}

void DisplayXRBackend::on_reshade_overlay(bool open) {
    reshade_overlay_open = open;
    if (overlay != nullptr) {
        overlay->set_visible(!open && weaving_enabled);
    }
}

void DisplayXRBackend::do_weave(bool do_weave) {
    weaving_enabled = do_weave;
    if (overlay != nullptr) {
        overlay->set_visible(do_weave && !reshade_overlay_open);
    }
}

LatencyModes DisplayXRBackend::get_latency_mode() {
    // The runtime predicts the eye positions itself
    return LatencyModes::LATENCY_IN_FRAMES_AUTOMATIC;
}

bool DisplayXRBackend::set_latency_in_frames(int32_t) {
    return false;
}

bool DisplayXRBackend::set_latency_frametime_adaptive(uint32_t) {
    return false;
}

void DisplayXRBackend::draw_status_overlay(reshade::api::effect_runtime*, const std::string& error_message) {
    ImGui::TextUnformatted("Status: ACTIVE (DisplayXR)");
    ImGui::Text("Runtime: %s", runtime_name.c_str());
    ImGui::Text("Session: %s", session_state_name(session_state));
    if (swapchain != XR_NULL_HANDLE) {
        ImGui::Text("Swapchain: %ux%u, format %s", swapchain_width, swapchain_height, format_name.c_str());
    }
    ImGui::Text("Frames submitted: %llu", static_cast<unsigned long long>(frames_submitted));
    if (xrRequestDisplayModeDXR != nullptr && auto_2d_enabled) {
        ImGui::Text("Display: %s (2D while the eyes show a flat picture)", display_2d ? "2D" : "3D");
    }
    ImGui::TextUnformatted("The woven image is hidden while this overlay is open.");
    const std::string& message = error_message.empty() ? status_message : error_message;
    if (!message.empty()) {
        ImGui::TextColored(ImColor(240, 100, 100), "%s", message.c_str());
    }
}
