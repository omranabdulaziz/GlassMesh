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
ACCENT_FILL = "4b9df06b"   # Translucent blue of slider fields.
TEXT = "ffffffff"          # Primary text, white.
TEXT_DIM = "d3e1f2ff"      # Secondary text.
WHITE = "ffffffff"

GLASS_FIELD = "ffffff1c"   # Light translucent field (number/text/checkbox backgrounds).
GLASS_FIELD_SEL = "ffffff30"
GLASS_BUTTON = "ffffff26"  # Light translucent button.
GLASS_OUTLINE = "ffffff33" # Soft white edge.
GLASS_MENU = "2a5288b8"    # Menus and popups (~72% opaque, blurred and made vivid behind).
GLASS_TOOLTIP = "1d3a60e0"

# Editors are the content of glass "cards": panes of glass the window draws over the frosted window
# background (see `glass_card_draw()`). Their backgrounds only tint the glass a little. Content
# where colors matter (3D viewport, image editor, node canvas, sequencer) is opaque and neutral.
EDITOR_LIGHT = "1c3d6647"  # Properties, outliner, file browser, preferences...
EDITOR_DARK = "142c4c70"   # Timelines, graph, text...
EDITOR_OPAQUE = "15243bff" # Editors that are always drawn opaque (the sequencer).
HEADER = EDITOR_LIGHT      # Headers are part of their editor's card.
HEADER_DARK = EDITOR_DARK
HEADER_OVERLAP = "2a4a7899"
BAR = "0c22401f"           # Top bar and status bar: almost clear, on the window's glass.

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
        "wcol_regular": widget(GLASS_OUTLINE, ACCENT, GLASS_BUTTON, ACCENT, "ffffffcc", TEXT, WHITE, 0.4),
        "wcol_tool": widget(GLASS_OUTLINE, ACCENT, GLASS_BUTTON, ACCENT, WHITE, TEXT, WHITE, 0.4),
        # Every tool is its own small glass tile (see `widget_toolbar_item_glass`).
        "wcol_toolbar_item": widget("ffffff4d", "ffffff66", "9ebde66b", ACCENT, "ffffffe6", TEXT, WHITE, 0.4),
        "wcol_text": widget(GLASS_OUTLINE, ACCENT, GLASS_FIELD, "0a1a30bf", "ffffff40", TEXT, WHITE, 0.5),
        "wcol_radio": widget(GLASS_OUTLINE, "ffffff4d", GLASS_FIELD, ACCENT, WHITE, TEXT, WHITE, 0.4),
        "wcol_option": widget("ffffff4d", ACCENT, GLASS_FIELD, ACCENT, WHITE, TEXT, WHITE, 0.35),
        "wcol_toggle": widget(GLASS_OUTLINE, "ffffff4d", GLASS_BUTTON, ACCENT, WHITE, TEXT, WHITE, 0.4),
        "wcol_num": widget(GLASS_OUTLINE, ACCENT, GLASS_FIELD, GLASS_FIELD_SEL, "ffffff99", TEXT, WHITE, 0.4),
        "wcol_numslider": widget(GLASS_OUTLINE, ACCENT, ACCENT_FILL, "4b9df099", ACCENT, TEXT, WHITE, 0.4),
        "wcol_tab": widget("ffffff00", "ffffff00", "ffffff00", ACCENT, "ffffff00", TEXT, WHITE, 0.5),
        "wcol_menu": widget(GLASS_OUTLINE, "ffffff4d", GLASS_BUTTON, ACCENT_SOFT, "ffffffcc", TEXT, WHITE, 0.45),
        "wcol_pulldown": widget("ffffff00", "ffffff00", "ffffff00", "ffffff26", "ffffff8f", TEXT, WHITE, 0.4),
        "wcol_menu_back": widget("ffffff40", "ffffff40", GLASS_MENU, ACCENT, "ffffffd9", TEXT_DIM, WHITE, 0.5),
        "wcol_menu_item": widget("ffffff00", "ffffff00", "00000000", ACCENT, "ffffff8f", TEXT, WHITE, 0.4),
        "wcol_tooltip": widget("ffffff40", "ffffff40", GLASS_TOOLTIP, ACCENT, "ffffffd9", TEXT, WHITE, 0.5),
        "wcol_box": widget("ffffff1f", "ffffff1f", "ffffff0d", "ffffff1f", "10203880", TEXT, WHITE, 0.4),
        "wcol_scroll": widget("ffffff00", "ffffff00", "ffffff00", WHITE, "ffffff4d", TEXT, WHITE, 0.5),
        "wcol_progress": widget(GLASS_OUTLINE, GLASS_OUTLINE, GLASS_FIELD, ACCENT, ACCENT, TEXT, WHITE, 0.4),
        "wcol_list_item": widget("ffffff00", "ffffff00", "ffffff00", ACCENT, "ffffff33", TEXT, WHITE, 0.4),
        "wcol_pie_menu": widget("ffffff40", "ffffff40", "274a74e0", ACCENT, "ffffff66", TEXT, WHITE, 0.5),
        "link": "7fbfffff",
        "widget_emboss": "00000014",
        "menu_shadow_fac": 0.55,
        "menu_shadow_width": 22,
        # The frosted window background (the blurred wallpaper) is tinted with this color, its
        # alpha is the amount (see `interface_glass.cc`). It also shows in the gaps between editors.
        "editor_border": "c9ddf447",
        "editor_outline": "ffffff38",
        "editor_outline_active": "ffffff66",
        "icon_saturation": 1.0,
        "widget_text_cursor": "7fbfffff",
        "panel_roundness": 1.0,
        "panel_header": "ffffff00",
        "panel_back": "ffffff0f",
        "panel_sub_back": "0000000f",
        "panel_outline": "ffffff21",
        "panel_title": WHITE,
        "panel_text": TEXT,
        "panel_active": ACCENT,
    },
    "regions": {
        "asset_shelf": {"back": "1c3d66b8", "header_back": "1c3d66b8"},
        "channels": {"back": "142c4c85", "text": TEXT_DIM, "text_selected": WHITE},
        "scrubbing": {"back": "142c4c99", "text": TEXT_DIM},
        # Used for the glass base of panels in overlapping side-bars, keep the region itself clear.
        "sidebars": {"back": "2a4a7400", "tab_back": "1f3a6152"},
    },
    "common": {
        # Channel rows are light glass bands instead of dark gray ones. Some are drawn opaque in the
        # channel list, so they are blue rather than translucent white.
        "anim": {
            "playhead": ACCENT,
            "channels": "3a8ee659",
            "channels_sub": "2b4f7c59",
            "channel": "ffffff0a",
            "channel_selected": "3a8ee633",
            "channel_group": "ffffff14",
            "channel_group_active": "3a8ee640",
        },
    },
    "space_properties": {
        "back": EDITOR_LIGHT,
        "header": HEADER,
        "text": TEXT,
        "title": WHITE,
        "match": ACCENT,
    },
    "space_view3d": {
        # The viewport is content: an opaque, neutral gray gradient, so colors are seen as they are.
        "back": "454545ff",
        "back_grad": "2e2e2eff",
        "background_type": 1,
        "header": HEADER_OVERLAP,
        "text": TEXT,
        "title": WHITE,
        "grid": "54545480",
        "grid_major": "545454ff",
    },
    "space_file": {"back": EDITOR_LIGHT, "header": HEADER, "hilite": ACCENT, "row_alternate": "ffffff05"},
    # Grid lines are drawn opaque: a slightly lighter blue than the editor instead of near black.
    "space_graph": {"back": EDITOR_DARK, "header": HEADER_DARK, "text": TEXT_DIM, "grid": "34557fff"},
    "space_info": {"back": EDITOR_DARK, "header": HEADER_DARK, "info_selected": "2f6fb8ff"},
    "space_action": {"back": EDITOR_DARK, "header": HEADER_DARK, "text": TEXT_DIM, "grid": "34557fff", "anim_active": "2f6fb866"},
    "space_nla": {"back": EDITOR_DARK, "header": HEADER_DARK, "text": TEXT_DIM, "grid": "34557fff"},
    "space_sequencer": {"back": EDITOR_OPAQUE, "header": HEADER_DARK, "text": TEXT_DIM, "grid": "0e1a2cff"},
    "space_image": {"back": "303030ff", "header": HEADER_DARK},
    "space_text": {"back": EDITOR_DARK, "header": HEADER_DARK, "shade2": "2a4466e6", "line_numbers": "7f95b0ff", "grid": "0e1726ff"},
    "space_outliner": {
        "back": EDITOR_LIGHT,
        "header": HEADER,
        # Rows are highlighted with pills: the active one in the accent color, other selected ones
        # in a quieter blue, with white text instead of the classic orange.
        "active": ACCENT,
        "selected_highlight": "2e67a6ff",
        "selected_object": "dbe9ffff",
        "active_object": WHITE,
        "row_alternate": "ffffff04",
        "text": TEXT,
    },
    "space_node": {
        # The node canvas is an opaque dark card like in the mockups, nodes sit on it.
        "back": "182937ff",
        "header": HEADER_DARK,
        "grid": "ffffff14",
        "syntaxl": "1d3246f2",   # Node backdrop.
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
    "space_console": {"back": EDITOR_DARK, "header": HEADER_DARK},
    "space_clip": {"back": EDITOR_DARK, "header": HEADER_DARK},
    "space_topbar": {"back": BAR, "header": BAR},
    "space_statusbar": {"back": BAR, "header": BAR, "text": TEXT_DIM, "header_text": TEXT_DIM},
    "space_spreadsheet": {"back": EDITOR_DARK, "header": HEADER_DARK, "selected_highlight": "3a8ee6d9"},
}

# Fields that upstream's default theme doesn't list: inserted after an anchor field, then set from
# `THEME` like any other value.
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
                        field = extra.split("=")[0].strip()
                        if "." + m_value.group(2) == anchor and field + " =" not in "\n".join(lines):
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
