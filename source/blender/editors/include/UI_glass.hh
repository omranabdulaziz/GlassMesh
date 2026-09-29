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
  return (glass_enabled() ? 16.0f : 6.0f) * UI_SCALE_FAC;
}

/** Radius of the frosted backdrop blur, in pixels. */
inline float glass_blur_radius()
{
  return 22.0f * UI_SCALE_FAC;
}

/** Strength of the bright rim along the edges of glass panels, menus and buttons. */
inline float glass_rim_strength()
{
  return glass_enabled() ? 0.3f : 0.0f;
}

/** Strength of the soft sheen over the upper part of glass buttons. */
inline float glass_sheen_strength()
{
  return glass_enabled() ? 0.05f : 0.0f;
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

/** Fill the whole window with the frosted glass wallpaper. */
void glass_wallpaper_draw(const int window_size[2]);

/**
 * The passes that draw an editor as a pane of glass ("card") over the frosted window background.
 * Editors are the content of the cards: opaque editors (3D viewport, image editor...) cover the
 * body, translucent ones let it show through.
 */
enum class GlassCardPass {
  /** The soft drop shadow on the window background, before all bodies. */
  Shadow,
  /** The glass itself, before the editor is drawn. */
  Body,
  /** The rounded corners and the specular rim along the edge, after the editor is drawn. */
  Edge,
};

/**
 * Draw one pass of the glass card \a card (window pixels, an area's `totrct`). \a active is the
 * active editor, which gets a brighter rim. Returns false (and draws nothing) if the wallpaper
 * isn't available.
 */
bool glass_card_draw(GlassCardPass pass, const rcti *card, const int window_size[2], bool active);

/**
 * The approximate color of the frosted window background along the top of the window (sRGB),
 * for the operating system's title bar. Returns false when the glass style is disabled.
 */
bool glass_window_top_color(float r_color[3]);

/** Free the GPU resources of the glass style (needs the GPU context). */
void glass_free_resources();

}  // namespace blender::ui
