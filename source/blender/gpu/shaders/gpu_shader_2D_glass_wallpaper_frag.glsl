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

float3 glass_sample(float2 uv)
{
  if (blur.x == 0.0f && blur.y == 0.0f) {
    return textureLod(image, uv, params.x).rgb;
  }
  /* 9-tap Gaussian using bilinear filtering (5 fetches). */
  float2 step = blur.xy;
  float3 sum = textureLod(image, uv, params.x).rgb * 0.2270270270f;
  sum += textureLod(image, uv + step * 1.3846153846f, params.x).rgb * 0.3162162162f;
  sum += textureLod(image, uv - step * 1.3846153846f, params.x).rgb * 0.3162162162f;
  sum += textureLod(image, uv + step * 3.2307692308f, params.x).rgb * 0.0702702703f;
  sum += textureLod(image, uv - step * 3.2307692308f, params.x).rgb * 0.0702702703f;
  return sum;
}

void main()
{
  float2 uv = win_co * uv_transform.xy + uv_transform.zw;
  float3 color = glass_sample(uv);

  if (blur.z != 0.0f) {
    /* Off-screen pre-processing pass. */
    fragColor = float4(color, 1.0f);
    return;
  }

  float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
  color = mix(float3(luminance), color, params.y) * params.z;
  color = mix(color, tint.rgb, tint.a);
  /* Grain, also hides banding of the smooth blurred gradients. */
  color += (glass_noise(win_co) - 0.5f) * params.w;

  fragColor = blender_srgb_to_framebuffer_space(float4(clamp(color, 0.0f, 1.0f), 1.0f));
}
