/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_glass_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_glass_backdrop)

#include "gpu_shader_colorspace_lib.glsl"

#define GLASS_BLUR_SAMPLES 24

/* Interleaved gradient noise, stable in window space. */
float glass_noise(float2 co)
{
  return fract(52.9829189f * fract(dot(co, float2(0.06711056f, 0.00583715f))));
}

void main()
{
  float2 mask_uv = (win_co - mask_rect.xy) / mask_rect.zw;
  float mask_alpha = texture(mask, mask_uv).a;
  float coverage = smoothstep(mask_threshold.x, mask_threshold.y, mask_alpha);
  if (backdrop_mode == 2) {
    fragColor = float4(1.0f - coverage);
    return;
  }
  if (coverage <= 0.0f) {
    fragColor = float4(0.0f);
    return;
  }

  float radius = max(params.x, 1.0f);
  /* The backdrop has a mip-map chain: sample a level that roughly matches the spacing between
   * the samples so the blur stays smooth with a small amount of taps. */
  float lod = max(0.0f, log2(radius / 5.0f));
  float noise = glass_noise(win_co);
  float angle = noise * 6.2831853f;

  float4 accum = float4(0.0f);
  float weight_sum = 0.0f;
  for (int i = 0; i < GLASS_BLUR_SAMPLES; i++) {
    /* Vogel (golden angle) spiral, rotated per pixel. */
    float t = (float(i) + 0.5f) / float(GLASS_BLUR_SAMPLES);
    float r = sqrt(t) * radius;
    float theta = float(i) * 2.3999632f + angle;
    float2 ofs = float2(cos(theta), sin(theta)) * r;
    float2 uv = (win_co + ofs - backdrop_rect.xy) / backdrop_rect.zw;
    /* Gaussian-like falloff. */
    float w = exp(-2.0f * t);
    accum += textureLod(backdrop, uv, lod) * w;
    weight_sum += w;
  }
  accum /= weight_sum;
  /* A see-through window has transparency: the backdrop is pre-multiplied. */
  float alpha = (backdrop_mode == 1) ? accum.a : 1.0f;
  float3 color = (backdrop_mode == 1) ? accum.rgb / max(alpha, 1e-4f) : accum.rgb;

  /* "Vibrancy": slightly boost saturation and brightness like frosted glass does. */
  float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
  color = mix(float3(luma), color, params.z) * params.w;
  color = mix(color, tint.rgb, tint.a);
  /* Subtle grain so the glass doesn't look like a flat blur. */
  color += (noise - 0.5f) * params.y;
  color = clamp(color, float3(0.0f), float3(1.0f));

  fragColor = blender_srgb_to_framebuffer_space(float4(color, 1.0f));
  /* Pre-multiplied output. */
  fragColor *= alpha * coverage;
}
