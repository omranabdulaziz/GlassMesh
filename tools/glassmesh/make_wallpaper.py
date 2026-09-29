# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Render GlassMesh's default wallpaper: an original, fully procedural alpine lake.

GlassMesh draws this image behind its translucent interface (see `interface_glass.cc`),
like a desktop wallpaper seen through glass. Everything in the scene is generated here,
no external images or assets are used.

Usage (any Blender 5.x build with Cycles):

  blender --background --factory-startup --python tools/glassmesh/make_wallpaper.py -- \\
      --output release/datafiles/glassmesh/wallpaper.jpg [--width 2560 --height 1600 --samples 96]
"""

import argparse
import math
import os
import random
import sys

import bpy
import mathutils
from mathutils import Vector
from mathutils import noise


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--width", type=int, default=2560)
    parser.add_argument("--height", type=int, default=1600)
    parser.add_argument("--samples", type=int, default=96)
    parser.add_argument("--grid", type=int, default=1100, help="Terrain resolution (X)")
    return parser.parse_args(argv)


HAZE = 0.45


def smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


# -----------------------------------------------------------------------------
# Terrain

def terrain_height(x, y):
    """Height field of a valley with a lake, side hills and a far mountain range."""
    p = Vector((x / 520.0, y / 520.0, 0.37))
    ridged = noise.ridged_multi_fractal(p, 0.75, 2.2, 9, 1.0, 2.6, noise_basis='PERLIN_ORIGINAL')
    detail = noise.fractal(Vector((x / 120.0, y / 120.0, 1.7)), 0.7, 2.0, 6, noise_basis='PERLIN_ORIGINAL')

    # Main range, rising behind the lake.
    far = smoothstep(520.0, 1500.0, y)
    main = far * (430.0 + 300.0 * noise.noise(Vector((x / 1400.0, 3.1, 0.0))))
    # Hills closing the valley on both sides.
    side = smoothstep(260.0, 900.0, abs(x - 60.0 * math.sin(y / 300.0))) * smoothstep(60.0, 420.0, y)
    hills = side * 200.0

    envelope = max(main, hills)
    h = envelope * (0.15 + 0.85 * min(ridged / 1.8, 1.4)) + detail * 22.0 * (0.3 + envelope / 300.0)

    # Lake basin: below the water level where the envelope is low.
    shore = smoothstep(4.0, 60.0, envelope)
    height = h * shore + (1.0 - shore) * (-6.0 + detail * 2.0)
    # Snow on the upper part of each mountain, relative to its size.
    snow = smoothstep(0.3, 0.52, height / max(envelope, 1.0)) * smoothstep(90.0, 170.0, height)
    return height, snow


def make_terrain(res_x):
    res_y = int(res_x * 0.62)
    x0, x1 = -3200.0, 3200.0
    y0, y1 = 40.0, 3600.0
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=res_x, y_subdivisions=res_y, size=1.0)
    ob = bpy.context.active_object
    ob.name = "Terrain"
    me = ob.data
    coords = [0.0] * (len(me.vertices) * 3)
    snow = [0.0] * len(me.vertices)
    me.vertices.foreach_get("co", coords)
    for i in range(0, len(coords), 3):
        # Denser rows close to the camera: map the grid's Y quadratically.
        u = coords[i] + 0.5
        v = coords[i + 1] + 0.5
        x = x0 + (x1 - x0) * u
        y = y0 + (y1 - y0) * (v * v)
        coords[i] = x
        coords[i + 1] = y
        coords[i + 2], snow[i // 3] = terrain_height(x, y)
    me.vertices.foreach_set("co", coords)
    me.attributes.new("snow", 'FLOAT', 'POINT').data.foreach_set("value", snow)
    me.update()
    for poly in me.polygons:
        poly.use_smooth = True
    return ob


def make_rocks():
    """A few boulders in the lower corners, like a shore in front of the camera."""
    rng = random.Random(7)
    rocks = []
    placements = [(-9.5, 9.0, 2.6), (-6.8, 12.5, 1.5), (-12.0, 16.0, 2.2), (8.8, 8.5, 2.4),
                  (11.5, 13.0, 1.9), (6.4, 11.8, 1.1), (-4.5, 7.0, 0.8), (13.5, 20.0, 2.8)]
    for (x, y, s) in placements:
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=5, radius=1.0, location=(x, y, -0.2))
        ob = bpy.context.active_object
        me = ob.data
        seed = Vector((rng.random() * 50.0, rng.random() * 50.0, rng.random() * 50.0))
        for v in me.vertices:
            n = v.co.normalized()
            d = noise.fractal(n * 1.6 + seed, 0.7, 2.0, 5, noise_basis='PERLIN_ORIGINAL')
            v.co = n * (1.0 + 0.35 * d)
            v.co.z *= 0.55
        ob.scale = (s * (0.9 + 0.4 * rng.random()), s * (0.8 + 0.4 * rng.random()), s)
        ob.rotation_euler = (0.0, 0.0, rng.random() * math.tau)
        for poly in me.polygons:
            poly.use_smooth = True
        rocks.append(ob)
    return rocks


# -----------------------------------------------------------------------------
# Materials

def node(nt, kind, loc, **props):
    n = nt.nodes.new(kind)
    n.location = loc
    for key, value in props.items():
        setattr(n, key, value)
    return n


def aerial_perspective(nt, color_socket, strength=1.0):
    """Mix a surface color towards the sky color with distance (cheap aerial perspective)."""
    cam = node(nt, "ShaderNodeCameraData", (-900, -400))
    dist = node(nt, "ShaderNodeMath", (-700, -400), operation='MULTIPLY')
    nt.links.new(cam.outputs["View Distance"], dist.inputs[0])
    dist.inputs[1].default_value = -1.0 / 6500.0
    exp = node(nt, "ShaderNodeMath", (-550, -400), operation='EXPONENT')
    nt.links.new(dist.outputs[0], exp.inputs[0])
    inv = node(nt, "ShaderNodeMath", (-400, -400), operation='SUBTRACT')
    inv.inputs[0].default_value = 1.0
    nt.links.new(exp.outputs[0], inv.inputs[1])
    fac = node(nt, "ShaderNodeMath", (-250, -400), operation='MULTIPLY')
    nt.links.new(inv.outputs[0], fac.inputs[0])
    fac.inputs[1].default_value = strength
    mix = node(nt, "ShaderNodeMix", (-100, -200), data_type='RGBA', blend_type='MIX')
    nt.links.new(fac.outputs[0], mix.inputs["Factor"])
    nt.links.new(color_socket, mix.inputs["A"])
    mix.inputs["B"].default_value = (0.36, 0.52, 0.78, 1.0)
    return mix.outputs["Result"]


def terrain_material():
    mat = bpy.data.materials.new("Terrain")
    nt = mat.node_tree
    nt.nodes.clear()
    out = node(nt, "ShaderNodeOutputMaterial", (700, 0))
    bsdf = node(nt, "ShaderNodeBsdfPrincipled", (400, 0))
    nt.links.new(bsdf.outputs[0], out.inputs[0])

    geo = node(nt, "ShaderNodeNewGeometry", (-1600, 200))
    sep = node(nt, "ShaderNodeSeparateXYZ", (-1400, 300))
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    sepn = node(nt, "ShaderNodeSeparateXYZ", (-1400, 50))
    nt.links.new(geo.outputs["Normal"], sepn.inputs[0])

    tex = node(nt, "ShaderNodeTexNoise", (-1400, -200))
    tex.inputs["Scale"].default_value = 0.02
    tex.inputs["Detail"].default_value = 8.0
    nt.links.new(geo.outputs["Position"], tex.inputs["Vector"])

    # Snow: from the terrain's "snow" attribute, broken up by noise, and not on cliffs.
    attr = node(nt, "ShaderNodeAttribute", (-1400, 450), attribute_name="snow")
    breakup = node(nt, "ShaderNodeMath", (-1200, 300), operation='MULTIPLY_ADD')
    nt.links.new(tex.outputs["Fac"], breakup.inputs[0])
    breakup.inputs[1].default_value = 1.2
    breakup.inputs[2].default_value = -0.6
    snow_raw = node(nt, "ShaderNodeMath", (-1000, 300), operation='ADD')
    nt.links.new(attr.outputs["Fac"], snow_raw.inputs[0])
    nt.links.new(breakup.outputs[0], snow_raw.inputs[1])
    hs = node(nt, "ShaderNodeMapRange", (-800, 300))
    nt.links.new(snow_raw.outputs[0], hs.inputs["Value"])
    hs.inputs["From Min"].default_value = 0.35
    hs.inputs["From Max"].default_value = 0.65
    slope = node(nt, "ShaderNodeMapRange", (-800, 50))
    nt.links.new(sepn.outputs["Z"], slope.inputs["Value"])
    slope.inputs["From Min"].default_value = 0.0
    slope.inputs["From Max"].default_value = 0.2
    snow = node(nt, "ShaderNodeMath", (-600, 200), operation='MULTIPLY')
    nt.links.new(hs.outputs[0], snow.inputs[0])
    nt.links.new(slope.outputs[0], snow.inputs[1])

    # Rock with some variation, forest in the valley.
    rock_ramp = node(nt, "ShaderNodeValToRGB", (-1000, -200))
    rock_ramp.color_ramp.elements[0].color = (0.035, 0.036, 0.04, 1.0)
    rock_ramp.color_ramp.elements[1].color = (0.13, 0.12, 0.11, 1.0)
    nt.links.new(tex.outputs["Fac"], rock_ramp.inputs[0])
    low = node(nt, "ShaderNodeMapRange", (-1000, -450))
    nt.links.new(sep.outputs["Z"], low.inputs["Value"])
    low.inputs["From Min"].default_value = 130.0
    low.inputs["From Max"].default_value = 30.0
    forest = node(nt, "ShaderNodeMix", (-750, -300), data_type='RGBA')
    nt.links.new(low.outputs[0], forest.inputs["Factor"])
    nt.links.new(rock_ramp.outputs["Color"], forest.inputs["A"])
    forest.inputs["B"].default_value = (0.012, 0.03, 0.02, 1.0)

    color = node(nt, "ShaderNodeMix", (-400, 0), data_type='RGBA')
    nt.links.new(snow.outputs[0], color.inputs["Factor"])
    nt.links.new(forest.outputs["Result"], color.inputs["A"])
    color.inputs["B"].default_value = (0.86, 0.9, 0.97, 1.0)

    nt.links.new(aerial_perspective(nt, color.outputs["Result"], HAZE), bsdf.inputs["Base Color"])
    rough = node(nt, "ShaderNodeMapRange", (0, -300))
    nt.links.new(snow.outputs[0], rough.inputs["Value"])
    rough.inputs["To Min"].default_value = 0.85
    rough.inputs["To Max"].default_value = 0.45
    nt.links.new(rough.outputs[0], bsdf.inputs["Roughness"])

    bump = node(nt, "ShaderNodeBump", (100, -500))
    bump.inputs["Strength"].default_value = 0.25
    detail = node(nt, "ShaderNodeTexNoise", (-200, -600))
    # Features of tens of meters: finer bumps are sub-pixel at this distance and only add noise.
    detail.inputs["Scale"].default_value = 0.03
    detail.inputs["Detail"].default_value = 6.0
    nt.links.new(geo.outputs["Position"], detail.inputs["Vector"])
    nt.links.new(detail.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
    if os.environ.get("GLASSMESH_WALLPAPER_DEBUG") == "snow":
        emit = node(nt, "ShaderNodeEmission", (400, 300))
        nt.links.new(snow.outputs[0], emit.inputs["Color"])
        nt.links.new(emit.outputs[0], out.inputs[0])
    return mat


def rock_material():
    mat = bpy.data.materials.new("Rock")
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tex = node(nt, "ShaderNodeTexNoise", (-700, 0))
    tex.inputs["Scale"].default_value = 3.0
    tex.inputs["Detail"].default_value = 12.0
    ramp = node(nt, "ShaderNodeValToRGB", (-450, 0))
    ramp.color_ramp.elements[0].color = (0.012, 0.013, 0.016, 1.0)
    ramp.color_ramp.elements[1].color = (0.12, 0.115, 0.11, 1.0)
    nt.links.new(tex.outputs["Fac"], ramp.inputs[0])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.7
    bump = node(nt, "ShaderNodeBump", (-200, -300))
    bump.inputs["Strength"].default_value = 0.6
    nt.links.new(tex.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
    return mat


def water_material():
    mat = bpy.data.materials.new("Water")
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    coord = node(nt, "ShaderNodeTexCoord", (-900, 0))
    sep = node(nt, "ShaderNodeSeparateXYZ", (-700, 0))
    nt.links.new(coord.outputs["Object"], sep.inputs[0])
    depth = node(nt, "ShaderNodeMapRange", (-500, 0))
    nt.links.new(sep.outputs["Y"], depth.inputs["Value"])
    depth.inputs["From Min"].default_value = 8.0
    depth.inputs["From Max"].default_value = 160.0
    col = node(nt, "ShaderNodeMix", (-300, 0), data_type='RGBA')
    nt.links.new(depth.outputs[0], col.inputs["Factor"])
    col.inputs["A"].default_value = (0.008, 0.11, 0.13, 1.0)
    col.inputs["B"].default_value = (0.001, 0.012, 0.03, 1.0)
    nt.links.new(col.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.015
    bsdf.inputs["IOR"].default_value = 1.33
    waves = node(nt, "ShaderNodeTexNoise", (-500, -300))
    waves.inputs["Scale"].default_value = 1.4
    waves.inputs["Detail"].default_value = 6.0
    mapping = node(nt, "ShaderNodeMapping", (-700, -300))
    mapping.inputs["Scale"].default_value = (1.0, 3.5, 1.0)
    nt.links.new(coord.outputs["Object"], mapping.inputs["Vector"])
    nt.links.new(mapping.outputs[0], waves.inputs["Vector"])
    bump = node(nt, "ShaderNodeBump", (-200, -300))
    bump.inputs["Strength"].default_value = 0.05
    nt.links.new(waves.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
    return mat


def world_setup():
    world = bpy.data.worlds.new("Sky")
    bpy.context.scene.world = world
    nt = world.node_tree
    nt.nodes.clear()
    out = node(nt, "ShaderNodeOutputWorld", (800, 0))
    bg = node(nt, "ShaderNodeBackground", (600, 0))
    nt.links.new(bg.outputs[0], out.inputs[0])
    coord = node(nt, "ShaderNodeTexCoord", (-900, -200))
    sep = node(nt, "ShaderNodeSeparateXYZ", (-700, -200))
    nt.links.new(coord.outputs["Generated"], sep.inputs[0])

    # Sky: a clear, deep blue at the zenith, lighter towards the horizon.
    up = node(nt, "ShaderNodeMath", (-500, 300), operation='MAXIMUM')
    nt.links.new(sep.outputs["Z"], up.inputs[0])
    up.inputs[1].default_value = 0.0
    curve = node(nt, "ShaderNodeMath", (-300, 300), operation='POWER')
    nt.links.new(up.outputs[0], curve.inputs[0])
    curve.inputs[1].default_value = 0.55
    sky = node(nt, "ShaderNodeValToRGB", (-100, 300))
    ramp = sky.color_ramp
    ramp.elements[0].color = (0.62, 0.79, 0.97, 1.0)
    ramp.elements[1].position = 1.0
    ramp.elements[1].color = (0.02, 0.13, 0.5, 1.0)
    mid = ramp.elements.new(0.3)
    mid.color = (0.2, 0.44, 0.86, 1.0)
    nt.links.new(curve.outputs[0], sky.inputs[0])

    # Soft clouds: noise projected on a plane above the camera, fading towards the horizon.
    zc = node(nt, "ShaderNodeMath", (-500, -300), operation='MAXIMUM')
    nt.links.new(sep.outputs["Z"], zc.inputs[0])
    zc.inputs[1].default_value = 0.02
    proj = node(nt, "ShaderNodeVectorMath", (-300, -200), operation='DIVIDE')
    nt.links.new(coord.outputs["Generated"], proj.inputs[0])
    comb = node(nt, "ShaderNodeCombineXYZ", (-450, -450))
    nt.links.new(zc.outputs[0], comb.inputs[0])
    nt.links.new(zc.outputs[0], comb.inputs[1])
    nt.links.new(zc.outputs[0], comb.inputs[2])
    nt.links.new(comb.outputs[0], proj.inputs[1])
    clouds = node(nt, "ShaderNodeTexNoise", (-100, -200))
    clouds.inputs["Scale"].default_value = 1.1
    clouds.inputs["Detail"].default_value = 9.0
    clouds.inputs["Roughness"].default_value = 0.62
    nt.links.new(proj.outputs[0], clouds.inputs["Vector"])
    shape = node(nt, "ShaderNodeMapRange", (100, -200))
    nt.links.new(clouds.outputs["Fac"], shape.inputs["Value"])
    shape.inputs["From Min"].default_value = 0.53
    shape.inputs["From Max"].default_value = 0.76
    horizon = node(nt, "ShaderNodeMapRange", (100, -450))
    nt.links.new(sep.outputs["Z"], horizon.inputs["Value"])
    horizon.inputs["From Min"].default_value = 0.03
    horizon.inputs["From Max"].default_value = 0.35
    cover = node(nt, "ShaderNodeMath", (300, -300), operation='MULTIPLY')
    nt.links.new(shape.outputs[0], cover.inputs[0])
    nt.links.new(horizon.outputs[0], cover.inputs[1])
    mix = node(nt, "ShaderNodeMix", (400, 0), data_type='RGBA')
    nt.links.new(cover.outputs[0], mix.inputs["Factor"])
    nt.links.new(sky.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (1.25, 1.27, 1.32, 1.0)
    nt.links.new(mix.outputs["Result"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0
    return world


# -----------------------------------------------------------------------------

def main():
    args = parse_args()
    scene = bpy.context.scene
    for ob in list(scene.objects):
        bpy.data.objects.remove(ob)

    terrain = make_terrain(args.grid)
    terrain.data.materials.append(terrain_material())

    rock_mat = rock_material()
    for rock in make_rocks():
        rock.data.materials.append(rock_mat)

    bpy.ops.mesh.primitive_plane_add(size=12000.0, location=(0.0, 2000.0, 0.0))
    water = bpy.context.active_object
    water.name = "Water"
    water.data.materials.append(water_material())

    world_setup()

    sun_data = bpy.data.lights.new("Sun", 'SUN')
    sun_data.energy = 4.0
    sun_data.angle = math.radians(1.0)
    sun_data.color = (1.0, 0.96, 0.9)
    sun = bpy.data.objects.new("Sun", sun_data)
    scene.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(62.0), 0.0, math.radians(-75.0))

    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 26.0
    cam = bpy.data.objects.new("Camera", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (0.0, 0.0, 3.2)
    cam.rotation_euler = (math.radians(88.5), 0.0, 0.0)
    scene.camera = cam

    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = args.samples
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 4
    scene.cycles.glossy_bounces = 2
    scene.cycles.transmission_bounces = 2
    scene.cycles.diffuse_bounces = 2
    scene.render.resolution_x = args.width
    scene.render.resolution_y = args.height
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Punchy'
    scene.view_settings.exposure = 0.45
    scene.render.image_settings.file_format = 'JPEG'
    scene.render.image_settings.quality = 90
    scene.render.filepath = args.output
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
