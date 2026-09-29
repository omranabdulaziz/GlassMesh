/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_glass_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_glass_wallpaper)

#include "gpu_shader_colorspace_lib.glsl"

float glass_noise(float2 co)
{
  return fract(52.9829189f * fract(dot(co, float2(0.06711056f, 0.00583715f))));
}

/* Large soft blob, `p` and `center` are in aspect corrected [0..1] space. */
float glass_blob(float2 p, float2 center, float radius)
{
  float d = length(p - center) / radius;
  return exp(-d * d * 2.2f);
}

void main()
{
  float2 size = max(window_size.xy, float2(1.0f));
  float2 p = win_co / size;
  /* Keep blobs round regardless of the window aspect ratio. */
  float aspect = size.x / size.y;
  float2 pa = float2(p.x * aspect, p.y);

  /* Base: vertical gradient, lighter "sky" at the top, deeper "water" at the bottom. */
  float3 color = mix(color_bottom.rgb, color_top.rgb, smoothstep(0.0f, 1.0f, p.y));
  /* A soft horizon band. */
  float horizon = (p.y - 0.58f) * 6.0f;
  color = mix(color, color_top.rgb * 1.08f, exp(-horizon * horizon) * 0.35f);
  /* Blobs. */
  color = mix(color, color_accent1.rgb, glass_blob(pa, float2(0.12f * aspect, 0.82f), 0.55f) * 0.55f);
  color = mix(color, color_accent2.rgb, glass_blob(pa, float2(0.86f * aspect, 0.22f), 0.60f) * 0.50f);
  color = mix(color, color_accent1.rgb, glass_blob(pa, float2(0.62f * aspect, 1.05f), 0.45f) * 0.30f);
  color = mix(color, color_bottom.rgb * 0.8f, glass_blob(pa, float2(0.35f * aspect, -0.1f), 0.5f) * 0.45f);
  /* Very subtle grain to avoid banding. */
  color += (glass_noise(win_co) - 0.5f) * (1.5f / 255.0f);

  float alpha = 1.0f;
  if (border_mode) {
    /* Same coverage as `gpu_shader_2D_area_borders_frag.glsl`. */
    float dist = (length(uv_border) - (0.98f - width)) * scale;
    alpha = smoothstep(-0.09f, 1.09f, dist);
  }

  fragColor = blender_srgb_to_framebuffer_space(float4(clamp(color, 0.0f, 1.0f), 1.0f));
  fragColor.a = alpha;
}
