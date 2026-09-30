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
#include "GPU_immediate.hh"
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

/**
 * Draw \a src unchanged (raw colors) into \a rect_ndc of the bound off-screen buffer, with
 * `uv = ndc * uv_ndc.xy + uv_ndc.zw`, optionally blurred along \a step. The matrices must be
 * identity (see #glass_offscreen_begin).
 */
static void glass_raw_quad(gpu::Texture *src,
                           const float rect_ndc[4],
                           const float uv_ndc[4],
                           float lod,
                           float2 step)
{
  gpu::Batch *batch = GPU_batch_preset_quad();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_WALLPAPER);
  const float tint[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float params[4] = {lod, 1.0f, 1.0f, 0.0f};
  const float blur[4] = {step.x, step.y, 1.0f, 0.0f};
  GPU_batch_uniform_4fv(batch, "rect_geom", rect_ndc);
  GPU_batch_uniform_4fv(batch, "uv_transform", uv_ndc);
  GPU_batch_uniform_4fv(batch, "tint", tint);
  GPU_batch_uniform_4fv(batch, "params", params);
  GPU_batch_uniform_4fv(batch, "blur", blur);
  GPU_batch_texture_bind(batch, "image", src);
  GPU_batch_draw(batch);
  GPU_texture_unbind(src);
}

/** Bind \a dst for drawing in normalized device coordinates (identity matrices). */
static void glass_offscreen_begin(GPUOffScreen *dst)
{
  GPU_offscreen_bind(dst, true);
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_identity_projection_set();
  GPU_matrix_identity_set();
}

static void glass_offscreen_end(GPUOffScreen *dst)
{
  GPU_matrix_pop();
  GPU_matrix_pop_projection();
  GPU_offscreen_unbind(dst, true);
}

/** Draw \a src into the whole of \a dst with the wallpaper shader (off-screen pass). */
static void glass_wallpaper_pass(GPUOffScreen *dst, gpu::Texture *src, float lod, float2 step)
{
  glass_offscreen_begin(dst);
  const float rect[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
  const float uv[4] = {0.5f, 0.5f, 0.5f, 0.5f};
  glass_raw_quad(src, rect, uv, lod, step);
  glass_offscreen_end(dst);
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

/** What is behind the window being drawn, see #glass_window_backdrop_set. */
static GlassWindowBackdrop g_backdrop;
static bool g_backdrop_is_set = false;

void glass_window_backdrop_set(const GlassWindowBackdrop *backdrop)
{
  g_backdrop_is_set = backdrop != nullptr;
  g_backdrop = backdrop ? *backdrop : GlassWindowBackdrop{};
}

/**
 * Map window pixels to wallpaper coordinates. The wallpaper covers the desktop when the window's
 * place on it is known, so it lies still behind windows that move, like a real desktop, and
 * several windows show the same wallpaper. Otherwise it covers the window.
 */
static void glass_wallpaper_uv_transform(const int window_size[2], float r_uv[4])
{
  const bool use_desktop = g_backdrop_is_set && g_backdrop.desktop_size[0] > 0 &&
                           g_backdrop.desktop_size[1] > 0;
  const float w = float(max_ii(use_desktop ? g_backdrop.desktop_size[0] : window_size[0], 1));
  const float h = float(max_ii(use_desktop ? g_backdrop.desktop_size[1] : window_size[1], 1));
  const float ofs_x = use_desktop ? float(g_backdrop.window_pos[0]) : 0.0f;
  const float ofs_y = use_desktop ? float(g_backdrop.window_pos[1]) : 0.0f;
  float dw = w;
  float dh = w / g_wallpaper.aspect;
  if (dh < h) {
    dh = h;
    dw = h * g_wallpaper.aspect;
  }
  r_uv[0] = 1.0f / dw;
  r_uv[1] = 1.0f / dh;
  r_uv[2] = (ofs_x - (w - dw) * 0.5f) / dw;
  r_uv[3] = (ofs_y - (h - dh) * 0.5f) / dh;
}

/**
 * The textures of what is behind the window: a frosted one and a sharper one (with mip-maps,
 * for refraction and diffusion), and how window pixels map to them.
 */
static void glass_backdrop_textures(const int window_size[2],
                                    gpu::Texture **r_frosted,
                                    gpu::Texture **r_sharp,
                                    float r_uv[4])
{
  if (g_backdrop_is_set && g_backdrop.behind) {
    /* Another window is behind this one, its frosted image covers the window. */
    *r_frosted = g_backdrop.behind;
    *r_sharp = g_backdrop.behind;
    r_uv[0] = 1.0f / float(max_ii(window_size[0], 1));
    r_uv[1] = 1.0f / float(max_ii(window_size[1], 1));
    r_uv[2] = 0.0f;
    r_uv[3] = 0.0f;
    return;
  }
  *r_frosted = GPU_offscreen_color_texture(g_wallpaper.frosted);
  *r_sharp = g_wallpaper.image;
  glass_wallpaper_uv_transform(window_size, r_uv);
}

bool glass_frost_copy(gpu::Texture *src, GPUOffScreen **r_dst, GPUOffScreen **r_tmp)
{
  const int w = max_ii(1, GPU_texture_width(src) / 2);
  const int h = max_ii(1, GPU_texture_height(src) / 2);
  for (GPUOffScreen **ofs : {r_dst, r_tmp}) {
    if (*ofs && (GPU_offscreen_width(*ofs) != w || GPU_offscreen_height(*ofs) != h)) {
      GPU_offscreen_free(*ofs);
      *ofs = nullptr;
    }
    if (*ofs == nullptr) {
      *ofs = glass_frosted_offscreen_create(w, h);
    }
    if (*ofs == nullptr) {
      return false;
    }
  }
  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);
  glass_wallpaper_pass(*r_dst, src, 0.0f, float2(0.0f));
  for (int i = 0; i < 2; i++) {
    glass_wallpaper_pass(
        *r_tmp, GPU_offscreen_color_texture(*r_dst), 0.0f, float2(1.5f / float(w), 0.0f));
    glass_wallpaper_pass(
        *r_dst, GPU_offscreen_color_texture(*r_tmp), 0.0f, float2(0.0f, 1.5f / float(h)));
  }
  GPU_blend(old_blend);
  return true;
}

bool glass_backdrop_compose(GPUOffScreen *dst,
                            gpu::Texture *window_frame,
                            const float frame_rect[4])
{
  if (!glass_wallpaper_ensure() || !g_backdrop_is_set) {
    return false;
  }
  const float w = float(max_ii(g_backdrop.window_size[0], 1));
  const float h = float(max_ii(g_backdrop.window_size[1], 1));

  const GPUBlend old_blend = GPU_blend_get();
  GPU_blend(GPU_BLEND_NONE);
  glass_offscreen_begin(dst);

  /* The frosted wallpaper around the other window. Window pixel `p = (ndc * 0.5 + 0.5) * size`. */
  float uv[4];
  glass_wallpaper_uv_transform(g_backdrop.window_size, uv);
  const float full[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
  const float wallpaper_uv[4] = {0.5f * w * uv[0],
                                 0.5f * h * uv[1],
                                 0.5f * w * uv[0] + uv[2],
                                 0.5f * h * uv[1] + uv[3]};
  glass_raw_quad(
      GPU_offscreen_color_texture(g_wallpaper.frosted), full, wallpaper_uv, 0.0f, float2(0.0f));

  /* The other window where it is. */
  const float fw = max_ff(frame_rect[2] - frame_rect[0], 1.0f);
  const float fh = max_ff(frame_rect[3] - frame_rect[1], 1.0f);
  const float rect_ndc[4] = {frame_rect[0] / w * 2.0f - 1.0f,
                             frame_rect[1] / h * 2.0f - 1.0f,
                             frame_rect[2] / w * 2.0f - 1.0f,
                             frame_rect[3] / h * 2.0f - 1.0f};
  const float frame_uv[4] = {
      0.5f * w / fw, 0.5f * h / fh, (0.5f * w - frame_rect[0]) / fw, (0.5f * h - frame_rect[1]) / fh};
  glass_raw_quad(window_frame, rect_ndc, frame_uv, 0.0f, float2(0.0f));

  glass_offscreen_end(dst);
  GPU_blend(old_blend);
  return true;
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
  if (g_backdrop.see_through) {
    /* The desktop (blurred by the system) shows through, with the window tint on it. Written as
     * is: the window has transparency, pre-multiplied. */
    float tint[4];
    theme::get_color_4fv(TH_EDITOR_BORDER, tint);
    const GPUBlend old_blend = GPU_blend_get();
    GPU_blend(GPU_BLEND_NONE);
    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4f(tint[0] * tint[3], tint[1] * tint[3], tint[2] * tint[3], tint[3]);
    immRectf(pos, 0.0f, 0.0f, float(window_size[0]), float(window_size[1]));
    immUnbindProgram();
    GPU_blend(old_blend);
    return;
  }
  if (!glass_wallpaper_ensure()) {
    return;
  }
  float tint[4], params[4], uv[4];
  glass_frosted_look(tint, params);
  gpu::Texture *frosted, *sharp;
  glass_backdrop_textures(window_size, &frosted, &sharp, uv);
  const float blur[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float rect[4] = {0.0f, 0.0f, float(window_size[0]), float(window_size[1])};

  gpu::Batch *batch = GPU_batch_preset_quad();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_GLASS_WALLPAPER);
  GPU_batch_uniform_4fv(batch, "rect_geom", rect);
  GPU_batch_uniform_4fv(batch, "uv_transform", uv);
  GPU_batch_uniform_4fv(batch, "tint", tint);
  GPU_batch_uniform_4fv(batch, "params", params);
  GPU_batch_uniform_4fv(batch, "blur", blur);
  GPU_batch_texture_bind(batch, "image", frosted);

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
  gpu::Texture *frosted, *sharp;
  glass_backdrop_textures(window_size, &frosted, &sharp, uv);
  float frame_tint[4], frame_look[4], card_tint[4], card_look[4];
  glass_frosted_look(frame_tint, frame_look);
  /* The frosted look's first parameter is a mip-map level, the card shader takes saturation,
   * brightness, frost and grain. */
  const float frame_params[4] = {frame_look[1], frame_look[2], 0.0f, frame_look[3]};
  glass_card_look(card_tint, card_look);
  const float optics[4] = {14.0f * scale, 9.0f * scale, 5.0f * scale, 0.09f};
  const float rim[4] = {1.25f * scale, active ? 0.75f : 0.5f, 0.55f, 2.0f};
  /* In a see-through window the cards are milky panes over the desktop the system blurs. */
  const float see_through_opacity = 0.22f;
  const float diffuse[4] = {
      0.5f, 6.5f, glass_corner_exponent(), g_backdrop.see_through ? see_through_opacity : 0.0f};
  if (g_backdrop.see_through) {
    /* The window tint around the cards, pre-multiplied in the shader. */
    theme::get_color_4fv(TH_EDITOR_BORDER, frame_tint);
  }

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
  GPU_batch_uniform_4fv(batch, "diffuse", diffuse);
  GPU_batch_uniform_1i(batch, "card_mode", int(pass));
  GPU_batch_texture_bind(batch, "frosted", frosted);
  GPU_batch_texture_bind(batch, "image", sharp);

  const GPUBlend old_blend = GPU_blend_get();
  if (pass == GlassCardPass::Edge && g_backdrop.see_through) {
    /* Clear what is outside the rounded corners first (the editor is drawn as a rectangle), the
     * window tint that replaces it has transparency. */
    GPU_batch_uniform_1i(batch, "card_mode", 3);
    GPU_blend(GPU_BLEND_MULTIPLY);
    GPU_batch_draw(batch);
    GPU_batch_uniform_1i(batch, "card_mode", int(pass));
  }
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
