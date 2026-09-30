/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * GlassMesh: window compositing for the translucent "glass" interface style.
 *
 * - The frosted (blurred) wallpaper is drawn behind all editors, like a desktop seen through a
 *   frosted window, and every editor is drawn on it as a pane of glass (see #glass_card_draw).
 *   Editors whose colors matter (3D viewport, image editor, node canvas) are opaque content.
 * - Translucent regions that overlap other content (headers, tool-bars and side-bars drawn over
 *   the 3D viewport, menus, popups and tool-tips) get a real frosted backdrop: the window
 *   frame-buffer is copied, pre-filtered at a quarter of the resolution and blurred, and drawn
 *   only where the region itself has (translucent) content.
 *
 * All of this only affects how already drawn regions are composited into the window.
 */

#include "BLI_listbase_iterator.hh"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_screen.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "ED_screen.hh"

#include "UI_glass.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

#include "wm_draw.hh"
#include "wm_window.hh"

namespace blender {

/** Resolution divider of the pre-filtered backdrop. */
static constexpr int GLASS_BACKDROP_REDUCE = 4;

/**
 * Region alpha range that is treated as a glass surface. Glass surfaces use a translucency of at
 * least ~0.5 in the default theme, while soft drop shadows (which should not blur what is behind
 * them) stay below that.
 */
static constexpr float GLASS_MASK_THRESHOLD[2] = {0.36f, 0.46f};

static struct {
  /** Full resolution copy of the window frame-buffer. */
  GPUOffScreen *full = nullptr;
  /** Pre-filtered, reduced resolution copy used for the blur. */
  GPUOffScreen *reduced = nullptr;
} g_glass;

/**
 * Windows show the GlassMesh window behind them: while a window has child windows (preferences,
 * file browser, render view...), a frosted copy of its last frame is kept, and its child windows
 * show it where they lie over it, with the wallpaper (the stand-in for the desktop) around it.
 */
struct GlassWindow {
  /** Frosted copy of the window's last frame, and a temporary buffer to make it. */
  GPUOffScreen *frame = nullptr;
  GPUOffScreen *frame_tmp = nullptr;
  /** What is behind the window: the frosted parent window and wallpaper. */
  GPUOffScreen *behind = nullptr;
};

/** Resolution divider of what is behind a window (it is blurred a lot). */
static constexpr int GLASS_BEHIND_REDUCE = 8;

static Map<const wmWindow *, GlassWindow> g_windows;

static void glass_window_free(GlassWindow &glass_win)
{
  for (GPUOffScreen **ofs : {&glass_win.frame, &glass_win.frame_tmp, &glass_win.behind}) {
    if (*ofs) {
      GPU_offscreen_free(*ofs);
      *ofs = nullptr;
    }
  }
}

static void glass_offscreen_ensure(GPUOffScreen **offscreen, const int2 &size)
{
  if (*offscreen && (GPU_offscreen_width(*offscreen) != size.x ||
                     GPU_offscreen_height(*offscreen) != size.y))
  {
    GPU_offscreen_free(*offscreen);
    *offscreen = nullptr;
  }
  if (*offscreen == nullptr) {
    *offscreen = GPU_offscreen_create(size.x,
                                      size.y,
                                      false,
                                      gpu::TextureFormat::UNORM_8_8_8_8,
                                      GPU_TEXTURE_USAGE_SHADER_READ,
                                      false,
                                      nullptr);
  }
}

void wm_draw_glass_exit()
{
  for (GlassWindow &glass_win : g_windows.values()) {
    glass_window_free(glass_win);
  }
  g_windows.clear();
  ui::glass_free_resources();
  if (g_glass.full) {
    GPU_offscreen_free(g_glass.full);
    g_glass.full = nullptr;
  }
  if (g_glass.reduced) {
    GPU_offscreen_free(g_glass.reduced);
    g_glass.reduced = nullptr;
  }
}

/** Where the window is on the desktop (native pixels, from the bottom left), if that is known. */
static bool glass_window_desktop_rect(const wmWindow *win, int2 &r_pos, int2 &r_desktop_size)
{
  if (!(WM_capabilities_flag() & WM_CAPABILITY_WINDOW_POSITION)) {
    return false;
  }
  int desktop_size[2];
  if (!wm_get_desktopsize(desktop_size) || desktop_size[0] <= 0 || desktop_size[1] <= 0) {
    return false;
  }
  const int2 native_size = WM_window_native_pixel_size(win);
  const float fac = float(native_size.x) / float(max_ii(win->sizex, 1));
  r_pos = int2(int(float(win->posx) * fac), int(float(win->posy) * fac));
  r_desktop_size = int2(int(float(desktop_size[0]) * fac), int(float(desktop_size[1]) * fac));
  return true;
}

static bool glass_window_has_children(const wmWindowManager *wm, const wmWindow *win)
{
  for (const wmWindow &other : wm->windows) {
    if (other.parent == win) {
      return true;
    }
  }
  return false;
}

static bool g_force_opaque = false;

void wm_draw_glass_force_opaque(const bool force_opaque)
{
  g_force_opaque = force_opaque;
}

/** The window is drawn see-through (with transparency), see #WM_window_is_see_through. */
static bool wm_draw_glass_see_through(const wmWindow *win)
{
  return !g_force_opaque && WM_window_is_see_through(win);
}

void wm_draw_glass_window_begin(const wmWindowManager *wm, const wmWindow *win)
{
  if (!ui::glass_enabled()) {
    return;
  }
  /* Forget closed windows (their pointers are only compared, never used). */
  g_windows.remove_if([&](const auto &item) {
    for (const wmWindow &other : wm->windows) {
      if (&other == item.key) {
        return false;
      }
    }
    glass_window_free(item.value);
    return true;
  });

  ui::GlassWindowBackdrop backdrop;
  const int2 win_size = WM_window_native_pixel_size(win);
  backdrop.window_size[0] = win_size.x;
  backdrop.window_size[1] = win_size.y;
  int2 win_pos, desktop_size;
  const bool has_position = glass_window_desktop_rect(win, win_pos, desktop_size);
  if (has_position) {
    backdrop.window_pos[0] = win_pos.x;
    backdrop.window_pos[1] = win_pos.y;
    backdrop.desktop_size[0] = desktop_size.x;
    backdrop.desktop_size[1] = desktop_size.y;
  }
  backdrop.see_through = wm_draw_glass_see_through(win);
  ui::glass_window_backdrop_set(&backdrop);

  /* A child window lying over its parent shows (the frosted copy of) the parent behind it. When
   * see-through, the system shows the parent (blurred) itself. */
  const GlassWindow *parent_glass = win->parent ? g_windows.lookup_ptr(win->parent) : nullptr;
  if (!has_position || !parent_glass || !parent_glass->frame || !ui::glass_blur_enabled() ||
      backdrop.see_through)
  {
    return;
  }
  int2 parent_pos, parent_desktop_size;
  if (!glass_window_desktop_rect(win->parent, parent_pos, parent_desktop_size)) {
    return;
  }
  const int2 parent_size = WM_window_native_pixel_size(win->parent);
  const float frame_rect[4] = {float(parent_pos.x - win_pos.x),
                               float(parent_pos.y - win_pos.y),
                               float(parent_pos.x - win_pos.x + parent_size.x),
                               float(parent_pos.y - win_pos.y + parent_size.y)};
  if (frame_rect[2] <= 0.0f || frame_rect[3] <= 0.0f || frame_rect[0] >= float(win_size.x) ||
      frame_rect[1] >= float(win_size.y))
  {
    return;
  }
  GlassWindow &glass_win = g_windows.lookup_or_add_default(win);
  const int2 behind_size = {max_ii(1, win_size.x / GLASS_BEHIND_REDUCE),
                            max_ii(1, win_size.y / GLASS_BEHIND_REDUCE)};
  glass_offscreen_ensure(&glass_win.behind, behind_size);
  if (glass_win.behind &&
      ui::glass_backdrop_compose(
          glass_win.behind, GPU_offscreen_color_texture(parent_glass->frame), frame_rect))
  {
    gpu::Texture *behind = GPU_offscreen_color_texture(glass_win.behind);
    GPU_texture_filter_mode(behind, true);
    GPU_texture_extend_mode(behind, GPU_SAMPLER_EXTEND_MODE_EXTEND);
    backdrop.behind = behind;
    ui::glass_window_backdrop_set(&backdrop);
  }
}

void wm_draw_glass_window_end(wmWindowManager *wm, const wmWindow *win)
{
  if (!ui::glass_enabled()) {
    return;
  }
  ui::glass_window_backdrop_set(nullptr);

  const bool keep_frame = ui::glass_blur_enabled() && glass_window_has_children(wm, win) &&
                          (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_POSITION) &&
                          !wm_draw_glass_see_through(win);
  if (!keep_frame) {
    if (GlassWindow *glass_win = g_windows.lookup_ptr(win)) {
      if (glass_win->frame) {
        GPU_offscreen_free(glass_win->frame);
        glass_win->frame = nullptr;
      }
    }
    return;
  }
  /* Keep a frosted copy of what was drawn, for the child windows, and let them show it. */
  if (!wm_draw_glass_capture(win)) {
    return;
  }
  GlassWindow &glass_win = g_windows.lookup_or_add_default(win);
  gpu::Texture *reduced = GPU_offscreen_color_texture(g_glass.reduced);
  GPU_texture_filter_mode(reduced, true);
  ui::glass_frost_copy(reduced, &glass_win.frame, &glass_win.frame_tmp);
  for (wmWindow &other : wm->windows) {
    if (other.parent == win) {
      if (bScreen *screen = WM_window_get_active_screen(&other)) {
        screen->do_draw = true;
      }
    }
  }
}

void wm_draw_glass_wallpaper(const wmWindow *win)
{
  if (!ui::glass_enabled()) {
    return;
  }
  const int2 win_size = WM_window_native_pixel_size(win);
  ui::glass_wallpaper_draw(win_size);
}

void wm_draw_glass_cards(const wmWindow *win)
{
  if (!ui::glass_enabled()) {
    return;
  }
  bScreen *screen = WM_window_get_active_screen(win);
  /* Same as the editor edges, see #ED_screen_draw_edges. */
  if (!ED_screen_glass_cards_visible(win, screen)) {
    return;
  }
  const int2 win_size = WM_window_native_pixel_size(win);
  /* All shadows first, so they don't fall on neighboring cards. */
  for (const ScrArea &area : screen->areabase) {
    if (!ui::glass_card_draw(ui::GlassCardPass::Shadow, &area.totrct, win_size, false)) {
      return;
    }
  }
  for (const ScrArea &area : screen->areabase) {
    ui::glass_card_draw(ui::GlassCardPass::Body, &area.totrct, win_size, false);
  }
}

void wm_draw_glass_region_keep_opaque(const wmWindow *win, const ARegion *region)
{
  if (!ui::glass_enabled() || !wm_draw_glass_see_through(win)) {
    return;
  }
  const rcti &rect = region->winrct;
  GPU_color_mask(false, false, false, true);
  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(0.0f, 0.0f, 0.0f, 1.0f);
  immRectf(pos, rect.xmin, rect.ymin, rect.xmax + 1, rect.ymax + 1);
  immUnbindProgram();
  GPU_color_mask(true, true, true, true);
}

bool wm_draw_glass_region_is_frosted_viewport(const ScrArea *area, const ARegion *region)
{
  if (!ui::glass_enabled()) {
    return false;
  }
  /* Regions drawn through a #GPUViewport whose background doesn't change how their content looks:
   * - The node editor and the sequencer timeline clear their background with the theme alpha.
   * - The image editor leaves the area around the image transparent (#BG_GLASS_CHECKER).
   * The 3D viewport (and the sequencer preview) stay opaque: a see-through background would
   * change the colors of what is being made. */
  return ELEM(area->spacetype, SPACE_IMAGE, SPACE_NODE, SPACE_SEQ) &&
         region->regiontype == RGN_TYPE_WINDOW && region->runtime->draw_buffer &&
         region->runtime->draw_buffer->viewport;
}

void wm_draw_glass_frosted_viewport_backdrop(const ScrArea *area, const ARegion *region)
{
  if (area->spacetype != SPACE_IMAGE) {
    return;
  }
  /* The image editor's (translucent) background color, around the image. */
  float color[4];
  ui::theme::get_color_back_glass_4fv(area->spacetype, color);
  if (color[3] <= 0.0f) {
    color[3] = 1.0f;
  }
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);
  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_ALPHA);
  immRectf(pos,
           region->winrct.xmin,
           region->winrct.ymin,
           region->winrct.xmax + 1,
           region->winrct.ymax + 1);
  GPU_blend(old_blend);
  immUnbindProgram();
}

bool wm_draw_glass_region_is_translucent(const ARegion *region)
{
  if (!ui::glass_enabled()) {
    return false;
  }
  /* Regions using a #GPUViewport (3D viewport, image editor, ...) are color managed and always
   * drawn opaque, everything else uses pre-multiplied alpha and can be blended. */
  return region->runtime->draw_buffer && region->runtime->draw_buffer->offscreen &&
         !region->runtime->draw_buffer->viewport;
}

/** Bind the glass backdrop shader and set everything except the textures. */
/** Modes of the backdrop shader, see #gpu_shader_2D_glass_backdrop. */
enum class GlassBackdropMode { Opaque = 0, SeeThrough = 1, CoverageMask = 2 };

static gpu::Shader *glass_backdrop_shader_bind(const float rect_geom[4],
                                               const float backdrop_rect[4],
                                               const float mask_rect[4],
                                               const float tint[4],
                                               const float params[4],
                                               const float mask_threshold[2],
                                               const GlassBackdropMode mode)
{
  gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_GLASS_BACKDROP);
  GPU_shader_bind(shader);
  GPU_shader_uniform_1i(shader, "backdrop_mode", int(mode));
  GPU_shader_uniform_4fv(shader, "rect_geom", rect_geom);
  GPU_shader_uniform_4fv(shader, "backdrop_rect", backdrop_rect);
  GPU_shader_uniform_4fv(shader, "mask_rect", mask_rect);
  GPU_shader_uniform_4fv(shader, "tint", tint);
  GPU_shader_uniform_4fv(shader, "params", params);
  GPU_shader_uniform_2fv(shader, "mask_threshold", mask_threshold);
  return shader;
}

static void glass_backdrop_draw_quad(gpu::Shader *shader,
                                     gpu::Texture *backdrop,
                                     gpu::Texture *mask)
{
  GPUSamplerState sampler = GPUSamplerState::default_sampler();
  sampler.filtering = GPU_SAMPLER_FILTERING_LINEAR;
  GPUSamplerState sampler_mask = GPUSamplerState::default_sampler();

  const int backdrop_binding = GPU_shader_get_sampler_binding(shader, "backdrop");
  const int mask_binding = GPU_shader_get_sampler_binding(shader, "mask");
  GPU_texture_bind_ex(backdrop, sampler, backdrop_binding);
  GPU_texture_bind_ex(mask, sampler_mask, mask_binding);

  gpu::Batch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, shader);
  GPU_batch_draw(quad);

  GPU_texture_unbind(backdrop);
  GPU_texture_unbind(mask);
}

bool wm_draw_glass_capture(const wmWindow *win)
{
  if (!ui::glass_blur_enabled()) {
    return false;
  }

  const int2 win_size = WM_window_native_pixel_size(win);
  if (win_size.x <= 0 || win_size.y <= 0) {
    return false;
  }
  const int2 reduced_size = {max_ii(1, win_size.x / GLASS_BACKDROP_REDUCE),
                             max_ii(1, win_size.y / GLASS_BACKDROP_REDUCE)};

  glass_offscreen_ensure(&g_glass.full, win_size);
  glass_offscreen_ensure(&g_glass.reduced, reduced_size);
  if (!g_glass.full || !g_glass.reduced) {
    return false;
  }

  /* 1. Copy what is drawn in the window so far. */
  gpu::FrameBuffer *window_fb = GPU_framebuffer_active_get();
  GPU_offscreen_bind(g_glass.full, true);
  gpu::FrameBuffer *full_fb = GPU_framebuffer_active_get();
  GPU_framebuffer_blit(window_fb, 0, full_fb, 0, GPU_COLOR_BIT);
  GPU_offscreen_unbind(g_glass.full, true);

  /* 2. Pre-filter into the reduced resolution copy, so the final blur is smooth with few taps. */
  GPU_offscreen_bind(g_glass.reduced, true);
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_ortho_2d_set(0.0f, float(win_size.x), 0.0f, float(win_size.y));
  GPU_matrix_identity_set();

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);

  const float rect[4] = {0.0f, 0.0f, float(win_size.x), float(win_size.y)};
  const float full_rect[4] = {0.0f, 0.0f, float(win_size.x), float(win_size.y)};
  const float no_tint[4] = {1.0f, 1.0f, 1.0f, 0.0f};
  /* Radius matching the reduction factor, no grain, neutral colors. */
  const float params[4] = {float(GLASS_BACKDROP_REDUCE) * 1.25f, 0.0f, 1.0f, 1.0f};
  /* Everything is covered: the mask threshold is below any possible alpha. */
  const float mask_all[2] = {-2.0f, -1.0f};
  gpu::Texture *full_tex = GPU_offscreen_color_texture(g_glass.full);
  /* A see-through window has transparency, keep it. */
  const GlassBackdropMode mode = wm_draw_glass_see_through(win) ? GlassBackdropMode::SeeThrough :
                                                                 GlassBackdropMode::Opaque;
  gpu::Shader *shader = glass_backdrop_shader_bind(
      rect, full_rect, full_rect, no_tint, params, mask_all, mode);
  glass_backdrop_draw_quad(shader, full_tex, full_tex);

  GPU_blend(old_blend);
  GPU_matrix_pop();
  GPU_matrix_pop_projection();
  GPU_offscreen_unbind(g_glass.reduced, true);

  return true;
}

bool wm_draw_glass_region_wants_blur(ARegion *region)
{
  if (!ui::glass_blur_enabled()) {
    return false;
  }
  if (!wm_draw_glass_region_is_translucent(region)) {
    return false;
  }
  if (BLI_rcti_is_empty(&region->winrct)) {
    return false;
  }
  /* Regions that are sliding in or out are drawn with an offset, don't bother. */
  if (ED_region_blend_alpha(region) < 1.0f) {
    return false;
  }
  return true;
}

void wm_draw_glass_backdrop(const wmWindow *win, ARegion *region)
{
  if (!g_glass.reduced) {
    return;
  }
  gpu::Texture *mask = wm_draw_region_texture(region, 0);
  if (mask == nullptr) {
    return;
  }

  const int2 win_size = WM_window_native_pixel_size(win);
  const rcti &winrct = region->winrct;

  const float rect_geom[4] = {
      float(winrct.xmin), float(winrct.ymin), float(winrct.xmax + 1), float(winrct.ymax + 1)};
  const float backdrop_rect[4] = {0.0f, 0.0f, float(win_size.x), float(win_size.y)};
  const float mask_rect[4] = {float(winrct.xmin),
                              float(winrct.ymin),
                              float(GPU_texture_width(mask)),
                              float(GPU_texture_height(mask))};
  /* A hint of milky white makes the glass read as "frosted" on top of dark content. */
  const float tint[4] = {1.0f, 1.0f, 1.0f, 0.07f};
  /* Blur radius, grain, saturation boost (vibrancy) and brightness. */
  const float params[4] = {ui::glass_blur_radius(), 0.018f, 1.6f, 1.03f};

  gpu::Texture *backdrop = GPU_offscreen_color_texture(g_glass.reduced);
  if (wm_draw_glass_see_through(win)) {
    /* The blurred backdrop has transparency (the desktop shows through): clear what it replaces
     * first, then add it. */
    gpu::Shader *shader = glass_backdrop_shader_bind(rect_geom,
                                                     backdrop_rect,
                                                     mask_rect,
                                                     tint,
                                                     params,
                                                     GLASS_MASK_THRESHOLD,
                                                     GlassBackdropMode::CoverageMask);
    GPU_blend(GPU_BLEND_MULTIPLY);
    glass_backdrop_draw_quad(shader, backdrop, mask);
    shader = glass_backdrop_shader_bind(rect_geom,
                                        backdrop_rect,
                                        mask_rect,
                                        tint,
                                        params,
                                        GLASS_MASK_THRESHOLD,
                                        GlassBackdropMode::SeeThrough);
    GPU_blend(GPU_BLEND_ADDITIVE_PREMULT);
    glass_backdrop_draw_quad(shader, backdrop, mask);
    GPU_blend(GPU_BLEND_NONE);
    return;
  }

  gpu::Shader *shader = glass_backdrop_shader_bind(rect_geom,
                                                   backdrop_rect,
                                                   mask_rect,
                                                   tint,
                                                   params,
                                                   GLASS_MASK_THRESHOLD,
                                                   GlassBackdropMode::Opaque);

  GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  glass_backdrop_draw_quad(shader, backdrop, mask);
  GPU_blend(GPU_BLEND_NONE);
}

}  // namespace blender
