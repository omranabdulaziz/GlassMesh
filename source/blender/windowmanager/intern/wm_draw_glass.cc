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
#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_screen.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "ED_screen.hh"

#include "UI_glass.hh"

#include "WM_api.hh"

#include "wm_draw.hh"

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
  if (screen->state != SCREENNORMAL ||
      (screen->areabase.is_single() && win->global_areas.areabase.first == nullptr))
  {
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
static gpu::Shader *glass_backdrop_shader_bind(const float rect_geom[4],
                                               const float backdrop_rect[4],
                                               const float mask_rect[4],
                                               const float tint[4],
                                               const float params[4],
                                               const float mask_threshold[2])
{
  gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_GLASS_BACKDROP);
  GPU_shader_bind(shader);
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
  gpu::Shader *shader = glass_backdrop_shader_bind(
      rect, full_rect, full_rect, no_tint, params, mask_all);
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

  gpu::Shader *shader = glass_backdrop_shader_bind(
      rect_geom, backdrop_rect, mask_rect, tint, params, GLASS_MASK_THRESHOLD);

  GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  glass_backdrop_draw_quad(shader, GPU_offscreen_color_texture(g_glass.reduced), mask);
  GPU_blend(GPU_BLEND_NONE);
}

}  // namespace blender
