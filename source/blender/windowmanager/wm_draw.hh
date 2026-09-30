/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

namespace blender {

struct ARegion;
struct GPUOffScreen;
namespace gpu {
class Texture;
}
struct GPUViewport;
struct ScrArea;
struct bContext;
struct wmWindow;

struct wmDrawBuffer {
  GPUOffScreen *offscreen;
  GPUViewport *viewport;
  bool stereo;
  int bound_view;
};

/* `wm_draw.cc` */

void wm_draw_update(bContext *C);
void wm_draw_region_clear(wmWindow *win, ARegion *region);
void wm_draw_region_blend(ARegion *region, int view, bool blend);
void wm_draw_region_test(bContext *C, ScrArea *area, ARegion *region);

gpu::Texture *wm_draw_region_texture(ARegion *region, int view);

/* `wm_draw_glass.cc` (GlassMesh) */

/** Draw the glass wallpaper behind everything (no-op when the glass style is disabled). */
void wm_draw_glass_wallpaper(const wmWindow *win);
/**
 * Before and after drawing a window: set what is behind it (the wallpaper, or the GlassMesh window
 * behind a child window), and keep a frosted copy of windows that have child windows.
 */
void wm_draw_glass_window_begin(const wmWindowManager *wm, const wmWindow *win);
void wm_draw_glass_window_end(wmWindowManager *wm, const wmWindow *win);
/**
 * Draw the editors as panes of glass over the wallpaper (their shadow and body), before the
 * editors themselves. Their edges are drawn afterwards (#ED_screen_draw_edges).
 */
void wm_draw_glass_cards(const wmWindow *win);
/**
 * An opaque editor (like the 3D viewport) in a see-through window stays opaque: nothing of the
 * desktop shows through its colors.
 */
void wm_draw_glass_region_keep_opaque(const wmWindow *win, const ARegion *region);
/** A non-viewport region that can be blended (translucent) with the glass style. */
bool wm_draw_glass_region_is_translucent(const ARegion *region);
/**
 * A region drawn through a #GPUViewport (node editor, image editor, sequencer timeline) that is
 * blended over the glass as frosted glass, with pre-multiplied alpha.
 */
bool wm_draw_glass_region_is_frosted_viewport(const ScrArea *area, const ARegion *region);
/** Draw what is behind a region for which #wm_draw_glass_region_is_frosted_viewport is true. */
void wm_draw_glass_frosted_viewport_backdrop(const ScrArea *area, const ARegion *region);
/** A region that should get a frosted (blurred) backdrop. */
bool wm_draw_glass_region_wants_blur(ARegion *region);
/**
 * Copy the currently bound (window) frame-buffer into the glass backdrop textures.
 * \return false when the backdrop is not available (blur disabled or allocation failure).
 */
bool wm_draw_glass_capture(const wmWindow *win);
/** Draw the frosted backdrop for \a region, using the last capture. */
void wm_draw_glass_backdrop(const wmWindow *win, ARegion *region);
/** Free GPU resources, the GPU context must be active. */
void wm_draw_glass_exit();

}  // namespace blender
