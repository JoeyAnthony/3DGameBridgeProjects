// Copyright 2026, DisplayXR
// SPDX-License-Identifier: Apache-2.0
//
// PROVISIONAL — DXR is DisplayXR's Khronos-registered OpenXR author ID, but
// the XR_DXR_* extensions in this header are NOT yet registered in the
// Khronos OpenXR registry: extension numbers and XrStructureType values sit
// in a provisional experimental block (1004999xxx) pending official
// assignment. Extension names are expected to be stable; numeric values are
// not.
// See GOVERNANCE.md.
//
/*!
 * @file
 * @brief  Header for XR_DXR_swapchain_color_encoding extension
 * @ingroup external_openxr
 *
 * Declares that a UNORM colour swapchain holds DISPLAY-ENCODED (sRGB) values.
 *
 * OpenXR reads every non-sRGB colour format as linear (ADR-044): a UNORM
 * swapchain's values are sRGB-encoded on the way to the panel. That is right
 * for an app that renders linear light, and wrong for one that hands on
 * pixels which are already display-referred — a game's back buffer captured by
 * an injected layer, a decoded video frame, a desktop capture. With an 8-bit
 * format such an app picks the `_SRGB` sibling and copies its bytes raw, but
 * there is no `_SRGB` sibling for the deeper formats: a 10-bit back buffer
 * (R10G10B10A2) could only be converted down to 8 bits first.
 *
 * Chaining XrSwapchainColorEncodingCreateInfoDXR with
 * XR_SWAPCHAIN_COLOR_ENCODING_SRGB_DXR on xrCreateSwapchain declares the
 * swapchain's values display-encoded. The runtime then treats the swapchain
 * exactly as it treats an `_SRGB` one: on a frame that needs no blending the
 * values reach the display processor as they are, and where layers blend they
 * are decoded to linear first.
 *
 * Valid for colour swapchains with a UNORM format. On an `_SRGB` format the
 * declaration is redundant and accepted. XR_SWAPCHAIN_COLOR_ENCODING_LINEAR_DXR
 * is OpenXR's default reading. A runtime path that cannot honour the
 * declaration fails xrCreateSwapchain with XR_ERROR_FEATURE_UNSUPPORTED, so an
 * app can fall back (convert, or pick the `_SRGB` sibling) instead of getting
 * the wrong colours.
 */
#ifndef XR_DXR_SWAPCHAIN_COLOR_ENCODING_H
#define XR_DXR_SWAPCHAIN_COLOR_ENCODING_H 1

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XR_DXR_swapchain_color_encoding 1
#define XR_DXR_swapchain_color_encoding_SPEC_VERSION 1
#define XR_DXR_SWAPCHAIN_COLOR_ENCODING_EXTENSION_NAME "XR_DXR_swapchain_color_encoding"

#define XR_TYPE_SWAPCHAIN_COLOR_ENCODING_CREATE_INFO_DXR ((XrStructureType)1004999370)

/*!
 * @brief How the values of a colour swapchain are encoded.
 */
typedef enum XrSwapchainColorEncodingDXR {
    //! Linear values — OpenXR's reading of every non-sRGB format.
    XR_SWAPCHAIN_COLOR_ENCODING_LINEAR_DXR = 0,
    //! Display-encoded values (sRGB transfer function, IEC 61966-2-1), read like an `_SRGB` format.
    XR_SWAPCHAIN_COLOR_ENCODING_SRGB_DXR = 1,
    XR_SWAPCHAIN_COLOR_ENCODING_MAX_ENUM_DXR = 0x7FFFFFFF
} XrSwapchainColorEncodingDXR;

/*!
 * @brief Declares the colour encoding of a swapchain's values.
 *
 * @extends XrSwapchainCreateInfo
 */
typedef struct XrSwapchainColorEncodingCreateInfoDXR {
    XrStructureType             type;           //!< Must be XR_TYPE_SWAPCHAIN_COLOR_ENCODING_CREATE_INFO_DXR
    const void* XR_MAY_ALIAS    next;
    XrSwapchainColorEncodingDXR colorEncoding;  //!< How the swapchain's colour values are encoded
} XrSwapchainColorEncodingCreateInfoDXR;

#ifdef __cplusplus
}
#endif

#endif // XR_DXR_SWAPCHAIN_COLOR_ENCODING_H
