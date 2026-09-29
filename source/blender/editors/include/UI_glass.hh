/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * GlassMesh: shared settings of the translucent "glass" interface style.
 *
 * The glass style is purely visual. It is controlled by #UserDef.glass_flag
 * (Preferences > Interface > Display > Glass) and never changes any functionality.
 */

#pragma once

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

namespace blender::gpu {
class Batch;
}

namespace blender::ui {

/** True when the glass interface style is enabled in the preferences. */
inline bool glass_enabled()
{
  return (U.glass_flag & USER_GLASS_DISABLE) == 0;
}

/** True when the frosted backdrop blur should be drawn behind translucent regions. */
inline bool glass_blur_enabled()
{
  return glass_enabled() && (U.glass_flag & USER_GLASS_NO_BLUR) == 0;
}

/** Corner radius of editors (areas), in pixels. */
inline float glass_editor_radius()
{
  return (glass_enabled() ? 10.0f : 6.0f) * UI_SCALE_FAC;
}

/** Radius of the frosted backdrop blur, in pixels. */
inline float glass_blur_radius()
{
  return 22.0f * UI_SCALE_FAC;
}

/** Strength of the bright rim along the edges of glass panels, menus and buttons. */
inline float glass_rim_strength()
{
  return glass_enabled() ? 0.14f : 0.0f;
}

/** Strength of the soft sheen over the upper part of glass buttons. */
inline float glass_sheen_strength()
{
  return glass_enabled() ? 0.035f : 0.0f;
}

/**
 * Same as #draw_roundbox_4fv_ex, with the glass rim highlight and sheen (see #glass_rim_strength
 * and #glass_sheen_strength).
 */
void draw_roundbox_4fv_glass(const rctf *rect,
                             const float inner1[4],
                             const float inner2[4],
                             float shade_dir,
                             const float outline[4],
                             float outline_width,
                             float rad,
                             float glass_rim,
                             float glass_sheen);

/**
 * Bind the procedural glass "wallpaper" shader (#GPU_SHADER_2D_GLASS_WALLPAPER) on \a batch and
 * set its colors from the theme. In border mode the caller sets the same geometry uniforms as for
 * #GPU_SHADER_2D_AREA_BORDERS, otherwise `rect_geom` is the quad to fill (window pixels).
 */
void glass_wallpaper_shader_bind(gpu::Batch *batch, const int window_size[2], bool border_mode);

/** Fill the whole window with the glass wallpaper. */
void glass_wallpaper_draw(const int window_size[2]);

}  // namespace blender::ui
