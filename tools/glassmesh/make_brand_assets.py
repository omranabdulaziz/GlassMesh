#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Generate the GlassMesh logo, application icons and splash screen.

The artwork is original and fully procedural: a rounded "glass" tile with a translucent
geodesic (subdivided icosahedron) mesh sphere. It is deliberately unrelated to the Blender logo.

Requirements (Linux): Python 3 with Pillow, ``rsvg-convert`` (librsvg), ``inkscape`` (to convert
text to paths for the monochrome UI logo), ``png2icns`` (icnsutils) and the "Inter" font.

Usage (from the repository root)::

    python3 tools/glassmesh/make_brand_assets.py

Outputs (all are committed, re-run this script after changing the design):

- ``release/datafiles/glassmesh/glassmesh_logo.svg``       Full color master logo.
- ``release/datafiles/glassmesh/splash.png``               Splash screen image.
- ``release/datafiles/icons_svg/blender.svg``              Monochrome UI logo (app menu).
- ``release/datafiles/icons_svg/blender_logo_large.svg``   Monochrome UI logo + wordmark (About).
- ``release/freedesktop/icons/scalable/apps/blender.svg``  Linux application icon.
- ``release/freedesktop/icons/symbolic/apps/blender-symbolic.svg``
- ``release/windows/icons/winblender.ico``                 Windows application icon.
- ``release/darwin/Blender.app/Contents/Resources/glassmesh_icon.icns``  macOS icon.
- ``intern/ghost/intern/GHOST_IconX11.hh``                 X11 window icon pixels.

File names are kept where the build system expects them, only their content changes.
"""

import io
import math
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

# -----------------------------------------------------------------------------
# Geometry: a geodesic sphere (icosahedron subdivided once).


def normalize(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / length, v[1] / length, v[2] / length)


def geodesic_sphere(subdivisions=1):
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [normalize(v) for v in (
        (-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
        (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
        (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1),
    )]
    faces = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
    ]
    for _ in range(subdivisions):
        cache = {}

        def midpoint(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                va, vb = verts[a], verts[b]
                verts.append(normalize(((va[0] + vb[0]) / 2, (va[1] + vb[1]) / 2, (va[2] + vb[2]) / 2)))
                cache[key] = len(verts) - 1
            return cache[key]

        new_faces = []
        for a, b, c in faces:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            new_faces += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new_faces
    return verts, faces


def rotate(v, rx, ry, rz):
    x, y, z = v
    # X axis.
    c, s = math.cos(rx), math.sin(rx)
    y, z = y * c - z * s, y * s + z * c
    # Y axis.
    c, s = math.cos(ry), math.sin(ry)
    x, z = x * c + z * s, -x * s + z * c
    # Z axis.
    c, s = math.cos(rz), math.sin(rz)
    x, y = x * c - y * s, x * s + y * c
    return (x, y, z)


def sphere_projected(cx, cy, radius):
    """Return the mesh as projected 2D faces: [(points, facing, light)], back faces first."""
    verts, faces = geodesic_sphere(1)
    verts = [rotate(v, math.radians(-18.0), math.radians(24.0), math.radians(-8.0)) for v in verts]
    light = normalize((-0.55, 0.65, 0.55))
    result = []
    for f in faces:
        a, b, c = (verts[i] for i in f)
        n = normalize(((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3))
        facing = n[2]
        diffuse = max(0.0, n[0] * light[0] + n[1] * light[1] + n[2] * light[2])
        # SVG Y axis points down.
        pts = [(cx + v[0] * radius, cy - v[1] * radius) for v in (a, b, c)]
        result.append((pts, facing, diffuse))
    result.sort(key=lambda item: item[1])
    return result


def fmt(v):
    return "{:.2f}".format(v)


def path_from_points(pts):
    return "M" + " L".join("{:s} {:s}".format(fmt(x), fmt(y)) for x, y in pts) + " Z"


# -----------------------------------------------------------------------------
# SVG artwork.


def mesh_orb_svg(cx, cy, radius, stroke, uid, glow=True):
    """Glass mesh orb: translucent facets, bright front edges, faint back edges."""
    parts = []
    parts.append(
        '<radialGradient id="orb_body_{u}" cx="0.38" cy="0.32" r="0.75">'
        '<stop offset="0" stop-color="#ffffff" stop-opacity="0.55"/>'
        '<stop offset="0.45" stop-color="#bfe6ff" stop-opacity="0.22"/>'
        '<stop offset="1" stop-color="#1b4fb0" stop-opacity="0.30"/>'
        '</radialGradient>'.format(u=uid))
    defs = "".join(parts)
    body = []
    if glow:
        body.append('<circle cx="{:s}" cy="{:s}" r="{:s}" fill="url(#orb_body_{u})"/>'.format(
            fmt(cx), fmt(cy), fmt(radius * 1.02), u=uid))
    faces = sphere_projected(cx, cy, radius)
    # Back faces: faint edges only (seen through the glass).
    for pts, facing, diffuse in faces:
        if facing >= 0.0:
            continue
        body.append('<path d="{:s}" fill="none" stroke="#ffffff" stroke-opacity="0.22" '
                    'stroke-width="{:s}" stroke-linejoin="round"/>'.format(path_from_points(pts), fmt(stroke * 0.55)))
    # Front faces: glassy facets with light dependent opacity and bright edges.
    for pts, facing, diffuse in faces:
        if facing < 0.0:
            continue
        opacity = 0.05 + 0.30 * diffuse ** 2.0
        body.append('<path d="{:s}" fill="#ffffff" fill-opacity="{:.3f}" stroke="#ffffff" '
                    'stroke-opacity="{:.3f}" stroke-width="{:s}" stroke-linejoin="round"/>'.format(
                        path_from_points(pts), opacity, 0.55 + 0.4 * facing, fmt(stroke)))
    # Specular highlight.
    body.append('<ellipse cx="{:s}" cy="{:s}" rx="{:s}" ry="{:s}" fill="#ffffff" fill-opacity="0.35" '
                'transform="rotate(-35 {:s} {:s})"/>'.format(
                    fmt(cx - radius * 0.38), fmt(cy - radius * 0.46), fmt(radius * 0.26), fmt(radius * 0.11),
                    fmt(cx - radius * 0.38), fmt(cy - radius * 0.46)))
    return defs, "".join(body)


def icon_svg(size=1024, tile=True):
    """Full color application icon."""
    s = size
    rx = s * 0.225
    defs = [
        '<linearGradient id="tile" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#8fd3ff"/>'
        '<stop offset="0.5" stop-color="#3a8ee6"/>'
        '<stop offset="1" stop-color="#1c3f9e"/>'
        '</linearGradient>',
        '<linearGradient id="gloss" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#ffffff" stop-opacity="0.45"/>'
        '<stop offset="0.5" stop-color="#ffffff" stop-opacity="0.06"/>'
        '<stop offset="1" stop-color="#ffffff" stop-opacity="0"/>'
        '</linearGradient>',
        '<radialGradient id="blob1" cx="0.2" cy="0.85" r="0.6">'
        '<stop offset="0" stop-color="#3fe0d0" stop-opacity="0.55"/>'
        '<stop offset="1" stop-color="#3fe0d0" stop-opacity="0"/>'
        '</radialGradient>',
        '<radialGradient id="blob2" cx="0.9" cy="0.15" r="0.55">'
        '<stop offset="0" stop-color="#b58cff" stop-opacity="0.45"/>'
        '<stop offset="1" stop-color="#b58cff" stop-opacity="0"/>'
        '</radialGradient>',
        '<filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">'
        '<feGaussianBlur stdDeviation="{:s}"/></filter>'.format(fmt(s * 0.02)),
    ]
    orb_defs, orb = mesh_orb_svg(s * 0.5, s * 0.5, s * 0.31, s * 0.012, "icon")
    defs.append(orb_defs)
    body = []
    if tile:
        body.append('<rect x="0" y="0" width="{0}" height="{0}" rx="{1}" fill="url(#tile)"/>'.format(s, fmt(rx)))
        body.append('<rect x="0" y="0" width="{0}" height="{0}" rx="{1}" fill="url(#blob1)"/>'.format(s, fmt(rx)))
        body.append('<rect x="0" y="0" width="{0}" height="{0}" rx="{1}" fill="url(#blob2)"/>'.format(s, fmt(rx)))
        body.append('<rect x="0" y="0" width="{0}" height="{1}" rx="{2}" fill="url(#gloss)"/>'.format(
            s, fmt(s * 0.62), fmt(rx)))
        body.append('<rect x="{0}" y="{0}" width="{1}" height="{1}" rx="{2}" fill="none" stroke="#ffffff" '
                    'stroke-opacity="0.55" stroke-width="{3}"/>'.format(
                        fmt(s * 0.006), fmt(s * 0.988), fmt(rx * 0.99), fmt(s * 0.012)))
    # Soft shadow under the orb.
    body.append('<ellipse cx="{:s}" cy="{:s}" rx="{:s}" ry="{:s}" fill="#0b1f55" fill-opacity="0.35" '
                'filter="url(#shadow)"/>'.format(fmt(s * 0.5), fmt(s * 0.84), fmt(s * 0.25), fmt(s * 0.045)))
    body.append(orb)
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{0}" viewBox="0 0 {0} {0}">'
            '<defs>{1}</defs>{2}</svg>\n'.format(s, "".join(defs), "".join(body)))


def mono_glyph_paths(cx, cy, radius, stroke, color="#fff", back_opacity=0.35):
    """Monochrome mesh orb (for UI / symbolic icons): outline + front edges."""
    faces = sphere_projected(cx, cy, radius)
    parts = []
    edges_front = set()
    for pts, facing, diffuse in faces:
        if facing < 0.0:
            continue
        for i in range(3):
            a = pts[i]
            b = pts[(i + 1) % 3]
            key = tuple(sorted(((round(a[0], 1), round(a[1], 1)), (round(b[0], 1), round(b[1], 1)))))
            edges_front.add(key)
    d = " ".join("M{:s} {:s} L{:s} {:s}".format(fmt(a[0]), fmt(a[1]), fmt(b[0]), fmt(b[1])) for a, b in edges_front)
    parts.append('<circle cx="{:s}" cy="{:s}" r="{:s}" fill="none" stroke="{:s}" stroke-width="{:s}"/>'.format(
        fmt(cx), fmt(cy), fmt(radius), color, fmt(stroke * 1.35)))
    parts.append('<path d="{:s}" fill="none" stroke="{:s}" stroke-width="{:s}" stroke-linecap="round"/>'.format(
        d, color, fmt(stroke)))
    return "".join(parts)


def ui_logo_svg():
    """Replacement for `icons_svg/blender.svg` (same 1800x1500 canvas, white)."""
    w, h = 1800, 1500
    body = mono_glyph_paths(w * 0.5, h * 0.5, 660, 46)
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" viewBox="0 0 {0} {1}">'
            '{2}</svg>\n'.format(w, h, body))


def ui_logo_large_svg():
    """Replacement for `icons_svg/blender_logo_large.svg`: glyph + "GlassMesh" word-mark."""
    w, h = 5236.3638, 1600
    body = mono_glyph_paths(720, 800, 660, 46)
    body += ('<text x="1560" y="1030" font-family="Inter" font-weight="600" font-size="640" '
             'letter-spacing="-10" fill="#fff">GlassMesh</text>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" viewBox="0 0 {0} {1}">'
            '{2}</svg>\n'.format(fmt(w), h, body))


def symbolic_svg():
    """Linux symbolic icon (16x16, dark monochrome)."""
    body = mono_glyph_paths(8, 8, 6.4, 0.9, color="#2e3436")
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
            '{0}</svg>\n'.format(body))


def splash_svg(w=1000, h=500):
    defs = [
        '<linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#7fb4e6"/>'
        '<stop offset="0.55" stop-color="#2f6cb5"/>'
        '<stop offset="1" stop-color="#0f2a57"/>'
        '</linearGradient>',
        '<radialGradient id="b1" cx="0.15" cy="0.2" r="0.55">'
        '<stop offset="0" stop-color="#48e3d6" stop-opacity="0.55"/>'
        '<stop offset="1" stop-color="#48e3d6" stop-opacity="0"/></radialGradient>',
        '<radialGradient id="b2" cx="0.85" cy="0.85" r="0.6">'
        '<stop offset="0" stop-color="#9a7bff" stop-opacity="0.55"/>'
        '<stop offset="1" stop-color="#9a7bff" stop-opacity="0"/></radialGradient>',
        '<radialGradient id="b3" cx="0.72" cy="0.3" r="0.45">'
        '<stop offset="0" stop-color="#ffffff" stop-opacity="0.35"/>'
        '<stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>',
        '<linearGradient id="card" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#ffffff" stop-opacity="0.30"/>'
        '<stop offset="1" stop-color="#ffffff" stop-opacity="0.10"/></linearGradient>',
        '<filter id="soft" x="-30%" y="-30%" width="160%" height="160%">'
        '<feGaussianBlur stdDeviation="18"/></filter>',
    ]
    orb_defs, orb = mesh_orb_svg(w * 0.73, h * 0.53, h * 0.34, 3.2, "splash")
    defs.append(orb_defs)
    body = [
        '<rect width="{0}" height="{1}" fill="url(#bg)"/>'.format(w, h),
        '<rect width="{0}" height="{1}" fill="url(#b1)"/>'.format(w, h),
        '<rect width="{0}" height="{1}" fill="url(#b2)"/>'.format(w, h),
        '<rect width="{0}" height="{1}" fill="url(#b3)"/>'.format(w, h),
        # Floating glass shapes.
        '<rect x="585" y="70" width="170" height="110" rx="34" fill="#ffffff" fill-opacity="0.10" '
        'stroke="#ffffff" stroke-opacity="0.35" stroke-width="1.5" transform="rotate(-12 670 125)"/>',
        '<circle cx="905" cy="395" r="46" fill="#ffffff" fill-opacity="0.10" stroke="#ffffff" '
        'stroke-opacity="0.35" stroke-width="1.5"/>',
        '<ellipse cx="{:s}" cy="{:s}" rx="150" ry="22" fill="#061634" fill-opacity="0.45" filter="url(#soft)"/>'.format(
            fmt(w * 0.73), fmt(h * 0.9)),
        orb,
        # Frosted card with the word-mark.
        '<rect x="44" y="120" width="470" height="250" rx="36" fill="url(#card)" stroke="#ffffff" '
        'stroke-opacity="0.45" stroke-width="1.5"/>',
        '<rect x="46" y="122" width="466" height="70" rx="34" fill="#ffffff" fill-opacity="0.08"/>',
        '<text x="84" y="235" font-family="Inter" font-weight="700" font-size="74" letter-spacing="-2" '
        'fill="#ffffff">GlassMesh</text>',
        '<text x="87" y="277" font-family="Inter" font-weight="500" font-size="21" fill="#ffffff" '
        'fill-opacity="0.92">Liquid Glass for 3D creation</text>',
        '<text x="87" y="322" font-family="Inter" font-weight="400" font-size="13.5" fill="#ffffff" '
        'fill-opacity="0.85">An unofficial fork of Blender. Not affiliated with or</text>',
        '<text x="87" y="341" font-family="Inter" font-weight="400" font-size="13.5" fill="#ffffff" '
        'fill-opacity="0.85">endorsed by the Blender Foundation.</text>',
    ]
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" viewBox="0 0 {0} {1}">'
            '<defs>{2}</defs>{3}</svg>\n'.format(w, h, "".join(defs), "".join(body)))


# -----------------------------------------------------------------------------
# Output helpers.


def write(relpath, data, mode="w"):
    path = os.path.join(ROOT, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, mode) as fh:
        fh.write(data)
    print("Wrote", relpath)
    return path


def rsvg(svg_path, png_path, width, height=None):
    cmd = ["rsvg-convert", "-w", str(width)]
    if height:
        cmd += ["-h", str(height)]
    cmd += ["-o", png_path, svg_path]
    subprocess.check_call(cmd)


def text_to_path(svg_path):
    """Convert text to paths in-place (the UI icon renderer does not support text)."""
    subprocess.check_call([
        "inkscape", svg_path, "--export-text-to-path", "--export-plain-svg",
        "--export-filename=" + svg_path,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def x11_icon_header(png_paths):
    from PIL import Image
    lines = [
        "/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.",
        " * SPDX-FileCopyrightText: 2026 GlassMesh Authors",
        " *",
        " * SPDX-License-Identifier: GPL-2.0-or-later */",
        "",
        "/** \\file",
        " * \\ingroup GHOST",
        " * Icon image data for X11 (the GlassMesh application icon).",
        " *",
        " * Generated by `tools/glassmesh/make_brand_assets.py`, don't edit by hand.",
        " */",
        "",
        "#pragma once",
        "",
        "/* Format: for every size `width, height` followed by ARGB pixels (as `_NET_WM_ICON`). */",
        "static const unsigned long BLENDER_ICONS_WM_X11[] = {",
    ]
    for png in png_paths:
        img = Image.open(png).convert("RGBA")
        w, h = img.size
        lines.append("    /* {:d}x{:d} */".format(w, h))
        lines.append("    {:d}, {:d},".format(w, h))
        values = []
        for r, g, b, a in list(img.convert("RGBA").tobytes("raw", "RGBA")[i:i + 4] for i in range(0, w * h * 4, 4)):
            values.append("0x{:08x}".format((a << 24) | (r << 16) | (g << 8) | b))
        for i in range(0, len(values), 8):
            lines.append("    " + ", ".join(values[i:i + 8]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    from PIL import Image

    tmp = tempfile.mkdtemp(prefix="glassmesh_brand_")
    try:
        # Master logo.
        logo_path = write("release/datafiles/glassmesh/glassmesh_logo.svg", icon_svg())

        # Linux icons.
        write("release/freedesktop/icons/scalable/apps/blender.svg", icon_svg())
        write("release/freedesktop/icons/symbolic/apps/blender-symbolic.svg", symbolic_svg())

        # UI logos (monochrome, text converted to paths).
        write("release/datafiles/icons_svg/blender.svg", ui_logo_svg())
        large = write("release/datafiles/icons_svg/blender_logo_large.svg", ui_logo_large_svg())
        text_to_path(large)

        # Splash.
        splash_svg_path = os.path.join(tmp, "splash.svg")
        with open(splash_svg_path, "w") as fh:
            fh.write(splash_svg())
        splash_png = os.path.join(ROOT, "release/datafiles/glassmesh/splash.png")
        os.makedirs(os.path.dirname(splash_png), exist_ok=True)
        rsvg(splash_svg_path, splash_png, 1000, 500)
        # Strip metadata and optimize.
        Image.open(splash_png).convert("RGB").save(splash_png, optimize=True)
        print("Wrote release/datafiles/glassmesh/splash.png")

        # Raster icons.
        sizes = [16, 24, 32, 48, 64, 128, 256, 512, 1024]
        pngs = {}
        for size in sizes:
            p = os.path.join(tmp, "icon_{:d}.png".format(size))
            rsvg(logo_path, p, size, size)
            pngs[size] = p

        # Windows.
        ico = Image.open(pngs[256])
        ico_path = os.path.join(ROOT, "release/windows/icons/winblender.ico")
        ico.save(ico_path, format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
        print("Wrote release/windows/icons/winblender.ico")

        # macOS.
        icns_path = os.path.join(ROOT, "release/darwin/Blender.app/Contents/Resources/glassmesh_icon.icns")
        subprocess.check_call(["png2icns", icns_path] + [pngs[s] for s in (16, 32, 48, 128, 256, 512, 1024)],
                              stdout=subprocess.DEVNULL)
        print("Wrote release/darwin/Blender.app/Contents/Resources/glassmesh_icon.icns")

        # X11 window icon.
        write("intern/ghost/intern/GHOST_IconX11.hh", x11_icon_header([pngs[s] for s in (16, 32, 48, 64)]))
    finally:
        shutil.rmtree(tmp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
