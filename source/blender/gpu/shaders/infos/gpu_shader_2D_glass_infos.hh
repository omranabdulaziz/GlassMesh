/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GlassMesh: shaders used to give the interface its translucent "glass" look.
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"

#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(glass_win_co_iface)
SMOOTH(float2, win_co)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_INTERFACE_INFO(glass_wallpaper_iface)
SMOOTH(float2, win_co)
SMOOTH(float2, uv_border)
GPU_SHADER_INTERFACE_END()

/**
 * Frosted glass backdrop.
 *
 * Draws a blurred, slightly tinted and grained copy of `backdrop` (usually a copy of the window
 * frame-buffer), only where the `mask` texture (the off-screen texture of the translucent region
 * that is about to be blended on top) has coverage. Output is pre-multiplied.
 */
GPU_SHADER_CREATE_INFO(gpu_shader_2D_glass_backdrop)
VERTEX_IN(0, float2, pos)
VERTEX_OUT(glass_win_co_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
/* Quad to draw in window pixels: (xmin, ymin, xmax, ymax). */
PUSH_CONSTANT(float4, rect_geom)
/* Placement of the backdrop texture in window pixels: (xmin, ymin, width, height). */
PUSH_CONSTANT(float4, backdrop_rect)
/* Placement of the mask texture in window pixels: (xmin, ymin, width, height). */
PUSH_CONSTANT(float4, mask_rect)
/* Straight RGB tint, alpha is the tint amount. */
PUSH_CONSTANT(float4, tint)
/* x: blur radius (pixels), y: grain amount, z: saturation, w: brightness. */
PUSH_CONSTANT(float4, params)
/* Mask alpha range (smooth-step) that is considered to be glass. */
PUSH_CONSTANT(float2, mask_threshold)
SAMPLER(0, sampler2D, backdrop)
SAMPLER(1, sampler2D, mask)
VERTEX_SOURCE("gpu_shader_2D_glass_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_glass_backdrop_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/**
 * Wallpaper: the image the glass interface is drawn over, like a desktop wallpaper behind a
 * frosted window. Used for the frosted window background, the gaps and rounded corners between
 * editors, and the (transparent) 3D viewport background. Also used to pre-blur the wallpaper.
 *
 * When `border_mode` is set, the geometry and coverage of #gpu_shader_2D_area_borders is used
 * so only the gaps and rounded corners between editors are filled.
 */
GPU_SHADER_CREATE_INFO(gpu_shader_2D_glass_wallpaper)
VERTEX_IN(0, float2, pos)
VERTEX_OUT(glass_wallpaper_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
/* Fill mode: quad in window pixels (xmin, ymin, xmax, ymax).
 * Border mode: area rectangle as `rctf` (xmin, xmax, ymin, ymax). */
PUSH_CONSTANT(float4, rect_geom)
/* Maps window pixels to image coordinates: `uv = win_co * xy + zw`. */
PUSH_CONSTANT(float4, uv_transform)
/* Straight RGB color mixed over the image, alpha is the amount. */
PUSH_CONSTANT(float4, tint)
/* x: mip-map level, y: saturation, z: brightness, w: grain amount. */
PUSH_CONSTANT(float4, params)
/* xy: image coordinate step between the taps of a 1D Gaussian blur (zero for no blur),
 * z: write the raw color (for off-screen passes) instead of frame-buffer space. */
PUSH_CONSTANT(float4, blur)
/* Border mode parameters, same meaning as in #gpu_shader_2D_area_borders. */
PUSH_CONSTANT(float, scale)
PUSH_CONSTANT(float, width)
PUSH_CONSTANT(int, cornerLen)
PUSH_CONSTANT(bool, border_mode)
SAMPLER(0, sampler2D, image)
VERTEX_SOURCE("gpu_shader_2D_glass_wallpaper_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_glass_wallpaper_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
