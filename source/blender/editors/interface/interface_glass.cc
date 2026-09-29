/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * GlassMesh: helpers for the translucent "glass" interface style that are shared between the
 * screen (editor gaps) and window-manager (window compositing) drawing code.
 */

#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"

#include "UI_glass.hh"
#include "UI_resources.hh"

namespace blender::ui {

/**
 * The wallpaper palette is derived from the theme's "Editor Border" color (the color of the gaps
 * between editors), so changing that color in the theme also changes the wallpaper.
 */
static void glass_wallpaper_palette(float r_top[4],
                                    float r_bottom[4],
                                    float r_accent1[4],
                                    float r_accent2[4])
{
  float base[4];
  theme::get_color_4fv(TH_EDITOR_BORDER, base);

  float hsv[3];
  rgb_to_hsv_v(base, hsv);

  auto from_hsv = [](float h, float s, float v, float r_col[4]) {
    const float hsv_col[3] = {h - floorf(h), clamp_f(s, 0.0f, 1.0f), clamp_f(v, 0.0f, 1.0f)};
    hsv_to_rgb_v(hsv_col, r_col);
    r_col[3] = 1.0f;
  };

  /* Light "sky" at the top, deep "water" at the bottom. */
  from_hsv(hsv[0] - 0.01f, hsv[1] * 0.70f, hsv[2] * 1.45f + 0.12f, r_top);
  from_hsv(hsv[0] + 0.02f, hsv[1] * 1.05f, hsv[2] * 0.55f, r_bottom);
  /* Accents: a cyan/teal and a violet glow. */
  from_hsv(hsv[0] - 0.06f, hsv[1] * 1.10f, hsv[2] * 1.25f + 0.05f, r_accent1);
  from_hsv(hsv[0] + 0.09f, hsv[1] * 0.90f, hsv[2] * 1.10f, r_accent2);
}

void glass_wallpaper_shader_bind(gpu::Batch *batch, const int window_size[2], bool border_mode)
{
  float top[4], bottom[4], accent1[4], accent2[4];
  glass_wallpaper_palette(top, bottom, accent1, accent2);
  const float size[4] = {float(window_size[0]), float(window_size[1]), 0.0f, 0.0f};

  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_WALLPAPER);
  GPU_batch_uniform_4fv(batch, "window_size", size);
  GPU_batch_uniform_4fv(batch, "color_top", top);
  GPU_batch_uniform_4fv(batch, "color_bottom", bottom);
  GPU_batch_uniform_4fv(batch, "color_accent1", accent1);
  GPU_batch_uniform_4fv(batch, "color_accent2", accent2);
  GPU_batch_uniform_1b(batch, "border_mode", border_mode);
  if (!border_mode) {
    /* Unused in fill mode, but keep them defined. */
    GPU_batch_uniform_1f(batch, "scale", 1.0f);
    GPU_batch_uniform_1f(batch, "width", 0.0f);
    GPU_batch_uniform_1i(batch, "cornerLen", 1);
  }
}

void glass_wallpaper_draw(const int window_size[2])
{
  gpu::Batch *batch = GPU_batch_preset_quad();
  glass_wallpaper_shader_bind(batch, window_size, false);
  const float rect[4] = {0.0f, 0.0f, float(window_size[0]), float(window_size[1])};
  GPU_batch_uniform_4fv(batch, "rect_geom", rect);

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);
  GPU_batch_draw(batch);
  GPU_blend(old_blend);
}

}  // namespace blender::ui
