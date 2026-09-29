/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_glass_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_glass_wallpaper)

void main()
{
  float2 final_pos;
  if (border_mode) {
    /* Same as `gpu_shader_2D_area_borders_vert.glsl`. */
    int corner_id = (gl_VertexID / cornerLen) % 4;
    bool inner = all(lessThan(abs(pos), float2(1.0f)));
    final_pos = pos * ((inner) ? (1.0f - width) : 1.05f);
    uv_border = final_pos;
    if (corner_id == 0) {
      final_pos = (final_pos - float2(1.0f, 1.0f)) * scale + rect_geom.yw;
    }
    else if (corner_id == 1) {
      final_pos = (final_pos - float2(-1.0f, 1.0f)) * scale + rect_geom.xw;
    }
    else if (corner_id == 2) {
      final_pos = (final_pos - float2(-1.0f, -1.0f)) * scale + rect_geom.xz;
    }
    else {
      final_pos = (final_pos - float2(1.0f, -1.0f)) * scale + rect_geom.yz;
    }
  }
  else {
    /* `pos` is in [0..1] range (#GPU_batch_preset_quad). */
    final_pos = mix(rect_geom.xy, rect_geom.zw, pos);
    uv_border = float2(0.0f);
  }
  win_co = final_pos;
  gl_Position = ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f);
}
