// Copyright 2025, The DisplayXR Project
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
 * @brief  Header for XR_DXR_win32_window_binding extension
 * @author David Fattal
 * @ingroup external_openxr
 *
 * This extension allows an OpenXR application to provide its own window handle
 * (HWND on Windows) to the runtime. When provided, the runtime will render
 * into the application's window instead of creating its own window.
 *
 * This enables:
 * - Windowed mode rendering (vs fullscreen)
 * - Application control over window input (keyboard, mouse)
 * - Multiple OpenXR applications on the same display
 * - Offscreen readback: windowHandle=NULL + readbackCallback → composited
 *   pixels delivered to callback instead of presented to a window
 * - Zero-copy shared texture: windowHandle=NULL + sharedTextureHandle →
 *   runtime composites into a shared D3D11/D3D12 texture (HANDLE)
 */
#ifndef XR_DXR_WIN32_WINDOW_BINDING_H
#define XR_DXR_WIN32_WINDOW_BINDING_H 1

#include <openxr/openxr.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XR_DXR_win32_window_binding 1
#define XR_DXR_win32_window_binding_SPEC_VERSION 9
#define XR_DXR_WIN32_WINDOW_BINDING_EXTENSION_NAME "XR_DXR_win32_window_binding"

// Use a value in the vendor extension range (1000000000+)
// This should be replaced with an official Khronos-assigned value if the extension is standardized
#define XR_TYPE_WIN32_WINDOW_BINDING_CREATE_INFO_DXR ((XrStructureType)1004999001)
// SPEC_VERSION 9: texture target binding (ADR-060).
#define XR_TYPE_WIN32_TEXTURE_TARGET_BINDING_DXR ((XrStructureType)1004999015)
#define XR_TYPE_WIN32_TEXTURE_TARGET_FRAME_DXR ((XrStructureType)1004999016)
#define XR_TYPE_SYSTEM_WIN32_TEXTURE_TARGET_PROPERTIES_DXR ((XrStructureType)1004999017)
#define XR_TYPE_WIN32_TEXTURE_TARGET_STATE_DXR ((XrStructureType)1004999018)

/*!
 * @brief Callback for offscreen readback mode (CPU fallback).
 *
 * DEPRECATED: new code must pass NULL. The runtime still honours it; no shipped
 * consumer passes a non-NULL value. Shared by the win32 and cocoa bindings.
 *
 * @param pixels   Pointer to RGBA pixel data (width * height * 4 bytes)
 * @param width    Image width in pixels
 * @param height   Image height in pixels
 * @param userdata Opaque pointer from readbackUserdata
 */
#ifndef XR_DXR_PFN_READBACK_CALLBACK_DEFINED
#define XR_DXR_PFN_READBACK_CALLBACK_DEFINED 1
typedef void (*PFN_xrReadbackCallback)(
    const uint8_t *pixels, uint32_t width, uint32_t height, void *userdata);
#endif

/*!
 * @brief Structure passed in XrSessionCreateInfo::next chain to provide
 *        an external window handle for session rendering.
 *
 * When this structure is provided in the next chain of XrSessionCreateInfo,
 * the runtime will render into the specified window instead of creating
 * its own window. The application is responsible for:
 * - Creating and managing the window lifecycle
 * - Handling the window message pump
 * - Processing input events
 *
 * Alternatively, set windowHandle to NULL and provide either:
 * - readbackCallback for CPU-side offscreen readback (GPU→CPU round-trip), or
 * - sharedTextureHandle for zero-copy GPU texture sharing (D3D11/D3D12 HANDLE)
 *
 * @extends XrSessionCreateInfo
 */
typedef struct XrWin32WindowBindingCreateInfoDXR {
    XrStructureType             type;                  //!< Must be XR_TYPE_WIN32_WINDOW_BINDING_CREATE_INFO_DXR
    const void* XR_MAY_ALIAS    next;                  //!< Pointer to next structure in chain
    void*                       windowHandle;          //!< HWND of the target window, or NULL
    PFN_xrReadbackCallback      readbackCallback;      //!< DEPRECATED, pass NULL (offscreen CPU readback)
    void*                       readbackUserdata;      //!< Passed to readbackCallback
    void*                       sharedTextureHandle;   //!< Shared D3D11/D3D12 texture HANDLE for zero-copy, or NULL
    //! When XR_TRUE, the runtime configures the bound HWND for transparent desktop
    //! composition: pixels written by the app with full opacity appear opaque on screen,
    //! and pixels written transparent (alpha = 0) compose through to the desktop
    //! underneath.
    //! The runtime picks the appropriate DXGI / Windows mechanism per graphics API;
    //! apps should not depend on which one. Only honored when windowHandle is non-NULL
    //! and the session is standalone — ignored in workspace/shell mode.
    //! Transparency is carried by the swapchain's per-pixel alpha and a transparent
    //! present (SPEC_VERSION 8, #573 removed the legacy chromaKeyColor field).
    XrBool32                    transparentBackgroundEnabled;
} XrWin32WindowBindingCreateInfoDXR;

// Window-space composition layer. Every window/surface binding extension
// (win32, cocoa, xlib, wayland, android) enables it and declares it with this
// SAME guarded block, so any combination of these headers can be included in
// any order. The guard is the type value, shared by all five copies.
#ifndef XR_TYPE_COMPOSITION_LAYER_WINDOW_SPACE_DXR
#define XR_TYPE_COMPOSITION_LAYER_WINDOW_SPACE_DXR ((XrStructureType)1004999002)

/*!
 * @brief Composition layer positioned in fractional window coordinates.
 *
 * Renders a textured quad at a position specified as fractions of the target
 * window dimensions. Coordinates automatically scale when the window is resized.
 * The same texture is composited into every view with a per-eye horizontal
 * shift controlled by the disparity parameter. The layer is composited before
 * the display processor (it is woven like any other layer).
 *
 * Valid on a session created with a window/surface binding, on a session of a
 * native compositor, and on an IPC (service) session.
 *
 * @extends XrFrameEndInfo (submitted as a composition layer)
 */
typedef struct XrCompositionLayerWindowSpaceDXR {
    XrStructureType             type;       //!< Must be XR_TYPE_COMPOSITION_LAYER_WINDOW_SPACE_DXR
    const void* XR_MAY_ALIAS    next;       //!< Pointer to next structure in chain
    XrCompositionLayerFlags     layerFlags; //!< e.g. XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
    XrSwapchainSubImage         subImage;   //!< Source swapchain + rect
    float                       x;          //!< Left edge, fraction of window width  [0..1]
    float                       y;          //!< Top edge, fraction of window height   [0..1]
    float                       width;      //!< Fraction of window width  [0..1]
    float                       height;     //!< Fraction of window height [0..1]
    float                       disparity;  //!< Horizontal shift, fraction of window width.
                                            //!< 0 = screen depth, negative = toward viewer
} XrCompositionLayerWindowSpaceDXR;
#endif // XR_TYPE_COMPOSITION_LAYER_WINDOW_SPACE_DXR

/*
 * SPEC_VERSION 9 — texture target binding (ADR-060).
 *
 * A shared-texture session (sharedTextureHandle != NULL) names where its
 * texture is drawn: a REFERENCE WINDOW (the app's own top-level window, or
 * NULL for desktop coordinates) and a TARGET RECT in that window's client
 * area, in physical device pixels. The runtime weaves into the shared texture
 * at (0, 0, rect.w, rect.h) — the rect IS the canvas — phases the interlace
 * from the target's live screen position, and frames the views to it. It never
 * subclasses, reparents or binds a display processor to the reference window.
 * Honored only when the display processor phases without a window; otherwise
 * the session falls back to windowHandle as an anchor (SPEC_VERSION 8 path).
 */

typedef XrFlags64 XrWin32TextureTargetFrameFlagsDXR;
//! ADVISORY: the reference window is being moved or resized by the user right
//! now. Reserved in SPEC_VERSION 9; the runtime never does phase math from it.
static const XrWin32TextureTargetFrameFlagsDXR XR_WIN32_TEXTURE_TARGET_FRAME_DRAGGING_BIT_DXR = 0x00000001;

typedef XrFlags64 XrWin32TextureTargetStateFlagsDXR;
//! The weave was phased from the target's position on a windowless display processor.
static const XrWin32TextureTargetStateFlagsDXR XR_WIN32_TEXTURE_TARGET_STATE_WINDOWLESS_PHASE_BIT_DXR = 0x00000001;
//! The display processor cannot phase windowless; windowHandle carried the weave (SPEC_VERSION 8 path).
static const XrWin32TextureTargetStateFlagsDXR XR_WIN32_TEXTURE_TARGET_STATE_ANCHOR_FALLBACK_BIT_DXR = 0x00000002;
//! The target spanned screens and was woven per screen.
static const XrWin32TextureTargetStateFlagsDXR XR_WIN32_TEXTURE_TARGET_STATE_SEGMENTED_BIT_DXR = 0x00000004;
//! Some part of the target was written flat 2D (a 2D screen, or a screen that cannot weave it).
static const XrWin32TextureTargetStateFlagsDXR XR_WIN32_TEXTURE_TARGET_STATE_FLAT_SEGMENT_BIT_DXR = 0x00000008;
//! The target rect was larger than the shared texture and was clamped to it.
static const XrWin32TextureTargetStateFlagsDXR XR_WIN32_TEXTURE_TARGET_STATE_CLAMPED_BIT_DXR = 0x00000010;

/*!
 * @brief System capability, chained on XrSystemProperties::next.
 *
 * A PREDICTION made before any session exists; the session's
 * XrWin32TextureTargetStateDXR is the authority.
 *
 * @extends XrSystemProperties
 */
typedef struct XrSystemWin32TextureTargetPropertiesDXR {
    XrStructureType       type;                  //!< Must be XR_TYPE_SYSTEM_WIN32_TEXTURE_TARGET_PROPERTIES_DXR
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsTextureTarget; //!< The runtime implements the texture target structs
    XrBool32              windowlessPhase;       //!< The active display processor is expected to phase without a window
} XrSystemWin32TextureTargetPropertiesDXR;

/*!
 * @brief Session creation: names the texture target.
 *
 * Valid only with XrWin32WindowBindingCreateInfoDXR::sharedTextureHandle set
 * and readbackCallback NULL. windowHandle, if set, is the fallback anchor.
 *
 * @extends XrWin32WindowBindingCreateInfoDXR
 */
typedef struct XrWin32TextureTargetBindingDXR {
    XrStructureType           type;              //!< Must be XR_TYPE_WIN32_TEXTURE_TARGET_BINDING_DXR
    const void* XR_MAY_ALIAS  next;
    void*                     referenceWindow;   //!< HWND (top-level, this process) or NULL = desktop device px
    XrRect2Di                 initialTargetRect; //!< Used until the first frame carries a target
} XrWin32TextureTargetBindingDXR;

/*!
 * @brief Per-frame target. Chained on XrFrameEndInfo::next (authoritative)
 *        and optionally on XrViewLocateInfo::next (frames that locate's views).
 *
 * Omitted on a frame, the last value is reused.
 *
 * @extends XrFrameEndInfo
 * @extends XrViewLocateInfo
 */
typedef struct XrWin32TextureTargetFrameDXR {
    XrStructureType                   type;            //!< Must be XR_TYPE_WIN32_TEXTURE_TARGET_FRAME_DXR
    const void* XR_MAY_ALIAS          next;
    void*                             referenceWindow; //!< This frame's reference; NULL = desktop device px
    XrRect2Di                         targetRect;      //!< Device px in referenceWindow's client area (or the desktop)
    XrWin32TextureTargetFrameFlagsDXR flags;           //!< Advisory hints; 0 = none
} XrWin32TextureTargetFrameDXR;

/*!
 * @brief What the runtime used. Chained on XrFrameState::next at xrWaitFrame;
 *        describes the most recently WOVEN frame. Diagnostic, not a control loop.
 *
 * @extends XrFrameState
 */
typedef struct XrWin32TextureTargetStateDXR {
    XrStructureType                   type;              //!< Must be XR_TYPE_WIN32_TEXTURE_TARGET_STATE_DXR
    void* XR_MAY_ALIAS                next;
    XrWin32TextureTargetStateFlagsDXR flags;
    XrOffset2Di                       wovenScreenOrigin; //!< The target's resolved desktop origin, device px
    XrRect2Di                         wovenTargetRect;   //!< The rect that frame was woven for
    XrTime                            wovenDisplayTime;  //!< That frame's predictedDisplayTime
} XrWin32TextureTargetStateDXR;

#ifdef __cplusplus
}
#endif

#endif // XR_DXR_WIN32_WINDOW_BINDING_H
