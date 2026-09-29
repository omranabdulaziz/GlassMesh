/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_glass_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_glass_backdrop)

void main()
{
  /* `pos` is in [0..1] range (#GPU_batch_preset_quad). */
  float2 co = mix(rect_geom.xy, rect_geom.zw, pos);
  win_co = co;
  gl_Position = ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
}
