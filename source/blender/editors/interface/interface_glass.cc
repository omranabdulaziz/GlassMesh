/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * GlassMesh: the wallpaper behind the "glass" interface style and the glass drawn over it, shared
 * between the screen (editor edges) and window-manager (window compositing) drawing code.
 *
 * The wallpaper is an image (a built-in one, or any image chosen in the preferences) that the
 * interface is drawn over, like a desktop wallpaper behind a frosted window:
 * - The window background and the gaps between editors show a heavily blurred ("frosted") copy.
 * - Every editor is a pane of glass ("card") lying on it: a drop shadow, a body that makes what is
 *   behind it more vivid and bends it near the edge, and a specular rim along the edge.
 * Content (3D viewport, image editor, node canvas) is opaque, the wallpaper never shows through.
 */

#include <cstring>

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "CLG_log.h"

#include "DNA_space_types.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "ED_datafiles.h"

#include "UI_glass.hh"
#include "UI_resources.hh"

namespace blender::ui {

static CLG_LogRef LOG = {"ui.glass"};

/** Width of the frosted copy of the wallpaper (it is blurred a lot, so it can be small). */
static constexpr int GLASS_FROSTED_WIDTH = 320;
/** Larger wallpapers are scaled down, the interface never shows more detail than this. */
static constexpr int GLASS_WALLPAPER_MAX_SIZE = 4096;

static struct {
  /** Full resolution wallpaper, with mip-maps. */
  gpu::Texture *image = nullptr;
  /** Heavily blurred copy of the wallpaper and a temporary buffer to create it. */
  GPUOffScreen *frosted = nullptr;
  GPUOffScreen *frosted_tmp = nullptr;
  /** Width / height of the wallpaper. */
  float aspect = 1.6f;
  /** #UserDef.glass_wallpaper the textures were created for. */
  char source[FILE_MAX] = "";
  bool loaded = false;
} g_wallpaper;

/* -------------------------------------------------------------------- */
/** \name Loading
 * \{ */

/** A plain blue gradient, only used when no wallpaper image can be loaded at all. */
static ImBuf *glass_wallpaper_fallback()
{
  const int w = 64, h = 40;
  ImBuf *ibuf = IMB_allocImBuf(w, h, ImBufFlags::ByteData);
  if (ibuf == nullptr) {
    return nullptr;
  }
  uchar *px = ibuf->byte_data_for_write();
  for (int y = 0; y < h; y++) {
    const float t = float(y) / float(h - 1);
    for (int x = 0; x < w; x++, px += 4) {
      px[0] = uchar(40 + 90 * t);
      px[1] = uchar(90 + 100 * t);
      px[2] = uchar(140 + 100 * t);
      px[3] = 255;
    }
  }
  return ibuf;
}

static ImBuf *glass_wallpaper_load_imbuf()
{
  ImBuf *ibuf = nullptr;
  if (U.glass_wallpaper[0] != '\0') {
    ibuf = IMB_load_image_from_filepath(U.glass_wallpaper, ImBufFlags::ByteData);
    if (ibuf == nullptr) {
      CLOG_WARN(&LOG, "Could not load the glass wallpaper \"%s\"", U.glass_wallpaper);
    }
  }
  if (ibuf == nullptr) {
    ibuf = IMB_load_image_from_memory(reinterpret_cast<const uchar *>(datatoc_wallpaper_jpg),
                                      size_t(datatoc_wallpaper_jpg_size),
                                      ImBufFlags::ByteData,
                                      "<glass wallpaper>");
  }
  if (ibuf && ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data) {
    IMB_byte_from_float(ibuf);
  }
  if (ibuf && ibuf->byte_buffer.data == nullptr) {
    IMB_freeImBuf(ibuf);
    ibuf = nullptr;
  }
  if (ibuf == nullptr) {
    ibuf = glass_wallpaper_fallback();
  }
  if (ibuf && (ibuf->x > GLASS_WALLPAPER_MAX_SIZE || ibuf->y > GLASS_WALLPAPER_MAX_SIZE)) {
    const float fac = float(GLASS_WALLPAPER_MAX_SIZE) / float(max_ii(ibuf->x, ibuf->y));
    IMB_scale(ibuf,
              uint(max_ii(1, int(ibuf->x * fac))),
              uint(max_ii(1, int(ibuf->y * fac))),
              IMBScaleFilter::Box,
              false);
  }
  return ibuf;
}

static void glass_wallpaper_free_textures()
{
  if (g_wallpaper.image) {
    GPU_texture_free(g_wallpaper.image);
    g_wallpaper.image = nullptr;
  }
  if (g_wallpaper.frosted) {
    GPU_offscreen_free(g_wallpaper.frosted);
    g_wallpaper.frosted = nullptr;
  }
  if (g_wallpaper.frosted_tmp) {
    GPU_offscreen_free(g_wallpaper.frosted_tmp);
    g_wallpaper.frosted_tmp = nullptr;
  }
  g_wallpaper.loaded = false;
}

/** Draw \a src into the whole of \a dst with the wallpaper shader (off-screen pass). */
static void glass_wallpaper_pass(GPUOffScreen *dst, gpu::Texture *src, float lod, float2 step)
{
  GPU_offscreen_bind(dst, true);
  GPU_matrix_push_projection();
  GPU_matrix_push();
  /* Identity matrices: the quad is given in normalized device coordinates. */
  GPU_matrix_identity_projection_set();
  GPU_matrix_identity_set();

  gpu::Batch *batch = GPU_batch_preset_quad();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_WALLPAPER);
  const float rect[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
  const float uv[4] = {0.5f, 0.5f, 0.5f, 0.5f};
  const float tint[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float params[4] = {lod, 1.0f, 1.0f, 0.0f};
  const float blur[4] = {step.x, step.y, 1.0f, 0.0f};
  GPU_batch_uniform_4fv(batch, "rect_geom", rect);
  GPU_batch_uniform_4fv(batch, "uv_transform", uv);
  GPU_batch_uniform_4fv(batch, "tint", tint);
  GPU_batch_uniform_4fv(batch, "params", params);
  GPU_batch_uniform_4fv(batch, "blur", blur);
  GPU_batch_texture_bind(batch, "image", src);
  GPU_batch_draw(batch);
  GPU_texture_unbind(src);

  GPU_matrix_pop();
  GPU_matrix_pop_projection();
  GPU_offscreen_unbind(dst, true);
}

static GPUOffScreen *glass_frosted_offscreen_create(int w, int h)
{
  GPUOffScreen *ofs = GPU_offscreen_create(w,
                                           h,
                                           false,
                                           gpu::TextureFormat::SFLOAT_16_16_16_16,
                                           GPU_TEXTURE_USAGE_SHADER_READ,
                                           false,
                                           nullptr);
  if (ofs) {
    gpu::Texture *tex = GPU_offscreen_color_texture(ofs);
    GPU_texture_filter_mode(tex, true);
    GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  }
  return ofs;
}

/** Make sure the wallpaper textures exist and match the preferences. Needs a GPU context. */
static bool glass_wallpaper_ensure()
{
  if (g_wallpaper.loaded && STREQ(g_wallpaper.source, U.glass_wallpaper)) {
    return g_wallpaper.image != nullptr && g_wallpaper.frosted != nullptr;
  }
  glass_wallpaper_free_textures();
  STRNCPY(g_wallpaper.source, U.glass_wallpaper);
  g_wallpaper.loaded = true;

  ImBuf *ibuf = glass_wallpaper_load_imbuf();
  if (ibuf == nullptr) {
    return false;
  }

  g_wallpaper.aspect = float(ibuf->x) / float(max_ii(ibuf->y, 1));
  g_wallpaper.image = GPU_texture_create_2d("glass_wallpaper",
                                            ibuf->x,
                                            ibuf->y,
                                            9999,
                                            gpu::TextureFormat::UNORM_8_8_8_8,
                                            GPU_TEXTURE_USAGE_SHADER_READ |
                                                GPU_TEXTURE_USAGE_SHADER_WRITE,
                                            nullptr);
  if (g_wallpaper.image) {
    GPU_texture_update(g_wallpaper.image, GPU_DATA_UBYTE, ibuf->byte_buffer.data);
    GPU_texture_filter_mode(g_wallpaper.image, true);
    GPU_texture_update_mipmap_chain(g_wallpaper.image);
    GPU_texture_mipmap_mode(g_wallpaper.image, true, true);
    GPU_texture_extend_mode(g_wallpaper.image, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  }
  const int image_width = ibuf->x;
  IMB_freeImBuf(ibuf);
  if (g_wallpaper.image == nullptr) {
    return false;
  }

  /* Frosted copy: down-sample, then blur with a few separable Gaussian passes. */
  const int fw = min_ii(GLASS_FROSTED_WIDTH, image_width);
  const int fh = max_ii(1, int(float(fw) / g_wallpaper.aspect + 0.5f));
  g_wallpaper.frosted = glass_frosted_offscreen_create(fw, fh);
  g_wallpaper.frosted_tmp = glass_frosted_offscreen_create(fw, fh);
  if (g_wallpaper.frosted == nullptr || g_wallpaper.frosted_tmp == nullptr) {
    glass_wallpaper_free_textures();
    g_wallpaper.loaded = true;
    return false;
  }

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);

  const float lod = log2f(max_ff(float(image_width) / float(fw), 1.0f));
  glass_wallpaper_pass(g_wallpaper.frosted, g_wallpaper.image, lod, float2(0.0f));
  const float spacing = 2.0f;
  for (int i = 0; i < 3; i++) {
    glass_wallpaper_pass(g_wallpaper.frosted_tmp,
                         GPU_offscreen_color_texture(g_wallpaper.frosted),
                         0.0f,
                         float2(spacing / float(fw), 0.0f));
    glass_wallpaper_pass(g_wallpaper.frosted,
                         GPU_offscreen_color_texture(g_wallpaper.frosted_tmp),
                         0.0f,
                         float2(0.0f, spacing / float(fh)));
  }

  GPU_blend(old_blend);
  return true;
}

void glass_free_resources()
{
  glass_wallpaper_free_textures();
  g_wallpaper.source[0] = '\0';
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

/** Map window pixels to wallpaper coordinates, so the wallpaper covers the whole window. */
static void glass_wallpaper_uv_transform(const int window_size[2], float r_uv[4])
{
  const float w = float(max_ii(window_size[0], 1));
  const float h = float(max_ii(window_size[1], 1));
  float dw = w;
  float dh = w / g_wallpaper.aspect;
  if (dh < h) {
    dh = h;
    dw = h * g_wallpaper.aspect;
  }
  r_uv[0] = 1.0f / dw;
  r_uv[1] = 1.0f / dh;
  r_uv[2] = -((w - dw) * 0.5f) / dw;
  r_uv[3] = -((h - dh) * 0.5f) / dh;
}

/**
 * The frosted window background: the blurred wallpaper, tinted with the theme's "Editor Border"
 * color (its alpha is the amount), so the look of the gaps between editors can be changed there.
 */
static void glass_frosted_look(float r_tint[4], float r_params[4])
{
  theme::get_color_4fv(TH_EDITOR_BORDER, r_tint);
  r_params[0] = 0.0f;
  /* Vivid, like the mockups' window glass: frosted glass saturates what is behind it. */
  r_params[1] = 1.6f;
  r_params[2] = 1.08f;
  r_params[3] = 1.6f / 255.0f;
}

void glass_wallpaper_draw(const int window_size[2])
{
  if (!glass_wallpaper_ensure()) {
    return;
  }
  float tint[4], params[4], uv[4];
  glass_frosted_look(tint, params);
  glass_wallpaper_uv_transform(window_size, uv);
  const float blur[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float rect[4] = {0.0f, 0.0f, float(window_size[0]), float(window_size[1])};

  gpu::Batch *batch = GPU_batch_preset_quad();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_WALLPAPER);
  GPU_batch_uniform_4fv(batch, "rect_geom", rect);
  GPU_batch_uniform_4fv(batch, "uv_transform", uv);
  GPU_batch_uniform_4fv(batch, "tint", tint);
  GPU_batch_uniform_4fv(batch, "params", params);
  GPU_batch_uniform_4fv(batch, "blur", blur);
  GPU_batch_texture_bind(batch, "image", GPU_offscreen_color_texture(g_wallpaper.frosted));

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);
  GPU_batch_draw(batch);
  GPU_blend(old_blend);
}

/**
 * The glass of the editor cards: the frosted wallpaper behind them, more vivid and a little
 * lighter than the window background around them. Editors tint it with their (mostly clear) theme
 * background color.
 */
static void glass_card_look(float r_tint[4], float r_look[4])
{
  /* A milky light blue, it keeps cards light over dark parts of the wallpaper too. */
  const float milk[4] = {0.60f, 0.76f, 0.92f, 0.16f};
  copy_v4_v4(r_tint, milk);
  r_look[0] = 1.3f;
  r_look[1] = 1.04f;
  r_look[2] = 0.1f;
  r_look[3] = 1.6f / 255.0f;
}

bool glass_card_draw(const GlassCardPass pass,
                     const rcti *card,
                     const int window_size[2],
                     const bool active)
{
  if (!glass_wallpaper_ensure()) {
    return false;
  }
  const float scale = UI_SCALE_FAC;
  const float card_rect[4] = {
      float(card->xmin), float(card->ymin), float(card->xmax + 1), float(card->ymax + 1)};
  const float card_shape[4] = {glass_editor_radius(), 14.0f * scale, 3.0f * scale, 0.26f};

  float rect_geom[4];
  copy_v4_v4(rect_geom, card_rect);
  if (pass == GlassCardPass::Shadow) {
    const float pad = card_shape[1] + card_shape[2];
    rect_geom[0] -= pad;
    rect_geom[1] -= pad;
    rect_geom[2] += pad;
    rect_geom[3] += pad;
  }

  float uv[4];
  glass_wallpaper_uv_transform(window_size, uv);
  float frame_tint[4], frame_look[4], card_tint[4], card_look[4];
  glass_frosted_look(frame_tint, frame_look);
  /* The frosted look's first parameter is a mip-map level, the card shader takes saturation,
   * brightness, frost and grain. */
  const float frame_params[4] = {frame_look[1], frame_look[2], 0.0f, frame_look[3]};
  glass_card_look(card_tint, card_look);
  const float optics[4] = {14.0f * scale, 9.0f * scale, 5.0f * scale, 0.09f};
  const float rim[4] = {1.25f * scale, active ? 0.75f : 0.5f, 0.55f, 2.0f};

  gpu::Batch *batch = GPU_batch_preset_quad();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_CARD);
  GPU_batch_uniform_4fv(batch, "rect_geom", rect_geom);
  GPU_batch_uniform_4fv(batch, "card_rect", card_rect);
  GPU_batch_uniform_4fv(batch, "card_shape", card_shape);
  GPU_batch_uniform_4fv(batch, "uv_transform", uv);
  GPU_batch_uniform_4fv(batch, "frame_tint", frame_tint);
  GPU_batch_uniform_4fv(batch, "frame_look", frame_params);
  GPU_batch_uniform_4fv(batch, "card_tint", card_tint);
  GPU_batch_uniform_4fv(batch, "card_look", card_look);
  GPU_batch_uniform_4fv(batch, "optics", optics);
  GPU_batch_uniform_4fv(batch, "rim", rim);
  GPU_batch_uniform_1i(batch, "card_mode", int(pass));
  GPU_batch_texture_bind(batch, "frosted", GPU_offscreen_color_texture(g_wallpaper.frosted));
  GPU_batch_texture_bind(batch, "image", g_wallpaper.image);

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  GPU_batch_draw(batch);
  GPU_blend(old_blend);
  return true;
}

bool glass_window_top_color(float r_color[3])
{
  if (!glass_enabled()) {
    return false;
  }
  /* Average color of the top rows of the wallpaper, only computed again when it changes. */
  static struct {
    char source[FILE_MAX] = "";
    float color[3] = {0.0f, 0.0f, 0.0f};
    bool valid = false;
    bool computed = false;
  } top;
  if (!top.computed || !STREQ(top.source, U.glass_wallpaper)) {
    STRNCPY(top.source, U.glass_wallpaper);
    top.computed = true;
    top.valid = false;
    if (ImBuf *ibuf = glass_wallpaper_load_imbuf()) {
      const int rows = max_ii(1, ibuf->y / 12);
      double sum[3] = {0.0, 0.0, 0.0};
      for (int y = ibuf->y - rows; y < ibuf->y; y++) {
        const uchar *px = ibuf->byte_buffer.data + size_t(y) * size_t(ibuf->x) * 4;
        for (int x = 0; x < ibuf->x; x++, px += 4) {
          sum[0] += px[0];
          sum[1] += px[1];
          sum[2] += px[2];
        }
      }
      const double count = double(rows) * double(ibuf->x) * 255.0;
      for (int i = 0; i < 3; i++) {
        top.color[i] = float(sum[i] / count);
      }
      top.valid = true;
      IMB_freeImBuf(ibuf);
    }
  }
  if (!top.valid) {
    return false;
  }
  /* Same adjustments as the frosted window background (see the wallpaper shader). */
  float tint[4], params[4];
  glass_frosted_look(tint, params);
  const float luminance = 0.2126f * top.color[0] + 0.7152f * top.color[1] +
                          0.0722f * top.color[2];
  for (int i = 0; i < 3; i++) {
    const float c = (luminance + (top.color[i] - luminance) * params[1]) * params[2];
    r_color[i] = clamp_f(c + (tint[i] - c) * tint[3], 0.0f, 1.0f);
  }
  return true;
}

/** \} */

}  // namespace blender::ui
