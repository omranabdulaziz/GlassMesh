/* SPDX-FileCopyrightText: 2026 GlassMesh Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_glass_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_glass_card)

#include "gpu_shader_colorspace_lib.glsl"

float glass_noise(float2 co)
{
  return fract(52.9829189f * fract(dot(co, float2(0.06711056f, 0.00583715f))));
}

/* Signed distance to the rounded card (negative inside), and the outward normal of its edge.
 * The corners are continuous ("squircle") superellipses, or circular arcs. */
float card_sdf(float2 p, out float2 r_normal)
{
  float2 center = (card_rect.xy + card_rect.zw) * 0.5f;
  float2 half_size = max((card_rect.zw - card_rect.xy) * 0.5f, float2(1.0f));
  float radius = max(min(card_shape.x, min(half_size.x, half_size.y)), 1e-3f);
  float2 rel = p - center;
  float2 side = float2(rel.x < 0.0f ? -1.0f : 1.0f, rel.y < 0.0f ? -1.0f : 1.0f);
  float2 q = abs(rel) - half_size + radius;
  float2 corner = max(q, float2(0.0f));
  float n = diffuse.z;
  float outside;
  if (n > 2.0f) {
    float2 c = corner / radius;
    outside = pow(pow(c.x, n) + pow(c.y, n), 1.0f / n) * radius;
    if (q.x > 0.0f && q.y > 0.0f) {
      r_normal = normalize(float2(pow(c.x, n - 1.0f), pow(c.y, n - 1.0f)) + 1e-6f) * side;
    }
  }
  else {
    outside = length(corner);
    if (q.x > 0.0f && q.y > 0.0f) {
      r_normal = normalize(q) * side;
    }
  }
  if (!(q.x > 0.0f && q.y > 0.0f)) {
    r_normal = (q.x > q.y) ? float2(side.x, 0.0f) : float2(0.0f, side.y);
  }
  return outside + min(max(q.x, q.y), 0.0f) - radius;
}

float2 wallpaper_uv(float2 co)
{
  return co * uv_transform.xy + uv_transform.zw;
}

float3 glass_look(float3 color, float4 tint, float4 look, float2 co)
{
  /* Vibrancy: glass makes what is behind it more vivid, not grayer. */
  float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
  color = mix(float3(luma), color, look.x) * look.y;
  color = mix(color, float3(1.0f), look.z);
  color = mix(color, tint.rgb, tint.a);
  /* Grain, also hides banding of the smooth blurred gradients. */
  color += (glass_noise(co) - 0.5f) * look.w;
  return clamp(color, float3(0.0f), float3(1.0f));
}

/* Opacity of the card's soft drop shadow at `co` (the card casts it slightly downwards). */
float shadow_alpha(float2 co)
{
  float2 normal;
  float dist = card_sdf(co + float2(0.0f, card_shape.z), normal);
  float fac = 1.0f - smoothstep(-card_shape.y * 0.25f, card_shape.y, dist);
  return fac * fac * card_shape.w;
}

/* How much of the light, coming from the upper left, an edge with this normal catches. Like real
 * glass, the edge opposite of the light lights up too (light leaving the pane), a bit less. */
float edge_light(float2 normal, float opposite)
{
  float facing = dot(normal, normalize(float2(-0.45f, 0.9f)));
  float light = 0.3f + 0.7f * pow(abs(facing), 1.5f);
  return (facing < 0.0f) ? light * opposite : light;
}

void main()
{
  float2 normal;
  float dist = card_sdf(win_co, normal);
  /* 1 inside the card, 0 outside, anti-aliased. */
  float coverage = 1.0f - smoothstep(-0.5f, 0.5f, dist);
  /* Distance from the edge towards the inside, in pixels. */
  float depth = max(-dist, 0.0f);

  if (card_mode == 0) {
    fragColor = float4(0.0f, 0.0f, 0.0f, shadow_alpha(win_co) * (1.0f - coverage));
    return;
  }

  if (card_mode == 1) {
    if (coverage <= 0.0f) {
      fragColor = float4(0.0f);
      return;
    }
    /* Refraction: towards the edge the pane bends light, it shows what is further out, and a
     * sharper, compressed image of it. */
    float lens = 1.0f - smoothstep(0.0f, optics.x, depth);
    lens *= lens;
    float3 color = texture(frosted, wallpaper_uv(win_co + normal * (optics.y * lens))).rgb;
    float3 sharp = textureLod(image, wallpaper_uv(win_co + normal * (optics.y * 2.5f * lens)), rim.w)
                       .rgb;
    color = mix(color, sharp, lens * 0.45f);
    /* Diffusion: frosted glass scatters light, large shapes behind it melt into their average
     * color instead of showing through as they are (that is what a tinted blur, "Mica", does). */
    float3 average = textureLod(image, wallpaper_uv(win_co), diffuse.y).rgb;
    color = mix(color, average, diffuse.x * (1.0f - lens));
    color = glass_look(color, card_tint, card_look, win_co);
    /* Bevel: light caught inside the thickness of the pane along its edge. */
    float bevel = 1.0f - smoothstep(0.0f, optics.z, depth);
    color += bevel * bevel * optics.w * edge_light(normal, 0.6f);
    fragColor = blender_srgb_to_framebuffer_space(float4(clamp(color, 0.0f, 1.0f), 1.0f));
    fragColor *= coverage;
    return;
  }

  /* Outside of the rounded corners: the window background (with this card's shadow). */
  float4 outside = float4(0.0f);
  if (coverage < 1.0f) {
    float3 frame = glass_look(texture(frosted, wallpaper_uv(win_co)).rgb, frame_tint, frame_look,
                              win_co);
    frame *= 1.0f - shadow_alpha(win_co);
    outside = blender_srgb_to_framebuffer_space(float4(frame, 1.0f)) * (1.0f - coverage);
  }
  /* Specular rim along the inside of the edge, added as light over the editor. */
  float rim_mask = coverage * (1.0f - smoothstep(rim.x - 0.5f, rim.x + 0.5f, depth));
  float light = rim_mask * rim.y * edge_light(normal, rim.z);
  fragColor = outside + float4(float3(light), light * 0.5f);
}
