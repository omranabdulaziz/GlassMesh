#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Apply the GlassMesh "Liquid Glass" colors to the built-in default theme.

This edits ``release/datafiles/userdef/userdef_default_theme.c`` in place (the file Blender's own
``tools/utils/blender_theme_as_c.py`` generates), so the GlassMesh default theme can be reproduced
or tweaked from one place::

    python3 tools/glassmesh/apply_glassmesh_theme.py

Only the values listed in ``THEME`` below are changed, everything else keeps the upstream Blender
defaults. The classic Blender look is still available through the "Blender Dark" theme preset.

Colors are ``RRGGBBAA`` hex strings, the alpha channel is what makes editors, headers, panels and
menus translucent ("glass") when the glass effect is enabled in the preferences.
"""

import os
import re
import sys

# -----------------------------------------------------------------------------
# Palette

ACCENT = "3a8ee6ff"        # Selection / active blue.
ACCENT_SOFT = "3a8ee6b3"
TEXT = "f2f6fbff"          # Primary text, near white.
TEXT_DIM = "c5d5e8ff"      # Secondary text.
WHITE = "ffffffff"

GLASS_FIELD = "10203873"   # Dark translucent field (number/text/checkbox backgrounds).
GLASS_FIELD_SEL = "102038a6"
GLASS_BUTTON = "ffffff1f"  # Light translucent button.
GLASS_OUTLINE = "ffffff24" # Soft white edge.
GLASS_MENU = "1f3553c8"    # Menus and popups (~78% opaque, blurred behind).
GLASS_TOOLTIP = "18293fdc"

EDITOR_LIGHT = "2a4c7a8c"  # Light glass editors: properties, outliner, file browser...
EDITOR_DARK = "162640d9"   # Dark glass editors: timelines, graph, text...
EDITOR_OPAQUE = "131f33ff" # Editors drawn through a color managed viewport can't be translucent.
HEADER = "22406a99"        # Headers (translucent).
HEADER_OVERLAP = "2a4a7899"

# -----------------------------------------------------------------------------
# Theme values, keyed by their path inside `U_theme_default`.


def widget(outline, outline_sel, inner, inner_sel, item, text, text_sel, roundness):
    return {
        "outline": outline,
        "outline_sel": outline_sel,
        "inner": inner,
        "inner_sel": inner_sel,
        "item": item,
        "text": text,
        "text_sel": text_sel,
        "roundness": roundness,
    }


THEME = {
    "tui": {
        "wcol_regular": widget(GLASS_OUTLINE, ACCENT, GLASS_BUTTON, ACCENT, "ffffffcc", TEXT, WHITE, 0.35),
        "wcol_tool": widget(GLASS_OUTLINE, ACCENT, GLASS_BUTTON, ACCENT, WHITE, TEXT, WHITE, 0.35),
        "wcol_toolbar_item": widget("ffffff00", "ffffff40", "ffffff00", ACCENT, "ffffffe6", TEXT, WHITE, 0.35),
        "wcol_text": widget("ffffff1f", ACCENT, "0a1a3073", "0a1a30bf", "ffffff40", TEXT, WHITE, 0.35),
        "wcol_radio": widget("ffffff1f", "ffffff33", GLASS_FIELD, ACCENT, WHITE, TEXT, WHITE, 0.35),
        "wcol_option": widget("ffffff33", ACCENT, GLASS_FIELD, ACCENT, WHITE, TEXT, WHITE, 0.3),
        "wcol_toggle": widget("ffffff1f", "ffffff33", "ffffff1a", ACCENT, WHITE, TEXT, WHITE, 0.35),
        "wcol_num": widget("ffffff1f", ACCENT, GLASS_FIELD, GLASS_FIELD_SEL, "ffffff99", TEXT, WHITE, 0.35),
        "wcol_numslider": widget("ffffff1f", ACCENT, GLASS_FIELD, GLASS_FIELD_SEL, ACCENT, TEXT, WHITE, 0.35),
        "wcol_tab": widget("ffffff00", "ffffff00", "ffffff00", ACCENT, "ffffff00", TEXT, WHITE, 0.5),
        "wcol_menu": widget("ffffff1f", "ffffff33", "10203866", ACCENT_SOFT, "ffffffcc", TEXT, WHITE, 0.35),
        "wcol_pulldown": widget("ffffff00", "ffffff00", "ffffff00", "ffffff26", "ffffff8f", TEXT, WHITE, 0.35),
        "wcol_menu_back": widget("ffffff38", "ffffff38", GLASS_MENU, ACCENT, "ffffffd9", TEXT_DIM, WHITE, 0.45),
        "wcol_menu_item": widget("ffffff00", "ffffff00", "00000000", ACCENT, "ffffff8f", TEXT, WHITE, 0.35),
        "wcol_tooltip": widget("ffffff38", "ffffff38", GLASS_TOOLTIP, ACCENT, "ffffffd9", TEXT, WHITE, 0.45),
        "wcol_box": widget("ffffff1a", "ffffff1a", "ffffff0a", "ffffff1a", "10203880", TEXT, WHITE, 0.35),
        "wcol_scroll": widget("ffffff00", "ffffff00", "ffffff00", WHITE, "ffffff4d", TEXT, WHITE, 0.5),
        "wcol_progress": widget("ffffff1f", "ffffff1f", GLASS_FIELD, ACCENT, ACCENT, TEXT, WHITE, 0.35),
        "wcol_list_item": widget("ffffff00", "ffffff00", "ffffff00", "3a8ee6e6", "ffffff33", TEXT, WHITE, 0.35),
        "wcol_pie_menu": widget("ffffff38", "ffffff38", "1f3553d9", ACCENT, "ffffff66", TEXT, WHITE, 0.5),
        "link": "7fbfffff",
        "widget_emboss": "0000001a",
        "menu_shadow_fac": 0.3,
        "menu_shadow_width": 14,
        # Also the base color of the glass "wallpaper" (see `interface_glass.cc`).
        "editor_border": "2e5c8cff",
        "editor_outline": "ffffff1c",
        "editor_outline_active": "ffffff40",
        "icon_saturation": 0.65,
        "widget_text_cursor": "7fbfffff",
        "panel_roundness": 1.0,
        "panel_header": "ffffff14",
        "panel_back": "ffffff0a",
        "panel_sub_back": "0000001a",
        "panel_outline": "ffffff1a",
        "panel_title": WHITE,
        "panel_text": TEXT,
        "panel_active": ACCENT,
    },
    "regions": {
        "asset_shelf": {"back": "1c3150c8", "header_back": "1c3150c8"},
        "channels": {"back": "14233ad9", "text": TEXT_DIM},
        "scrubbing": {"back": "16263fe0", "text": TEXT_DIM},
        # Used for the glass base of panels in overlapping side-bars, keep the region itself clear.
        "sidebars": {"back": "2a4a7400", "tab_back": "1f3a6199"},
    },
    "common": {
        "anim": {"playhead": ACCENT},
    },
    "space_properties": {"back": EDITOR_LIGHT, "header": HEADER, "text": TEXT, "title": WHITE, "match": ACCENT},
    "space_view3d": {
        "back": "4a5e78ff",
        "back_grad": "1c2533ff",
        "header": HEADER_OVERLAP,
        "text": TEXT,
        "title": WHITE,
        "grid": "5a6a7e80",
        "grid_major": "5d6f86ff",
    },
    "space_file": {"back": EDITOR_LIGHT, "header": HEADER, "hilite": ACCENT, "row_alternate": "ffffff05"},
    "space_graph": {"back": EDITOR_DARK, "header": HEADER, "text": TEXT_DIM, "grid": "0c1524ff"},
    "space_info": {"back": EDITOR_DARK, "header": HEADER, "info_selected": "2f6fb8ff"},
    "space_action": {"back": EDITOR_DARK, "header": HEADER, "text": TEXT_DIM, "grid": "0c1524ff", "anim_active": "2f6fb866"},
    "space_nla": {"back": EDITOR_DARK, "header": HEADER, "text": TEXT_DIM, "grid": "0e1a2cff"},
    "space_sequencer": {"back": EDITOR_OPAQUE, "header": HEADER, "text": TEXT_DIM, "grid": "0e1a2cff"},
    "space_image": {"back": "1a2536ff", "header": HEADER},
    "space_text": {"back": EDITOR_DARK, "header": HEADER, "shade2": "2a4466e6", "line_numbers": "7f95b0ff", "grid": "0e1726ff"},
    "space_outliner": {
        "back": EDITOR_LIGHT,
        "header": HEADER,
        "active": ACCENT,
        "selected_highlight": "2f6fb8ff",
        "row_alternate": "ffffff05",
        "text": TEXT,
    },
    "space_node": {
        "back": EDITOR_OPAQUE,
        "header": HEADER,
        "grid": "26385200",
        "syntaxl": "1f2f48f0",   # Node backdrop.
        "node_outline": "ffffff33",
        "syntaxn": "c83a55ff",   # Input nodes.
        "syntaxv": "2f86c8ff",   # Converter nodes.
        "syntaxb": "b8a62eff",   # Color nodes.
        "syntaxs": "8a4e4eff",   # Matte nodes.
        "syntaxd": "4a8a8cff",   # Distort nodes.
        "syntaxc": "3b6e3aff",   # Group nodes.
        "movie": "0f1a2ccc",     # Frame nodes.
        "nodeclass_output": "c2334dff",
        "nodeclass_filter": "6b3fa0ff",
        "nodeclass_vector": "3f5fd0ff",
        "nodeclass_texture": "d9822bff",
        "nodeclass_shader": "2e9e5eff",
        "nodeclass_script": "2a7a7aff",
        "nodeclass_geometry": "1d9a80ff",
        "nodeclass_attribute": "3a4a9aff",
    },
    "space_preferences": {"back": EDITOR_LIGHT, "header": HEADER, "match": ACCENT},
    "space_console": {"back": EDITOR_DARK, "header": HEADER},
    "space_clip": {"back": EDITOR_DARK, "header": HEADER},
    "space_topbar": {"back": "2a4a7880", "header": "2a4a7880"},
    "space_statusbar": {"back": "22406a99", "header": "22406a99", "text": TEXT_DIM, "header_text": TEXT_DIM},
    "space_spreadsheet": {"back": EDITOR_DARK, "header": HEADER, "selected_highlight": "2f6fb8ff"},
}

# Viewport background: a soft vertical gradient.
EXTRA_FIELDS = {
    "space_view3d": [(".back_grad", ".background_type = 1,")],
}

# -----------------------------------------------------------------------------
# C source patching.

THEME_C = os.path.join(
    os.path.dirname(__file__), "..", "..", "release", "datafiles", "userdef", "userdef_default_theme.c",
)

HEADER_NOTE = (
    " * GlassMesh: the colors were adjusted by 'tools/glassmesh/apply_glassmesh_theme.py',\n"
    " * edit the values there and re-run it.\n"
)


def format_value(value):
    if isinstance(value, str):
        return "RGBA(0x{:s})".format(value)
    if isinstance(value, float):
        text = repr(value)
        return text + "f"
    return str(value)


def flatten(tree, prefix=()):
    for key, value in tree.items():
        if isinstance(value, dict):
            yield from flatten(value, prefix + (key,))
        else:
            yield prefix + (key,), value


def main():
    with open(THEME_C, "r", encoding="utf-8") as fh:
        lines = fh.read().split("\n")

    wanted = {path: value for path, value in flatten(THEME)}
    found = set()

    re_open = re.compile(r"^(\s*)\.([a-z0-9_]+) = \{$")
    re_value = re.compile(r"^(\s*)\.([a-z0-9_]+) = (.*),$")
    stack = []
    out = []
    for line in lines:
        m_open = re_open.match(line)
        if m_open:
            stack.append(m_open.group(2))
            out.append(line)
            continue
        if re.match(r"^\s*\},?$", line):
            if stack:
                stack.pop()
            out.append(line)
            continue
        m_value = re_value.match(line)
        if m_value and stack:
            path = tuple(stack) + (m_value.group(2),)
            if path in wanted:
                line = "{:s}.{:s} = {:s},".format(m_value.group(1), m_value.group(2), format_value(wanted[path]))
                found.add(path)
            out.append(line)
            # Insert extra fields after an anchor field.
            for space, extras in EXTRA_FIELDS.items():
                if tuple(stack) == (space,):
                    for anchor, extra in extras:
                        if "." + m_value.group(2) == anchor and extra.strip() not in "\n".join(lines):
                            out.append(m_value.group(1) + extra)
            continue
        out.append(line)

    missing = sorted(set(wanted) - found)
    if missing:
        print("Missing theme values (not present in the C file):")
        for path in missing:
            print("  " + ".".join(path))
        return 1

    text = "\n".join(out)
    if "GlassMesh:" not in text:
        text = text.replace(" * Do not hand edit this file!\n", " * Do not hand edit this file!\n *\n" + HEADER_NOTE, 1)

    with open(THEME_C, "w", encoding="utf-8") as fh:
        fh.write(text)
    print("Updated", os.path.normpath(THEME_C), "({:d} values)".format(len(found)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
