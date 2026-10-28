// Copyright 2025-2026, The DisplayXR Project
// SPDX-License-Identifier: Apache-2.0
//
// PROVISIONAL — DXR is DisplayXR's Khronos-registered OpenXR author ID, but
// the XR_DXR_* extensions in this header are NOT yet registered in the
// Khronos OpenXR registry: extension numbers and XrStructureType values sit
// in a provisional experimental block (1004999xxx) pending official
// assignment. Extension names are expected to be stable; numeric values are
// not. SPEC_VERSION continues the pre-rename XR_EXT_* numbering (the
// interface history did not restart with the name).
// See GOVERNANCE.md in https://github.com/DisplayXR/displayxr-extensions.
//
/*!
 * @file
 * @brief  Header for XR_DXR_display_info extension
 * @author David Fattal
 * @ingroup external_openxr
 *
 * This extension exposes physical display properties and recommended view
 * scaling factors to the application via xrGetSystemProperties. The app
 * multiplies its current window size by the scale factors to compute the
 * per-eye render resolution each frame, eliminating the need for runtime
 * events on window resize.
 */
#ifndef XR_DXR_DISPLAY_INFO_H
#define XR_DXR_DISPLAY_INFO_H 1

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XR_DXR_display_info 1
#define XR_DXR_display_info_SPEC_VERSION 23
#define XR_DXR_DISPLAY_INFO_EXTENSION_NAME "XR_DXR_display_info"

// Reuse the type value from the deleted XR_EXT_dynamic_render_resolution
#define XR_TYPE_DISPLAY_INFO_DXR ((XrStructureType)1004999003)

/*!
 * @brief N-view primary view configuration (spec v19, #1486 / #80).
 *
 * Advertised by xrEnumerateViewConfigurations ONLY when this extension is
 * enabled on the instance, alongside a conformant
 * XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO (which always reports exactly 2
 * views). Under this type xrEnumerateViewConfigurationViews and xrLocateViews
 * report the device's MAXIMUM view count across all rendering modes (e.g. 4
 * on a display with a quad mode; 2 on a stereo-only display), fixed for the
 * instance lifetime; xrEndFrame accepts a projection layer whose viewCount
 * matches any rendering mode's view count. An app that renders N-view modes
 * begins its session with this type; a stereo-fixed app stays on
 * PRIMARY_STEREO and is never handed more than 2 views.
 *
 * Value: 1004999212 (display_info's 210-219 decade — see README.md registry).
 * Cast-defined because C cannot extend the core enum; not visible to -Wswitch.
 */
#define XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MULTIVIEW_DXR ((XrViewConfigurationType)1004999212)

/*!
 * @brief Display information returned by xrGetSystemProperties.
 *
 * When chained to XrSystemProperties via the next pointer, the runtime fills
 * in the physical display dimensions and recommended view scale factors.
 *
 * The application computes per-eye render resolution as:
 *   renderWidth  = (uint32_t)(windowWidth  * recommendedViewScaleX)
 *   renderHeight = (uint32_t)(windowHeight * recommendedViewScaleY)
 *
 * The scale factors are static display properties (sr_recommended / display_pixels)
 * that do not change with window resize.
 *
 * @extends XrSystemProperties
 */
typedef struct XrDisplayInfoDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_DISPLAY_INFO_DXR
    void* XR_MAY_ALIAS          next;       //!< Pointer to next structure in chain
    XrExtent2Df                 displaySizeMeters;          //!< Physical display size in meters
    XrVector3f                  nominalViewerPositionInDisplaySpace; //!< Nominal viewer position in display space (meters)
    float                       recommendedViewScaleX;      //!< Horizontal scale: sr_recommended_w / display_pixel_w
    float                       recommendedViewScaleY;      //!< Vertical scale: sr_recommended_h / display_pixel_h
    uint32_t                    displayPixelWidth;          //!< Native display panel width in pixels (0 if unknown)
    uint32_t                    displayPixelHeight;         //!< Native display panel height in pixels (0 if unknown)
} XrDisplayInfoDXR;

// ---- v16: Display desktop position ----

#define XR_TYPE_DISPLAY_DESKTOP_POSITION_DXR ((XrStructureType)1004999210)

/*!
 * @brief Desktop position of the 3D display, returned by xrGetSystemProperties
 * (v16 addition).
 *
 * When chained to XrSystemProperties (typically via XrDisplayInfoDXR's next
 * pointer), the runtime fills in the 3D panel's top-left corner in OS
 * virtual-desktop coordinates: top-down pixels with the origin at the primary
 * monitor's top-left (Windows virtual-screen / X11 root-window convention).
 * (0, 0) means the panel is the primary monitor or its position is unknown.
 *
 * Applications that create their own window (the handle/texture classes)
 * should create it at this position so the content opens on the 3D panel
 * rather than the primary monitor — the window-relative weave can only
 * produce correct 3D on the panel itself. Hosted-class apps need not care:
 * the runtime self-creates its window there. See runtime issue #715.
 *
 * @extends XrSystemProperties
 */
typedef struct XrDisplayDesktopPositionDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_DISPLAY_DESKTOP_POSITION_DXR
    void* XR_MAY_ALIAS          next;       //!< Pointer to next structure in chain
    int32_t                     left;       //!< Panel left edge in virtual-desktop pixels
    int32_t                     top;        //!< Panel top edge in virtual-desktop pixels
} XrDisplayDesktopPositionDXR;

// ---- v18: Full panel desktop rect + stable device name (#1301) ----

#define XR_TYPE_DISPLAY_DESKTOP_INFO_DXR ((XrStructureType)1004999211)

//! Size of XrDisplayDesktopInfoDXR::deviceName, in bytes, including the NUL.
#define XR_MAX_DISPLAY_DEVICE_NAME_SIZE_DXR 128

/*!
 * @brief Desktop geometry and stable identity of the monitor the 3D panel is
 * on, returned by xrGetSystemProperties (v18 addition).
 *
 * Supersedes XrDisplayDesktopPositionDXR, which carries only the origin. That
 * struct keeps working unchanged — this is a SEPARATE chained struct rather
 * than extra fields on it, because the runtime writes chained output structs
 * with its own compiled layout and appending to a published struct would write
 * past an app allocation compiled against v16.
 *
 * ## What it is for
 *
 * Nothing else in the wire protocol says WHERE the panel is, so a client that
 * creates its own window has to guess, and typically lands on the OS primary
 * monitor (see displayxr-unity#266). Matching displayPixelWidth/Height against
 * the OS monitor list is ambiguous the moment two monitors share a resolution.
 * Chain this struct instead, then place the window inside @ref desktopRect.
 *
 * ## Coordinate contract — read this before using desktopRect
 *
 * @ref desktopRect is in THE COORDINATE SPACE THE OS PLACES WINDOWS IN, which
 * is what a client needs and which differs per platform:
 *
 * - Windows: physical virtual-screen pixels (what a per-monitor-DPI-aware-v2
 *   process sees). The runtime pins a per-monitor-v2 DPI context while
 *   resolving, so the value is physical regardless of the host process's own
 *   DPI awareness.
 * - macOS: POINTS in the global display space, NOT backing pixels — a Retina
 *   panel's backing store is 2x this. Read displayPixelWidth/Height for pixels.
 * - Linux/X11: device pixels in root-window coordinates.
 *
 * All three are TOP-DOWN with the origin at the primary/main display's
 * top-left, and monitors left of or above it have NEGATIVE offsets. On macOS
 * that is deliberately the CoreGraphics convention, not Cocoa's — an app
 * placing an NSWindow must flip into NSScreen's bottom-up space itself, and one
 * that forgets will mirror its position vertically on any multi-display setup.
 *
 * A consumer MUST act in that same space. A DPI-UNAWARE process that passes
 * these coordinates to SetWindowPos (or any managed wrapper over it) is handed
 * virtualised coordinates by the OS and will place the window wrong by the
 * ratio of the two monitors' scale factors — correct on a single-monitor box,
 * wrong on exactly the mixed-DPI multi-monitor rigs this struct exists to
 * serve. Either declare per-monitor-v2 awareness for the process, or wrap the
 * placement call in SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2).
 *
 * ## desktopRect is not displayPixelWidth/Height
 *
 * XrDisplayInfoDXR::displayPixelWidth/Height are the panel's NATIVE
 * resolution. @ref desktopRect.extent is the monitor's CURRENT desktop mode.
 * They differ whenever the user runs a non-native mode. Use @ref desktopRect
 * for placement and displayPixel* for render sizing.
 *
 * @extends XrSystemProperties
 */
typedef struct XrDisplayDesktopInfoDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_DISPLAY_DESKTOP_INFO_DXR
    void* XR_MAY_ALIAS          next;       //!< Pointer to next structure in chain
    /*!
     * The panel monitor's full desktop rect in physical virtual-desktop
     * pixels. `offset` is the signed top-left; `extent` is the current desktop
     * mode's size. An all-zero rect means the runtime could not resolve it —
     * treat the geometry as unknown and fall back to your previous placement.
     */
    XrRect2Di                   desktopRect;
    /*!
     * Stable OS device name of the panel's monitor, NUL-terminated UTF-8, for
     * re-resolving the monitor after a hotplug or arrangement change (feed it
     * to EnumDisplayDevices, or match it against DXGI_OUTPUT_DESC::DeviceName).
     *
     * Windows: the GDI name, e.g. `\\.\DISPLAY1`.
     * macOS: the display UUID — deliberately not the CGDirectDisplayID, which
     * is reassigned across reboots and replug and so cannot survive the very
     * event this field exists for. Linux/X11: the RandR output name, e.g.
     * `HDMI-1`. Empty string when the runtime could not resolve one.
     */
    char                        deviceName[XR_MAX_DISPLAY_DEVICE_NAME_SIZE_DXR];
    /*!
     * XR_TRUE when the panel's monitor is the desktop's primary monitor, in
     * which case a client that only wants "open on the panel" can skip moving
     * its window entirely. Common in the field: making the 3D panel the
     * Windows primary is the standing workaround for the absence of this
     * struct, so machines that already worked around it report XR_TRUE.
     */
    XrBool32                    isPrimary;
    /*!
     * XR_TRUE when the resolved monitor's current mode matches the panel's
     * reported native resolution — i.e. the runtime genuinely located the 3D
     * panel.
     *
     * XR_FALSE means the display processor expressed no position and the
     * runtime fell back to the primary monitor. That happens with
     * `sim_display` (hardware-free development) and on the platforms whose
     * panel-origin plumbing is still open (#715). The rect is still a valid,
     * placeable monitor rect — it is just not evidence that a 3D panel is
     * there, so a client may prefer to leave its window where it is.
     */
    XrBool32                    isPanelConfirmed;
} XrDisplayDesktopInfoDXR;

/*!
 * @brief Hardware display state for xrRequestDisplayModeDXR (v15 repurpose).
 */
typedef enum XrDisplayModeDXR {
    XR_DISPLAY_MODE_2D_DXR = 0,
    XR_DISPLAY_MODE_3D_DXR = 1,
    XR_DISPLAY_MODE_MAX_ENUM_DXR = 0x7FFFFFFF
} XrDisplayModeDXR;

/*!
 * @brief Request the HARDWARE display state alone for the current mode
 * (v15 repurpose — was a deprecated mode-switching wrapper through v14).
 *
 * A rendering mode is a complete recipe: layout, view count, scales, and a
 * default hardware state. xrRequestDisplayRenderingModeDXR requests a mode
 * and the hardware state follows automatically. THIS function overrides the
 * hardware state ALONE: the active rendering mode, the app's submitted
 * content, and the display processor's atlas processing are untouched —
 * only the physical 2D/3D element (e.g. the switchable lenticular lens)
 * changes.
 *
 * E.g. XR_DISPLAY_MODE_2D_DXR over an active 3D mode keeps the weave
 * running with the lens off: the panel shows the woven atlas flat (blurry),
 * and an app fading its parallax to zero converges back to a sharp image —
 * the building block for app-authored 2D/3D transitions such as the MANUAL
 * eye-tracking loss flow.
 *
 * The override holds until the next mode request (whose default hardware
 * state then applies) or the next call to this function. A successful state
 * flip is reported via XrEventDataHardwareDisplayStateChangedDXR.
 *
 * @param session A valid XrSession handle.
 * @param displayMode The desired hardware state (2D or 3D).
 * @return XR_SUCCESS on success.
 */
typedef XrResult (XRAPI_PTR *PFN_xrRequestDisplayModeDXR)(XrSession session, XrDisplayModeDXR displayMode);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrRequestDisplayModeDXR(
    XrSession                                   session,
    XrDisplayModeDXR                            displayMode);
#endif

// ---- v6: Eye Tracking Mode Control ----

#define XR_TYPE_EYE_TRACKING_MODE_CAPABILITIES_DXR ((XrStructureType)1004999006)
#define XR_TYPE_VIEW_EYE_TRACKING_STATE_DXR        ((XrStructureType)1004999007)

/*!
 * @brief Eye tracking mode enum.
 *
 * MANAGED (0) is the default — apps that never call xrRequestEyeTrackingModeDXR
 * get current behavior (vendor SDK handles grace period + transitions).
 * MANUAL (1) provides unfiltered positions + explicit isTracking flag.
 */
typedef enum XrEyeTrackingModeDXR {
    XR_EYE_TRACKING_MODE_MANAGED_DXR  = 0,
    XR_EYE_TRACKING_MODE_MANUAL_DXR   = 1,
    XR_EYE_TRACKING_MODE_MAX_ENUM_DXR = 0x7FFFFFFF
} XrEyeTrackingModeDXR;

/*!
 * @brief Capability flags for eye tracking modes (bitmask).
 *
 * A value of 0 means the display has NO eye tracking capability at all.
 */
typedef XrFlags64 XrEyeTrackingModeCapabilityFlagsDXR;
static const XrEyeTrackingModeCapabilityFlagsDXR
    XR_EYE_TRACKING_MODE_CAPABILITY_NONE_DXR       = 0;
static const XrEyeTrackingModeCapabilityFlagsDXR
    XR_EYE_TRACKING_MODE_CAPABILITY_MANAGED_BIT_DXR = 0x00000001;
static const XrEyeTrackingModeCapabilityFlagsDXR
    XR_EYE_TRACKING_MODE_CAPABILITY_MANUAL_BIT_DXR = 0x00000002;

/*!
 * @brief Eye tracking mode capabilities — chained to XrSystemProperties.
 *
 * If supportedModes is 0 (NONE), the display has no eye tracking. In that
 * case defaultMode is undefined, xrRequestEyeTrackingModeDXR returns
 * XR_ERROR_FEATURE_UNSUPPORTED for any mode, and XrViewEyeTrackingStateDXR
 * always reports isTracking=XR_FALSE.
 *
 * xrLocateViews ALWAYS returns fully populated views (count, positions, FOVs)
 * regardless of tracking capability or state. The vendor SDK decides the view
 * positions (e.g., nominal viewer, last known, filtered). isTracking only
 * indicates whether those positions come from live eye tracking or a fallback.
 *
 * @extends XrSystemProperties
 */
typedef struct XrEyeTrackingModeCapabilitiesDXR {
    XrStructureType                        type;           //!< Must be XR_TYPE_EYE_TRACKING_MODE_CAPABILITIES_DXR
    void* XR_MAY_ALIAS                     next;
    XrEyeTrackingModeCapabilityFlagsDXR    supportedModes; //!< Bitmask of supported modes (0 = no tracking)
    XrEyeTrackingModeDXR                   defaultMode;    //!< Mode used if app never requests one
} XrEyeTrackingModeCapabilitiesDXR;

/*!
 * @brief Per-frame eye tracking state — chained to XrViewState in xrLocateViews.
 *
 * xrLocateViews ALWAYS returns fully populated views (positions, FOVs)
 * regardless of isTracking. When isTracking is XR_FALSE, positions are
 * still valid — the vendor SDK populates them as it sees fit (e.g., last
 * known, nominal viewer, filtered). isTracking tells the app whether
 * positions come from live eye tracking or vendor-chosen fallback.
 * The app may use isTracking to trigger its own animations or UI.
 *
 * @extends XrViewState
 */
typedef struct XrViewEyeTrackingStateDXR {
    XrStructureType           type;       //!< Must be XR_TYPE_VIEW_EYE_TRACKING_STATE_DXR
    void* XR_MAY_ALIAS        next;
    XrBool32                  isTracking; //!< XR_TRUE if eyes are actively tracked this frame
    XrEyeTrackingModeDXR     activeMode; //!< Currently active mode
} XrViewEyeTrackingStateDXR;

/*!
 * @brief Request eye tracking mode switch.
 *
 * Switches between managed and manual eye tracking modes. In managed mode,
 * the vendor SDK handles grace period + transitions internally. In manual mode,
 * the SDK provides unfiltered positions and the app uses isTracking to
 * handle tracking loss.
 *
 * @param session A valid XrSession handle.
 * @param mode The desired eye tracking mode.
 * @return XR_SUCCESS on success,
 *         XR_ERROR_FEATURE_UNSUPPORTED if the mode is not supported,
 *         XR_ERROR_VALIDATION_FAILURE if mode is invalid.
 */
typedef XrResult (XRAPI_PTR *PFN_xrRequestEyeTrackingModeDXR)(
    XrSession session, XrEyeTrackingModeDXR mode);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrRequestEyeTrackingModeDXR(
    XrSession               session,
    XrEyeTrackingModeDXR    mode);
#endif

// ---- v7: Display Rendering Mode Control ----

/*!
 * @brief Request a vendor-specific display rendering mode.
 *
 * Different 3D display vendors support multiple rendering variations
 * (e.g., side-by-side stereo, anaglyph, lenticular). This function lets
 * the application switch between them at runtime.
 *
 * Mode indices are vendor-defined:
 *   - Mode 0 = standard rendering (always available)
 *   - Mode 1+ = vendor-specific variations
 *
 * The runtime dispatches the request to the active display device, which
 * may accept or silently ignore unsupported indices.
 *
 * @param session A valid XrSession handle.
 * @param modeIndex The vendor-defined rendering mode index.
 * @return XR_SUCCESS on success.
 */
typedef XrResult (XRAPI_PTR *PFN_xrRequestDisplayRenderingModeDXR)(
    XrSession session, uint32_t modeIndex);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrRequestDisplayRenderingModeDXR(
    XrSession               session,
    uint32_t                modeIndex);
#endif

// ---- v8: Rendering Mode Enumeration ----

#define XR_TYPE_DISPLAY_RENDERING_MODE_INFO_DXR ((XrStructureType)1004999008)

/*!
 * @brief Information about a single display rendering mode.
 *
 * Returned by xrEnumerateDisplayRenderingModesDXR to describe each available
 * vendor-specific rendering mode (e.g., side-by-side, anaglyph, lenticular).
 */
typedef struct XrDisplayRenderingModeInfoDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_DISPLAY_RENDERING_MODE_INFO_DXR
    void* XR_MAY_ALIAS          next;       //!< Pointer to next structure in chain
    uint32_t                    modeIndex;  //!< Vendor-defined mode index (pass to xrRequestDisplayRenderingModeDXR)
    char                        modeName[XR_MAX_SYSTEM_NAME_SIZE]; //!< Human-readable mode name
    uint32_t                    viewCount;  //!< Number of views (1=mono, 2=stereo, etc.)
    float                       viewScaleX; //!< Per-view horizontal scale (vendor-provided)
    float                       viewScaleY; //!< Per-view vertical scale (vendor-provided)
    XrBool32                    hardwareDisplay3D;  //!< Whether display hardware is in 3D mode
    uint32_t                    tileColumns;     //!< Tile columns in atlas layout (v12)
    uint32_t                    tileRows;        //!< Tile rows in atlas layout (v12)
    uint32_t                    viewWidthPixels; //!< Per-view width in pixels (v12)
    uint32_t                    viewHeightPixels;//!< Per-view height in pixels (v12)
    /*!
     * (v13) True for the mode that is currently active for this session.
     *
     * Apps can read this at startup (after xrCreateSession + first
     * xrEnumerateDisplayRenderingModesDXR call) to learn the current mode
     * without waiting for an XrEventDataRenderingModeChangedDXR — useful
     * when the session begins under a workspace that already chose a mode.
     * Re-enumerating after a mode change reflects the new active mode.
     */
    XrBool32                    isActive;
    /*!
     * (v13) True iff this session may request this mode via
     * xrRequestDisplayRenderingModeDXR.
     *
     * False for non-controller sessions running under a workspace — the
     * workspace controller is the sole mode authority and app requests are
     * dropped by the runtime. Apps can use this to gate their UI (e.g.
     * disable the V toggle and show "Mode locked by workspace"). Always
     * true for standalone sessions and for workspace-controller sessions.
     */
    XrBool32                    isRequestable;
} XrDisplayRenderingModeInfoDXR;

/*!
 * @brief Enumerate available display rendering modes.
 *
 * Standard OpenXR two-call enumerate pattern. First call with
 * modeCapacityInput=0 to query modeCountOutput, then allocate and call again.
 *
 * @param session              A valid XrSession handle.
 * @param modeCapacityInput    Capacity of the modes array (0 to query count).
 * @param modeCountOutput      Output: number of modes available.
 * @param modes                Output array of mode info structs.
 * @return XR_SUCCESS on success, XR_ERROR_SIZE_INSUFFICIENT if capacity too small.
 */
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateDisplayRenderingModesDXR)(
    XrSession session,
    uint32_t modeCapacityInput,
    uint32_t *modeCountOutput,
    XrDisplayRenderingModeInfoDXR *modes);

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateDisplayRenderingModesDXR(
    XrSession                           session,
    uint32_t                            modeCapacityInput,
    uint32_t                           *modeCountOutput,
    XrDisplayRenderingModeInfoDXR      *modes);
#endif

// ---- v10: Unified Display Mode Events ----

#define XR_TYPE_EVENT_DATA_RENDERING_MODE_CHANGED_DXR        ((XrStructureType)1004999010)
#define XR_TYPE_EVENT_DATA_HARDWARE_DISPLAY_STATE_CHANGED_DXR ((XrStructureType)1004999011)

/*!
 * @brief Event fired when the active rendering mode changes.
 *
 * Pushed by xrRequestDisplayRenderingModeDXR on every actual mode change.
 *
 * @extends XrEventDataBaseHeader
 */
typedef struct XrEventDataRenderingModeChangedDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_EVENT_DATA_RENDERING_MODE_CHANGED_DXR
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    uint32_t                    previousModeIndex;
    uint32_t                    currentModeIndex;
} XrEventDataRenderingModeChangedDXR;

/*!
 * @brief Event fired when the physical display hardware state changes.
 *
 * Pushed by xrRequestDisplayRenderingModeDXR only when the hardware 3D
 * state flips (i.e., when switching between modes with different
 * hardwareDisplay3D values).
 *
 * @extends XrEventDataBaseHeader
 */
typedef struct XrEventDataHardwareDisplayStateChangedDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_EVENT_DATA_HARDWARE_DISPLAY_STATE_CHANGED_DXR
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    XrBool32                    hardwareDisplay3D;
} XrEventDataHardwareDisplayStateChangedDXR;

// xrSetSharedTextureOutputRectDXR was removed (ADR-031); display-zones (XR_DXR_display_zones) is the sole region paradigm

// ---- v14: Per-Mode Tracking Capability + Tracking-State Event (#441) ----

#define XR_TYPE_DISPLAY_RENDERING_MODE_TRACKING_INFO_DXR  ((XrStructureType)1004999012)
#define XR_TYPE_EVENT_DATA_EYE_TRACKING_STATE_CHANGED_DXR ((XrStructureType)1004999013)

/*!
 * @brief Per-mode tracking capability — chained by the APP to each
 * XrDisplayRenderingModeInfoDXR element's next before calling
 * xrEnumerateDisplayRenderingModesDXR.
 *
 * OPT-IN HANDSHAKE: to use the chain, the app MUST pre-set each array
 * element's type to XR_TYPE_DISPLAY_RENDERING_MODE_INFO_DXR (standard OpenXR
 * input convention) and set next to this struct (or a chain containing it).
 * The runtime only walks elements carrying that type — v13-and-earlier
 * binaries leave type/next uninitialized, and the runtime keeps overwriting
 * next with NULL for them, exactly as before.
 *
 * hasTracking tells whether the rendering mode consumes live eye tracking
 * (e.g., a tracked 3D mode or a "2D tracked" mode) or is fully untracked
 * (e.g., SBS/anaglyph export modes; every sim_display mode). When the ACTIVE
 * mode has hasTracking == XR_FALSE, XrViewEyeTrackingStateDXR.isTracking is
 * always XR_FALSE — regardless of tracker state. xrLocateViews still returns
 * fully populated views in every mode.
 *
 * LAYOUT-FREEZE POLICY: XrDisplayRenderingModeInfoDXR is frozen at its v13
 * layout. The runtime's enumerate fill writes array elements with its own
 * compiled stride, so appending fields (as v12/v13 did) silently corrupts app
 * binaries compiled against older headers. All future per-mode fields MUST be
 * added as chained structs like this one.
 *
 * @extends XrDisplayRenderingModeInfoDXR
 */
typedef struct XrDisplayRenderingModeTrackingInfoDXR {
    XrStructureType             type;        //!< Must be XR_TYPE_DISPLAY_RENDERING_MODE_TRACKING_INFO_DXR
    void* XR_MAY_ALIAS          next;
    XrBool32                    hasTracking; //!< Mode consumes live eye tracking
} XrDisplayRenderingModeTrackingInfoDXR;

/*!
 * @brief Event fired on every edge of the derived isTracking value.
 *
 * The runtime derives isTracking as
 *   activeMode.hasTracking && dp.is_tracking
 * and queues this event whenever the value changes — on DP-reported tracking
 * loss/recovery AND on rendering-mode switches into/out of untracked modes.
 *
 * This is the primary tracking-loss notification for MANUAL eye-tracking mode
 * (detect isTracking == XR_FALSE, run your own transition, request a 2D mode
 * when ready). It also fires in MANAGED mode, where apps may ignore it.
 * Edge detection runs in the runtime's xrLocateViews path, so a session that
 * never locates views receives no events.
 *
 * @extends XrEventDataBaseHeader
 */
typedef struct XrEventDataEyeTrackingStateChangedDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_EVENT_DATA_EYE_TRACKING_STATE_CHANGED_DXR
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    XrBool32                    isTracking; //!< New state
    XrEyeTrackingModeDXR        activeMode; //!< Session's MANAGED/MANUAL preference at edge time
} XrEventDataEyeTrackingStateChangedDXR;

// ---- v17: Display-mode request denial (panel lease, ADR-035 D2 / #961) ----

#define XR_TYPE_EVENT_DATA_DISPLAY_MODE_REQUEST_DENIED_DXR ((XrStructureType)1004999014)

//! Sentinel for XrEventDataDisplayModeRequestDeniedDXR::requestedModeIndex when
//! the denied request carried no content mode (a pure xrRequestDisplayModeDXR).
#define XR_DISPLAY_MODE_INDEX_NONE_DXR 0xFFFFFFFFu

/*!
 * @brief Why a display-mode request was denied.
 *
 * The runtime holds a single PANEL LEASE: while a workspace controller is
 * active it owns the display mode; otherwise the built-in policy grants it to
 * the focused session. A request from any other session is not applied and is
 * NOT queued for later — this event is the only answer it gets. A request that
 * the display hardware itself refuses (DISPLAY_PROCESSOR_REJECTED) leaves the
 * runtime's reported mode unchanged.
 */
typedef enum XrDisplayModeDenialReasonDXR {
    XR_DISPLAY_MODE_DENIAL_REASON_NONE_DXR = 0,
    XR_DISPLAY_MODE_DENIAL_REASON_WORKSPACE_OWNS_MODE_DXR = 1,        //!< a workspace controller owns the panel
    XR_DISPLAY_MODE_DENIAL_REASON_NOT_FOCUSED_DXR = 2,                //!< no controller; only the focused session may request
    XR_DISPLAY_MODE_DENIAL_REASON_NO_DISPLAY_PROCESSOR_DXR = 3,       //!< nothing to apply the request to
    XR_DISPLAY_MODE_DENIAL_REASON_DISPLAY_PROCESSOR_REJECTED_DXR = 4, //!< the display hardware refused the state
    XR_DISPLAY_MODE_DENIAL_REASON_RELAY_OWNS_MODE_DXR = 5,            //!< a relay owns this session's mode (mechanism reserved; no relay ships today)
    XR_DISPLAY_MODE_DENIAL_REASON_VIEW_CONFIG_CANNOT_FILL_DXR = 6,    //!< (v20) the session's view configuration reports fewer views than the mode needs
    XR_DISPLAY_MODE_DENIAL_REASON_MAX_ENUM_DXR = 0x7FFFFFFF
} XrDisplayModeDenialReasonDXR;

/*!
 * @brief Event delivered to the requesting session when its
 * xrRequestDisplayRenderingModeDXR / xrRequestDisplayModeDXR call was denied.
 *
 * Both entry points return XR_SUCCESS at call time (the request is forwarded
 * to the runtime's mode owner); this event, or a
 * XrEventDataRenderingModeChangedDXR / XrEventDataHardwareDisplayStateChangedDXR,
 * is the outcome. requestedModeIndex is XR_DISPLAY_MODE_INDEX_NONE_DXR when the
 * request carried no content mode; requestedHardware3D is -1 when it carried no
 * hardware state, else 0 (2D) / 1 (3D).
 *
 * @extends XrEventDataBaseHeader
 */
typedef struct XrEventDataDisplayModeRequestDeniedDXR {
    XrStructureType               type;       //!< Must be XR_TYPE_EVENT_DATA_DISPLAY_MODE_REQUEST_DENIED_DXR
    const void* XR_MAY_ALIAS      next;
    XrSession                     session;
    uint32_t                      requestedModeIndex;
    int32_t                       requestedHardware3D;
    XrDisplayModeDenialReasonDXR  reason;
} XrEventDataDisplayModeRequestDeniedDXR;

// ---- v21: Per-frame view activity — the fixed-count submission contract (ADR-041, Model E) ----

#define XR_TYPE_VIEW_ACTIVITY_STATE_DXR ((XrStructureType)1004999213)

/*!
 * @brief How many of this frame's located views carry live content.
 *
 * Chained to XrViewState in xrLocateViews. The VIEW COUNT IS FIXED for the
 * lifetime of the session — it is whatever the begun view configuration
 * reports (exactly 2 under XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, the
 * device maximum under XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MULTIVIEW_DXR) and
 * never changes when the display's rendering mode does. What changes per frame
 * is how many of those views are ACTIVE:
 *
 *   - views [0, activeViewCount) carry the active rendering mode's viewer
 *     poses and FOVs. These are the views the runtime composes.
 *   - views [activeViewCount, viewCountOutput) are INACTIVE. The runtime
 *     locates them at view 0's pose so they are always valid to render with,
 *     and IGNORES whatever the app submits for them.
 *
 * THE SUBMISSION CONTRACT (all view configuration types): xrEndFrame's
 * XrCompositionLayerProjection::viewCount must equal the count xrLocateViews
 * returned — that is what core OpenXR requires ("All views associated with
 * projection layers must be supplied"). An app that renders only the active
 * views satisfies it by ALIASING the inactive tail: give each inactive view
 * any valid subImage of a swapchain it rendered this frame (view 0's is the
 * obvious choice) while keeping that view's own located pose/fov. The runtime
 * never reads those pixels.
 *
 * Not chaining this struct is legal: an app that always renders every located
 * view needs nothing from it (the inactive tail then simply duplicates view 0
 * and is discarded).
 *
 * Filled only on the call that RETURNS views, i.e. `viewCapacityInput > 0`. The
 * two-call count query (`viewCapacityInput == 0`) returns before any view is
 * located and therefore writes nothing here — the same rule the rest of
 * XrViewState already follows. Initialise the field and read it back after the
 * second call.
 *
 * DEPRECATED, still accepted: an app that enabled this extension and began
 * PRIMARY_STEREO or PRIMARY_MULTIVIEW_DXR may submit a 1-view projection layer
 * while the active mode is itself 1-view. That is the pre-v21 2D submission path; it logs a one-shot
 * warning and is removed once the SDK + the shipped demos alias instead. The
 * DXR_UNDER_SUBMIT runtime switch selects the behaviour (see
 * docs/specs/extensions/XR_DXR_display_info.md).
 *
 * @extends XrViewState
 */
typedef struct XrViewActivityStateDXR {
    XrStructureType           type;            //!< Must be XR_TYPE_VIEW_ACTIVITY_STATE_DXR
    void* XR_MAY_ALIAS        next;
    uint32_t                  activeViewCount; //!< Views [0, activeViewCount) are live; the rest are inactive aliases
} XrViewActivityStateDXR;

// ---- v22: Per-display enumeration, display spaces, session display binding (multi-screen M1) ----

// Type values 1004999214-1004999217 (display_info's 210-219 decade, see
// README.md): 214-216 per-display enumeration (multi-screen M1), 217 the
// per-view display bindings chained on XrViewState (multi-screen M3);
// 218 is the v23 displays-changed event (below); 219 is free.
#define XR_TYPE_DISPLAY_DXR                       ((XrStructureType)1004999214)
#define XR_TYPE_DISPLAY_SPACE_CREATE_INFO_DXR     ((XrStructureType)1004999215)
#define XR_TYPE_SESSION_DISPLAY_BINDING_DXR       ((XrStructureType)1004999216)
#define XR_TYPE_VIEW_DISPLAY_BINDINGS_DXR         ((XrStructureType)1004999217)

//! Size of XrDisplayDXR::vendorPluginId, in bytes, including the NUL.
#define XR_MAX_DISPLAY_VENDOR_PLUGIN_ID_SIZE_DXR 64
//! Size of XrDisplayDXR::deviceName, in bytes, including the NUL.
#define XR_MAX_DISPLAY_NAME_SIZE_DXR 64

/*!
 * @brief Per-display flags reported in XrDisplayDXR::flags (v22).
 */
typedef XrFlags64 XrDisplayFlagsDXR;
//! The OS desktop's primary monitor.
static const XrDisplayFlagsDXR XR_DISPLAY_PRIMARY_BIT_DXR = 0x00000001;
//! The display has live eye tracking (eyeTracking.supportedModes != 0).
static const XrDisplayFlagsDXR XR_DISPLAY_TRACKED_BIT_DXR = 0x00000002;
/*!
 * The SYSTEM DEFAULT display: the one whose information XrDisplayInfoDXR,
 * XrDisplayDesktopInfoDXR and XrEyeTrackingModeCapabilitiesDXR on
 * XrSystemProperties report. Exactly one enumerated display carries it.
 */
static const XrDisplayFlagsDXR XR_DISPLAY_SYSTEM_DEFAULT_BIT_DXR = 0x00000004;

/*!
 * @brief One physical display (monitor) the runtime knows about, returned by
 * xrEnumerateDisplaysDXR (v22).
 *
 * The list contains every connected monitor the runtime identified — the 3D
 * panel(s) a vendor display processor claims AND ordinary monitors served by
 * the vendor-neutral fallback — so an app can tell which screen is 3D
 * (TRACKED, a vendor plug-in id) and place itself accordingly.
 *
 * @ref info is EMBEDDED BY VALUE (its `type`/`next` are written as
 * XR_TYPE_DISPLAY_INFO_DXR / NULL by the runtime; the app need not initialise
 * them), so the struct is one fixed-size record per display. For the
 * SYSTEM_DEFAULT display, @ref info and @ref eyeTracking carry exactly what
 * XrSystemProperties reports.
 *
 * Coordinate contract for @ref desktopRect: identical to
 * XrDisplayDesktopInfoDXR::desktopRect (the space the OS places windows in;
 * Linux/X11: root-window coordinates). An all-zero extent means unknown.
 */
typedef struct XrDisplayDXR {
    XrStructureType                  type;       //!< Must be XR_TYPE_DISPLAY_DXR
    void* XR_MAY_ALIAS               next;
    /*!
     * Opaque, non-zero identifier of this display. Where the platform names
     * the connector (desktop Linux) it is derived from the monitor's EDID
     * identity + connector only: it survives the user rearranging monitors
     * and is the same in every process, and changes only when a different
     * panel is plugged into that port, the panel moves port, or the kernel
     * renumbers the GPU (the connector key includes its card, e.g.
     * "card1-HDMI-A-1"). Where it does
     * not (Windows today) it also covers the desktop position, so a
     * rearrangement yields new ids. Either way, re-enumerate after a topology
     * change and treat an id that is no longer listed as gone. Pass it to
     * XrDisplaySpaceCreateInfoDXR and XrSessionDisplayBindingDXR.
     */
    uint64_t                         displayId;
    //! Physical size, nominal viewer, recommended view scale and native pixels of THIS display.
    XrDisplayInfoDXR                 info;
    //! The monitor's current desktop rect (offset = signed top-left, extent = current mode).
    XrRect2Di                        desktopRect;
    //! The connector's device (native) mode; 0 = unknown.
    uint32_t                         nativePixelWidth;
    uint32_t                         nativePixelHeight;
    //! The desktop compositor's scale for this output (e.g. 1.5); 0 = unknown.
    float                            desktopScale;
    //! Eye-tracking capabilities of THIS display (`type`/`next` written by the runtime).
    XrEyeTrackingModeCapabilitiesDXR eyeTracking;
    //! Bitmask of XR_DISPLAY_*_BIT_DXR.
    XrDisplayFlagsDXR                flags;
    //! Discovery id of the vendor plug-in that drives this display (e.g. "leia-sr", "sim-display"), NUL-terminated.
    char                             vendorPluginId[XR_MAX_DISPLAY_VENDOR_PLUGIN_ID_SIZE_DXR];
    //! OS output name (Linux/X11: RandR output, e.g. "HDMI-1"; Windows: GDI name), NUL-terminated; "" = unknown.
    char                             deviceName[XR_MAX_DISPLAY_NAME_SIZE_DXR];
} XrDisplayDXR;

/*!
 * @brief Enumerate the system's displays (v22). Standard two-call idiom.
 *
 * The list is snapshotted when the call is made (see XrDisplayDXR::displayId
 * for how long an id lasts). Order: the SYSTEM_DEFAULT display first, then the rest in the
 * runtime's monitor-enumeration order.
 *
 * @param instance          A valid XrInstance with XR_DXR_display_info enabled.
 * @param systemId          The XrSystemId from xrGetSystem.
 * @param displayCapacityInput Capacity of @p displays, or 0 to query the count.
 * @param displayCountOutput   Receives the number of displays (or the required capacity).
 * @param displays          Array of XrDisplayDXR with `type` set to XR_TYPE_DISPLAY_DXR; may be NULL when capacity is 0.
 * @return XR_SUCCESS, XR_ERROR_SIZE_INSUFFICIENT, XR_ERROR_SYSTEM_INVALID, XR_ERROR_VALIDATION_FAILURE.
 */
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateDisplaysDXR)(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    displayCapacityInput,
    uint32_t*                                   displayCountOutput,
    XrDisplayDXR*                               displays);

/*!
 * @brief Create info for a DISPLAY space (v22).
 *
 * A DISPLAY space is a reference-space-like XrSpace whose origin is the
 * centre of @ref displayId's visible area: +X right, +Y up, +Z toward the
 * viewer (the display space of docs/architecture/kooima-projection.md), with
 * @ref poseInDisplaySpace applied on top exactly like
 * XrReferenceSpaceCreateInfo::poseInReferenceSpace.
 *
 * Spec v22 limit: every DISPLAY space resolves to the session's single
 * display plane — the same pose XrViewDisplayRawDXR::displayPlanePose
 * reports — whichever display it names. Per-display poses (and therefore
 * meaningful relationships BETWEEN two displays' spaces) arrive with
 * per-screen DISPLAY poses in a later revision; there is no shared room frame.
 */
typedef struct XrDisplaySpaceCreateInfoDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_DISPLAY_SPACE_CREATE_INFO_DXR
    const void* XR_MAY_ALIAS    next;
    uint64_t                    displayId;  //!< An id from xrEnumerateDisplaysDXR
    XrPosef                     poseInDisplaySpace;
} XrDisplaySpaceCreateInfoDXR;

/*!
 * @brief Create a DISPLAY space (v22).
 * @return XR_SUCCESS, XR_ERROR_VALIDATION_FAILURE (unknown displayId),
 *         XR_ERROR_POSE_INVALID, XR_ERROR_SESSION_LOST, ...
 */
typedef XrResult (XRAPI_PTR *PFN_xrCreateDisplaySpaceDXR)(
    XrSession                                   session,
    const XrDisplaySpaceCreateInfoDXR*          createInfo,
    XrSpace*                                    space);

/*!
 * @brief Pin a session to one display (v22). Chained on XrSessionCreateInfo.
 *
 * @ref displayId 0 means "runtime decides" (identical to not chaining the
 * struct). Any other value must be an id from xrEnumerateDisplaysDXR, or
 * xrCreateSession fails with XR_ERROR_VALIDATION_FAILURE.
 *
 * Spec v22 effect: the bound display's physical size and nominal viewer
 * become the session's untracked-Kooima inputs (the display-scoped projection
 * an app gets before window metrics are known). Which display processor
 * weaves the session is unchanged — honouring the binding in the compositor
 * arrives with multi-display segmentation; XrSystemProperties keeps reporting
 * the SYSTEM_DEFAULT display.
 *
 * @extends XrSessionCreateInfo
 */
typedef struct XrSessionDisplayBindingDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_SESSION_DISPLAY_BINDING_DXR
    const void* XR_MAY_ALIAS    next;
    uint64_t                    displayId;  //!< 0 = runtime decides
} XrSessionDisplayBindingDXR;

/*!
 * @brief Which views of a locate belong to which display (v22, multi-screen M3).
 *
 * One entry of XrViewDisplayBindingsDXR::bindings: the contiguous view range
 * [firstView, firstView + viewCount) was located for the part of the window
 * that lands on display @ref displayId — the window SEGMENT @ref segmentRect.
 */
typedef struct XrViewDisplayBindingDXR {
    //! The display these views are for (an id from xrEnumerateDisplaysDXR).
    uint64_t    displayId;
    /*!
     * The segment: the part of the window's canvas on that display, in WINDOW
     * pixels (the client area, origin top-left). Render these views into a
     * tile of `segmentRect.extent * the active mode's view scale`; the
     * runtime places that tile at the segment's position in each view tile.
     */
    XrRect2Di   segmentRect;
    //! First view of the range (index into the XrView array xrLocateViews filled).
    uint32_t    firstView;
    //! Number of views in the range (the active rendering mode's view count).
    uint32_t    viewCount;
} XrViewDisplayBindingDXR;

/*!
 * @brief Per-view display bindings of a locate (v22, multi-screen M3).
 *
 * Chained to XrViewState in xrLocateViews; standard two-call idiom on the
 * binding array (`bindingCapacityInput == 0` returns the count in
 * `bindingCountOutput` and writes nothing else; a capacity smaller than the
 * count writes the count and no bindings — xrLocateViews itself still
 * succeeds, the views are always located).
 *
 * A window that spans displays is woven per display. Under
 * XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MULTIVIEW_DXR each 3D display the window
 * covers then gets its OWN views: the active rendering mode's view count per
 * display, located from that display's eyes with that display's segment as
 * the Kooima canvas, laid out CONTIGUOUSLY per display in the order of this
 * array (left to right on the desktop). XrViewActivityStateDXR::activeViewCount
 * is the sum. The inactive tail aliases the last active view, exactly as in
 * the single-display case.
 *
 * bindingCountOutput is the number of per-display view sets of this locate —
 * one per display segment while the views are located per display (2 for a
 * window spanning two displays; 1 for a window lying wholly on a display other
 * than the system default) — and 0 whenever the views are ONE set for the
 * whole window: a window on the system-default display only, no window yet, an
 * IPC/service session, a camera-rig or zone-scoped locate, or a session that
 * began XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO (a stereo session keeps its
 * two views, framed from the display holding most of the window, and each other
 * display shows its crop of them or a flat 2D copy). An app that never chains
 * this struct still renders correctly: render every active view, each into its
 * subImage.
 *
 * View poses of every display are expressed in ONE frame: the display holding
 * most of the window. Each other display's segment is placed beside it the way
 * the window's pixels continue across the seam, so a scene spanning the seam
 * stays continuous.
 *
 * @extends XrViewState
 */
typedef struct XrViewDisplayBindingsDXR {
    XrStructureType             type;                 //!< Must be XR_TYPE_VIEW_DISPLAY_BINDINGS_DXR
    void* XR_MAY_ALIAS          next;
    uint32_t                    bindingCapacityInput; //!< Capacity of @ref bindings, or 0 to query the count
    uint32_t                    bindingCountOutput;   //!< Number of bindings (or the required capacity)
    XrViewDisplayBindingDXR*    bindings;             //!< Array of bindingCapacityInput entries; may be NULL when the capacity is 0
} XrViewDisplayBindingsDXR;

// ---- v23: Display topology change event (display hotplug, ADR-047 Amendment 8) ----

// Type value 1004999218 (display_info's 210-219 decade, see README.md).
#define XR_TYPE_EVENT_DATA_DISPLAYS_CHANGED_DXR ((XrStructureType)1004999218)

/*!
 * @brief The system's display list changed while the session ran (v23).
 *
 * Queued once per change, to every running session, when a display was
 * plugged in or removed, was re-claimed by another display processor (a 3D
 * panel whose vendor service came up after the app started), or moved / changed
 * mode. The runtime has ALREADY applied it: a window on a newly claimed 3D
 * panel is woven by that panel's display processor from the next frame, with
 * no session restart; one on a removed display is no longer woven for it.
 *
 * Re-enumerate with xrEnumerateDisplaysDXR to see the new list: an id that is
 * no longer listed is gone, and a listed id's vendorPluginId / flags / info may
 * differ. Nothing else is required of the app. XrSystemProperties, the view
 * configuration view counts (ADR-041) and the swapchain sizing stay fixed for
 * the session; per-segment views (multi-screen M3) follow the new list within
 * that fixed view count.
 *
 * @extends XrEventDataBaseHeader
 */
typedef struct XrEventDataDisplaysChangedDXR {
    XrStructureType             type;         //!< Must be XR_TYPE_EVENT_DATA_DISPLAYS_CHANGED_DXR
    const void* XR_MAY_ALIAS    next;
    XrSession                   session;
    uint32_t                    displayCount; //!< How many displays xrEnumerateDisplaysDXR lists now
} XrEventDataDisplaysChangedDXR;

#ifndef XR_NO_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateDisplaysDXR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    displayCapacityInput,
    uint32_t*                                   displayCountOutput,
    XrDisplayDXR*                               displays);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateDisplaySpaceDXR(
    XrSession                                   session,
    const XrDisplaySpaceCreateInfoDXR*          createInfo,
    XrSpace*                                    space);
#endif

#ifdef __cplusplus
}
#endif

#endif // XR_DXR_DISPLAY_INFO_H
