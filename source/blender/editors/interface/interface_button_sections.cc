/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Calculating and drawing of bounding boxes for "button sections". That is, each group of buttons
 * separated by a separator spacer button.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_screen.hh"

#include "DNA_screen_types.h"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "UI_glass.hh"

#include "interface_intern.hh"

namespace blender::ui {

/**
 * Calculate a bounding box for each section. Sections will be merged if they are closer than
 * #UI_BUTTON_SECTION_MERGE_DISTANCE.
 *
 * If a section is closer than #UI_BUTTON_SECTION_MERGE_DISTANCE to a region edge, it will be
 * extended to the edge.
 *
 * \return the bounding boxes in region space.
 */
static Vector<rcti> button_section_bounds_calc(const ARegion *region, const bool add_padding)
{
  Vector<rcti> section_bounds;

  const auto finish_section_fn = [&](const rcti cur_section_bounds) {
    if (!section_bounds.is_empty() &&
        std::abs(section_bounds.last().xmax - cur_section_bounds.xmin) <
            UI_BUTTON_SECTION_MERGE_DISTANCE)
    {
      section_bounds.last().xmax = cur_section_bounds.xmax;
    }
    else {
      section_bounds.append(cur_section_bounds);
    }

    rcti &last_bounds = section_bounds.last();
    /* Extend to region edge if close enough. */
    if (last_bounds.xmin <= UI_BUTTON_SECTION_MERGE_DISTANCE) {
      last_bounds.xmin = 0;
    }
    if (last_bounds.xmax >= (region->winx - UI_BUTTON_SECTION_MERGE_DISTANCE)) {
      last_bounds.xmax = region->winx;
    }
  };

  {
    bool has_section_content = false;
    rcti cur_section_bounds;
    BLI_rcti_init_minmax(&cur_section_bounds);

    /* A bit annoying, but this function is called for both drawing and event handling. When
     * drawing, we need to exclude inactive blocks since they mess with the result. However, this
     * active state is only useful during drawing and must be ignored for handling (at which point
     * #Block::active is false for all blocks). */
    const bool is_drawing = region->runtime->do_draw & RGN_DRAWING;
    for (Block &block : region->runtime->uiblocks) {
      if (is_drawing && !block.active) {
        continue;
      }

      for (const Button &but : block.buttons()) {
        if (but.type == ButtonType::SeprSpacer) {
          /* Start a new section. */
          if (has_section_content) {
            finish_section_fn(cur_section_bounds);

            /* Reset for next section. */
            BLI_rcti_init_minmax(&cur_section_bounds);
            has_section_content = false;
          }
          continue;
        }

        rcti but_pixelrect;
        button_to_pixelrect(&but_pixelrect, region, &block, &but);
        BLI_rcti_do_minmax_rcti(&cur_section_bounds, &but_pixelrect);
        has_section_content = true;
      }
    }

    /* Finish last section in case the last button is not a spacer. */
    if (has_section_content) {
      finish_section_fn(cur_section_bounds);
    }
  }

  if (add_padding) {
    const uiStyle *style = style_get_dpi();
    const int pad_x = style->buttonspacex;
    /* Making this based on the header size since this feature is typically used in headers, and
     * this way we are more likely to pad the bounds all the way to the region edge. */
    const int pad_y = ceil((HEADER_PADDING_Y * UI_SCALE_FAC) / 2.0f);

    for (rcti &bounds : section_bounds) {
      BLI_rcti_pad(&bounds, pad_x, pad_y);
      /* Clamp, important for the rounded-corners to draw correct. */
      CLAMP_MIN(bounds.xmin, 0);
      CLAMP_MAX(bounds.xmax, region->winx);
      CLAMP_MIN(bounds.ymin, 0);
      CLAMP_MAX(bounds.ymax, region->winy);
    }
  }

  return section_bounds;
}

static void draw_button_sections_background(const ARegion *region,
                                            const Span<rcti> section_bounds,
                                            const ThemeColorID colorid,
                                            const ButtonSectionsAlign align,
                                            const float corner_radius)
{
  float bg_color[4];
  theme::get_color_4fv(colorid, bg_color);

  for (const rcti &bounds : section_bounds) {
    int roundbox_corners = [align]() -> int {
      switch (align) {
        case ButtonSectionsAlign::Top:
          return CNR_BOTTOM_LEFT | CNR_BOTTOM_RIGHT;
        case ButtonSectionsAlign::Bottom:
          return CNR_TOP_LEFT | CNR_TOP_RIGHT;
        case ButtonSectionsAlign::None:
          return CNR_ALL;
      }
      return CNR_ALL;
    }();

    /* No rounded corners at the region edge. */
    if (bounds.xmin == 0) {
      roundbox_corners &= ~(CNR_TOP_LEFT | CNR_BOTTOM_LEFT);
    }
    if (bounds.xmax >= region->winx) {
      roundbox_corners &= ~(CNR_TOP_RIGHT | CNR_BOTTOM_RIGHT);
    }

    rctf bounds_float;
    BLI_rctf_rcti_copy(&bounds_float, &bounds);
    /* Make space for the separator line. */
    if (align == ButtonSectionsAlign::Top) {
      bounds_float.ymax -= UI_BUTTON_SECTION_SEPERATOR_LINE_WITH;
    }
    else if (align == ButtonSectionsAlign::Bottom) {
      bounds_float.ymin += UI_BUTTON_SECTION_SEPERATOR_LINE_WITH;
    }

    draw_roundbox_corner_set(roundbox_corners);
    draw_roundbox_4fv(&bounds_float, true, corner_radius, bg_color);
  }
}

static void draw_button_sections_alignment_separator(const ARegion *region,
                                                     const Span<rcti> section_bounds,
                                                     const ThemeColorID colorid,
                                                     const ButtonSectionsAlign align,
                                                     const float corner_radius)
{
  const int separator_line_width = UI_BUTTON_SECTION_SEPERATOR_LINE_WITH;

  float bg_color[4];
  theme::get_color_4fv(colorid, bg_color);

  GPU_blend(GPU_BLEND_ALPHA);

  /* Separator line. */
  {
    GPUVertFormat *format = immVertexFormat();
    const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4fv(bg_color);

    if (align == ButtonSectionsAlign::Top) {
      immRectf(pos, 0, region->winy - separator_line_width, region->winx, region->winy);
    }
    else if (align == ButtonSectionsAlign::Bottom) {
      immRectf(pos, 0, 0, region->winx, separator_line_width);
    }
    else {
      BLI_assert_unreachable();
    }
    immUnbindProgram();
  }

  int prev_xmax = 0;
  for (const rcti &bounds : section_bounds) {
    if (prev_xmax != 0) {
      const rcti rounded_corner_rect = {
          prev_xmax, bounds.xmin, separator_line_width, region->winy - separator_line_width};

      draw_roundbox_corner_set(align == ButtonSectionsAlign::Top ?
                                   (CNR_TOP_LEFT | CNR_TOP_RIGHT) :
                                   (CNR_BOTTOM_LEFT | CNR_BOTTOM_RIGHT));
      draw_rounded_corners_inverted(rounded_corner_rect, corner_radius, bg_color);
    }

    prev_xmax = bounds.xmax;
  }

  GPU_blend(GPU_BLEND_NONE);
}

/**
 * GlassMesh: with the glass style the sections are drawn as floating, fully rounded glass pills
 * (inset from the region edges, no separator line).
 */
static void draw_button_sections_glass(const ARegion *region,
                                       const Span<rcti> section_bounds,
                                       const ThemeColorID colorid,
                                       const float aspect)
{
  float bg_color[4];
  theme::get_color_4fv(colorid, bg_color);
  /* Make sure the pills read as glass (and get the frosted backdrop). */
  bg_color[3] = std::max(bg_color[3], 0.55f);
  float outline[4] = {1.0f, 1.0f, 1.0f, 0.1f};

  const float inset_x = 4.0f * UI_SCALE_FAC / aspect;
  const float inset_y = 1.0f * UI_SCALE_FAC / aspect;

  GPU_blend(GPU_BLEND_ALPHA);
  draw_roundbox_corner_set(CNR_ALL);
  for (const rcti &bounds : section_bounds) {
    rctf pill;
    BLI_rctf_rcti_copy(&pill, &bounds);
    pill.xmin = std::max(pill.xmin, inset_x);
    pill.xmax = std::min(pill.xmax, float(region->winx) - inset_x);
    pill.ymin = std::max(pill.ymin, inset_y);
    pill.ymax = std::min(pill.ymax, float(region->winy) - inset_y);
    if (!BLI_rctf_is_valid(&pill)) {
      continue;
    }
    const float radius = std::min(0.5f * BLI_rctf_size_y(&pill), 10.0f * UI_SCALE_FAC / aspect);
    draw_roundbox_4fv_glass(&pill,
                            bg_color,
                            nullptr,
                            1.0f,
                            outline,
                            U.pixelsize,
                            radius,
                            glass_rim_strength() * 1.3f,
                            0.0f);
  }
  GPU_blend(GPU_BLEND_NONE);
}

void region_button_sections_draw(const ARegion *region,
                                 const int /*ThemeColorID*/ colorid,
                                 const ButtonSectionsAlign align)
{
  const float aspect = BLI_rctf_size_x(&region->v2d.cur) /
                       (BLI_rcti_size_x(&region->v2d.mask) + 1);
  const float corner_radius = 4.0f * UI_SCALE_FAC / aspect;

  const Vector<rcti> section_bounds = button_section_bounds_calc(region, true);

  if (glass_enabled()) {
    draw_button_sections_glass(region, section_bounds, ThemeColorID(colorid), aspect);
    return;
  }

  draw_button_sections_background(
      region, section_bounds, ThemeColorID(colorid), align, corner_radius);
  if (align != ButtonSectionsAlign::None) {
    draw_button_sections_alignment_separator(region,
                                             section_bounds,
                                             ThemeColorID(colorid),
                                             align,
                                             /* Slightly bigger corner radius, looks better. */
                                             corner_radius + 1);
  }
}

bool region_button_sections_is_inside_x(const ARegion *region, const int mval_x)
{
  const Vector<rcti> section_bounds = button_section_bounds_calc(region, true);

  for (const rcti &bounds : section_bounds) {
    if (BLI_rcti_isect_x(&bounds, mval_x)) {
      return true;
    }
  }
  return false;
}

void region_button_rects_get(const ARegion *region, Vector<rcti> &r_rects)
{
  for (const Block &block : region->runtime->uiblocks) {
    for (const Button &but : block.buttons()) {
      if ((but.flag & (UI_HIDDEN | UI_SCROLLED)) ||
          ELEM(but.type,
               ButtonType::Sepr,
               ButtonType::SeprLine,
               ButtonType::SeprSpacer,
               ButtonType::Label))
      {
        continue;
      }
      rcti rect;
      button_to_pixelrect(&rect, region, &block, &but);
      BLI_rcti_translate(&rect, region->winrct.xmin, region->winrct.ymin);
      r_rects.append(rect);
    }
  }
}

}  // namespace blender::ui
