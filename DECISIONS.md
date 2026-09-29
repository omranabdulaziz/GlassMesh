# GlassMesh – Decisions

Decisions made while building GlassMesh, with the reasoning, so they can be reviewed or
reverted later. The instructions were: the mockups are the main reference, visual and branding
changes only, don't break any functionality, don't ask questions.

## Repository & branch

1. **Base:** the `glassmesh` branch was created from `blender-v5.2-release` (Blender 5.2.2 LTS,
   commit `d13f752e`). The branch already existed in this fork, so upstream didn't need to be
   fetched.
2. **Push target:** all work is pushed to `glassmesh`, as requested. The automation's default
   session branch was not used for this work.
3. **Commits:** small, one topic each, prefixed `GlassMesh:` so they are easy to find or cherry pick
   onto future Blender releases.

## How the glass look is built

4. **Blender can't see the desktop.** A real "glass window over your desktop" would need a
   transparent OS window, which Blender's GPU/windowing code doesn't support on all platforms. So
   GlassMesh draws its own **procedural wallpaper** (a soft gradient with a few blurry color blobs)
   behind the editors. Translucent editors and the gaps between them show it, like the desktop in the
   mockups. Its colors come from the theme's **Editor Border** color, so users can re-tint it.
   *(Second pass: replaced by a photographic wallpaper, see #44 to #47.)*
5. **Real backdrop blur where it matters.** Regions that float over other content get a real
   frosted blur: headers, tool-bars and side-bars over the 3D viewport, menus, popups and tool-tips.
   The window frame-buffer is copied, pre-filtered at quarter resolution and blurred (24-tap
   golden-angle spiral with per-pixel rotation, which also gives the subtle grain). Plain editors
   (Properties, Outliner...) don't need a blur, the wallpaper behind them is already soft.
6. **The blur is only drawn where the region has glass.** The region's own alpha is used as the
   mask (between 0.36 and 0.46 alpha), so soft drop shadows (below ~0.3) don't blur what's behind
   them. The glass colors in the theme are therefore kept above ~0.5 alpha.
7. **Color managed regions stay opaque.** The 3D viewport, image editor, node editor and sequencer
   draw through a color managed `GPUViewport`, so they're always composited opaque. The theme keeps
   their backgrounds opaque (the node editor is dark navy in the mockups anyway).
8. **Only the compositing changes.** Regions draw exactly as before into their own buffers; glass
   only changes how those buffers are combined into the window (`wm_draw_glass.cc`). The translucent
   region clear is limited to the actual background clear (`frame_buffer_clear` / `ED_region_clear`),
   other drawing that uses the background color to cover things stays opaque.
9. **Theme alpha 0 means opaque.** Blender themes store 0 alpha for most editor backgrounds, meaning
   "not set". GlassMesh treats 0 as opaque, so older or third-party themes don't turn invisible.
10. **Widgets:** rim highlight and sheen are done in the existing widget shader, using the spare
    values in its last parameter vector. Nothing changes in the parameter layout, and 0 means off.
    `draw_roundbox_4fv_ex` now uploads all 12 parameter vectors (it uploaded 11), so values from a
    previous widget can't leak into it.
11. **Headers over the viewport** use floating, fully rounded glass pills per button group, instead
    of Blender's attached tabs. The **tool-bar** floats on a glass pill (mockups 1, 2, 5), and
    **side-bar panels** are glass cards sized to their content (the "Pose Options" card in mockup 2).
    *(Second pass: tools are separate tiles, see #50.)*
12. **Tabs** (workspace tabs, properties tabs, preferences navigation) are pills, the active one in
    the accent blue (mockups 1, 2, 4). The preferences navigation uses tab buttons when glass is on,
    because the mockup shows transparent items there. It still uses the original radio buttons when
    glass is off.
13. **Palette:** a blue frosted glass look with accent `#3A8EE6`, near-white text (`#F2F6FB`) and
    darker translucent fields. Editor glass is slightly darker than the mockups, so white text keeps
    good contrast (about 5:1 or better) on any background. The requirement "text must stay fully
    readable" won over matching the lightest mockup tones exactly. *(Second pass: lighter, see
    #48.)*
14. **Checkboxes stay checkboxes.** The mockups show both checkboxes ("Region Overlap", "Cursor") and
    iOS-style switches ("Splash Screen", "Auto IK"), for what are the same kind of Blender setting.
    There's no rule in Blender's layouts to decide which one gets a switch, so switches were not
    added. The checkboxes are rounded and blue like in the mockups. *(Second pass: switches with a
    layout based rule, see #49.)*
15. **Editor gaps:** factory preferences use a border width of 4 (Blender: 2) so the wallpaper shows
    between editors, and corners are 12 px instead of 6 (16 px in the second pass). The border
    width is an ordinary preference (Interface > Editors > Border Width).

## The "Glass" preferences

16. **Where:** Preferences > Interface > Display > **Glass** with *Glass Effect* and *Background
    Blur* (blur can be turned off separately for slow GPUs).
17. **Storage:** a new `UserDef.glass_flag` in existing struct padding. The struct size and
    `.blend`/`userpref.blend` layout are unchanged. The flags are negative (`USER_GLASS_DISABLE`,
    `USER_GLASS_NO_BLUR`), so zero-initialized/old preferences get the glass effect. *(Second
    pass: the wallpaper setting adds a path, see #46.)*
18. **What "off" does:** no wallpaper, no blur, no rims/sheen, opaque regions, classic tabs and
    corner radius. The theme's colors are not changed by the toggle. Users who want the full classic
    look pick the **Blender Dark** theme preset.
19. **Theme presets:** upstream's *Blender Dark* preset is empty because it *is* Blender's default.
    Since the default is now the GlassMesh theme, *Blender Dark* now contains Blender 5.2's real
    default colors. A new *GlassMesh* preset (empty) is the built-in default.
20. **Theme source:** the default theme values are applied to `userdef_default_theme.c` by
    `tools/glassmesh/apply_glassmesh_theme.py`. Some alpha values aren't exposed to Python, so
    Blender's own generator (from a `userpref.blend`) couldn't express them.

## Branding

21. **Name:** GlassMesh, in the window title, splash, About, app menu, macOS menu bar, `--version` /
    `--help`, Windows version info, Linux desktop file and the end-user read-me.
22. **Version number:** GlassMesh uses the version of the Blender it's based on (5.2.2 LTS), and
    says so ("based on Blender 5.2.2"). A separate GlassMesh version number would have meant touching
    version code that add-ons and files depend on.
23. **`--version` output** is now `GlassMesh 5.2.2 LTS`, keeping the version as the second word so
    tools that parse it keep working, followed by "based on Blender" and the disclaimer.
24. **Executable names:** `glassmesh` (Linux), `glassmesh.exe` + `glassmesh-launcher.exe`
    (Windows), `GlassMesh.app` (macOS), via `GLASSMESH_EXE_NAME` / `GLASSMESH_APP_NAME` in CMake. The
    CMake *target* is still `blender`, so the build system, tests and scripts that refer to the
    target keep working.
25. **Files in the repository keep their names** (`release/freedesktop/blender.desktop`,
    `release/darwin/Blender.app/...`, `winblender.ico`...), only their content changes and they're
    *installed* with GlassMesh names. This keeps the diff with upstream small.
26. **Separate identity per platform:** Windows AppUserModelID `glassmesh.5.2`, macOS bundle
    identifier `io.github.omranabdulaziz.glassmesh`, X11 class `GlassMesh` and Wayland app ID
    `glassmesh`. GlassMesh then doesn't get grouped with, or registered as, Blender.
27. **Kept as Blender:** the `.blend` file type identifiers (macOS UTI
    `org.blenderfoundation.blender.file`, Linux MIME `application/x-blender`, Windows ProgID
    `blendfile`), because they name the *file format*. Also kept: the thumbnailer binary name, the
    Python module `bpy`, all internal code and module names, and the Vulkan/OpenXR application name
    reported to drivers. Drivers may apply Blender-specific fixes based on that name.
28. **Config folders:** user preferences, startup files, add-ons, extensions and caches are in
    `~/.config/GlassMesh/5.2` (Linux), `~/Library/Application Support/GlassMesh/5.2` (macOS) and
    `%APPDATA%\GlassMesh\5.2` (Windows), plus a separate `GlassMesh` cache folder (shader caches
    must not be shared with a Blender of the same version). System-wide folders (`/usr/share/blender`,
    `ProgramData\Blender Foundation`) are unchanged: they are read-only for users, and install
    rules depend on them.
29. **Importing old preferences:** the quick-setup "import previous version" only looks at older
    *GlassMesh* folders, it never reads Blender's configuration.
30. **Bug reports:** *Help > Report a Bug* opens a pre-filled issue on the GlassMesh GitHub
    repository, not Blender's tracker. An unofficial fork's bugs shouldn't land with the Blender
    developers.
31. **Logo:** an original, procedural "glass mesh" (a translucent geodesic sphere on a glass
    tile), generated by `tools/glassmesh/make_brand_assets.py`. It's deliberately unlike the Blender
    logo (no orange, no eye/orbit shape). The word mark uses the Inter font (SIL Open Font License),
    converted to paths or pixels.
32. **In-app logo:** the SVG icons `blender.svg` / `blender_logo_large.svg` (app menu and About)
    were replaced by GlassMesh artwork. The icon *identifiers* stayed, so nothing referencing them
    breaks.
33. **macOS icon:** the Liquid Glass asset catalog (`Assets.car`) can only be compiled with Xcode,
    which isn't available here. `Info.plist` therefore no longer sets `CFBundleIconName` and uses
    the new `glassmesh_icon.icns` on all macOS versions.
34. **Splash:** new GlassMesh artwork with the disclaimer printed on it; the version is still drawn
    by the application. Links to Blender's release notes and the Blender Development Fund are kept.
35. **About dialog:** GlassMesh name and logo, "based on Blender" version, the disclaimer, the GPL
    notice and a link to the GitHub source (GPL compliance). Blender's credits, release notes,
    website and donation links are kept, crediting the people who wrote Blender.

## License / GPL

36. No license checks, DRM, activation, telemetry or anything limiting copying was added. All
    existing copyright headers, license files (`COPYING`, `doc/license`) and credits are untouched.
    New files carry `SPDX-FileCopyrightText: 2026 GlassMesh Authors` and `GPL-2.0-or-later` like the
    surrounding Blender code. The regenerated X11 icon header keeps its original copyright line.
37. **Git LFS:** Blender stores many binary files with Git LFS on `projects.blender.org`, and this
    GitHub fork doesn't have those objects. `.lfsconfig` points LFS downloads to Blender's server,
    and GlassMesh's *own* new binary files (splash, macOS icon) are stored directly in git
    (`.gitattributes` exceptions), so everyone can build from this repository.

## GitHub repository & test builds

38. **GitHub page:** `.github/README.md` (Blender's "this is a mirror" notice) was removed, because
    GitHub shows it instead of the root `README.md`. The pull request template now describes
    GlassMesh contributions. `.github/FUNDING.yml` (the *Sponsor* button, pointing to the Blender
    Development Fund) was kept, supporting Blender is in GlassMesh's interest too.
39. **Test build workflow:** GitHub-hosted runners (Ubuntu 24.04 with GCC 14, macOS 15 on Apple
    Silicon, Windows Server 2025 with Visual Studio 2026), using the same steps as the README and
    Blender's pre-compiled libraries. Intel macOS is not built, Blender 5.x doesn't support it.
    Default build options, so the test builds match what users get from `make`. The packages
    leave out the build tools (`makesdna`, `datatoc`...) and import libraries, like Blender's
    official downloads, but keep `blender.pdb` on Windows for readable crash logs.
40. **When it runs:** on pushes to `glassmesh` (not for documentation-only changes), on `v*` tags
    and by hand. The repository is public, so GitHub doesn't charge for the build minutes. A newer
    push cancels a build that's still running. Builds are kept for 14 days.
41. **Not signed:** the test builds are not code-signed (that needs paid Apple/Microsoft
    certificates). The macOS app gets an ad-hoc signature so it can be opened after confirming in
    the system settings. They're not meant to be the Gumroad builds, but can be a starting point.

42. **Windows troubleshooting scripts** (`blender_debug_gpu.cmd`, `blender_factory_startup.cmd`...)
    keep their file names, but start `glassmesh.exe`, say GlassMesh, write their logs to
    `%TEMP%\glassmesh\debug_logs` and point to the GlassMesh issue tracker.

## Local verification environment (not part of GlassMesh)

43. The session's network blocks `projects.blender.org`, so Blender's pre-compiled libraries and
    LFS files couldn't be downloaded. To still build and *look at* the result, a local "lite" build
    was made with Ubuntu system libraries, headers of the exact library versions, and the shared
    libraries from the official Blender 5.2.2 Linux release. Stand-ins for LFS files came from that
    release too (marked `skip-worktree`, never committed). The UI was checked in screenshots under
    Xvfb with Mesa's software OpenGL (llvmpipe). None of this is committed.

## Second pass: following the mockups more closely

The first version was judged "still very Blenderish". The feedback was that faithfulness to
Blender's own interface doesn't matter, only the mockups and Apple's interface do. The second pass
therefore follows the mockups wherever they differ from Blender's look. Functionality is still
unchanged. Where these decisions replace earlier ones, the earlier number is given.

44. **A photographic wallpaper** (replaces the procedural gradient of #4). Much of the mockups' look
    comes from a landscape photo seen through frosted glass. GlassMesh ships an original one: an
    alpine lake, fully procedural (terrain, snow, water, rocks, sky and clouds) and rendered with
    Cycles by `tools/glassmesh/make_wallpaper.py`. No photos and no Apple wallpapers were used,
    Apple's wallpapers are copyrighted. It is embedded in the executable, like the splash.
45. **A frosted window.** The window background and the gaps between editors show a heavily blurred
    copy of the wallpaper, tinted with the theme's *Editor Border* color (its alpha is the
    amount). The blurred copy is made once when the wallpaper is loaded, not every frame. Editors
    are translucent glass cards on top of it. The live blur of #5 is still drawn under regions
    that overlap content, and under menus and popups.
46. **Your own wallpaper.** *Preferences > Interface > Display > Wallpaper* takes any image, for
    example the desktop wallpaper, which gets close to the mockups' see-through-to-the-desktop look
    (#4). It is stored in a new `UserDef.glass_wallpaper` path, so unlike #17 the preferences
    struct grows by 1 KB. `.blend` files are not affected.
47. **A glass 3D viewport** (removed again, see #64). A new theme background type, *Glass*, is the default. In solid and
    wireframe shading the viewport background is transparent, and the wallpaper shows through,
    slightly softened and tinted with the viewport's *Gradient Low* theme color. This gives the
    mountain scenes behind the models in the mockups. Material preview and rendered shading still
    show the world, and images rendered from the viewport (*View > Viewport Render Image*) keep a
    gradient background. It can be changed per viewport (*Shading > Background: World/Viewport*) or
    in the theme. The *Blender Dark* preset sets Blender's single color again.
48. **Lighter glass** (revisits #13, replaced by #65). Editor cards are a translucent blue (65% opacity) over the
    milky frosted wallpaper, and headers have the same color as their card instead of being a
    separate strip. The top bar and status bar are almost clear. Fields and buttons are light
    glass, sliders are blue. It is lighter than version 1, like the mockups, and white text keeps
    a contrast of about 4.5:1 or better.
49. **Switches** (replaces #14). On/off settings in a property split layout, a row with the setting
    name and nothing else, are drawn as iOS style switches at the right end of the row. Checkboxes
    grouped under a heading, like *Tooltips: User Tooltips, Python Tooltips*, stay checkboxes. This
    rule reproduces the Preferences mockup: *Splash Screen* and *Developer Extras* are switches,
    *Show Tooltips* and *Python Tooltips* are checkboxes. Only *Region Overlap* differs, it is a
    switch here and a checkbox in the mockup, which isn't consistent itself. It is only drawing,
    the switch is still Blender's checkbox button.
50. **Tool tiles** (replaces the tool-bar pill of #11). Each tool of a tool-bar is its own rounded
    glass tile, the active one blue, like the tool-bars in the mockups.
51. **Panels are cards.** A panel's header is no longer a separate strip: the header and the
    content share one rounded glass card, closed panels included.
52. **Rounder and more colorful.** Editor corners are 16 px (was 12, #15), the outliner highlights
    rows with blue pills, widgets are a bit rounder, and icons have their full colors (Blender
    shows them at half saturation).
53. **Title bar (macOS and Windows).** Blender colors the system title bar with the top bar's
    theme color. With glass it gets the color of the frosted wallpaper at the top of the window
    instead, so it matches the almost clear top bar.
54. **Apple's system font on macOS.** New preferences on macOS use SF Pro and SF Mono, the
    operating system's own fonts (not shipped with GlassMesh), for the interface, like native Apple
    apps and the mockups. Other platforms keep Blender's Inter font. If the fonts can't be loaded,
    Blender's built-in fonts are used.
55. **Not done: traffic lights inside the window, and File/Edit/Render/Window/Help in the macOS
    menu bar.** The mockups show both. The first needs a window without a title bar, which also
    removes the area used to drag the window around. The second needs a bridge between Blender's
    Python menus and native menus. Neither could be done safely without a Mac to test on.
56. **One application menu in the top bar.** Like the mockups, the top bar shows the app name and
    the workspace tabs. *File, Edit, Render, Window* and *Help* are in one *GlassMesh* menu (the
    logo and the name), which works like the collapsed menus Blender already has. Right-click the
    top bar and enable *Show Menus* to get the separate menus back. Without glass, the top bar is
    unchanged.
57. **Glass navigation buttons.** The zoom, pan, camera and projection buttons of the 3D viewport
    are small glass circles, as in the mockups. Blender only shows a circle while hovering them.
58. **Rounder nodes.** Nodes have a corner radius of 0.32 widget units instead of 0.2.
59. **Glass node and image editors** (reverted, see #64) (revisits the opaque editors of version 1). The node editor
    and the image/UV editor are drawn through a color-managed viewport, which is why version 1 kept
    them opaque. Their backgrounds are now translucent cards like the other editors: the node
    editor clears its background with the theme alpha, and the image editor leaves the area around
    the image transparent (images keep their checkerboard). The sequencer is still opaque.
60. **Outliner rows.** With glass the outliner has no alternating row stripes, which were drawn
    opaque and covered the glass. The active row is an accent blue pill and other selected rows a
    quieter blue pill, both with white text instead of Blender's orange text.
61. **The frosted tint can't be edited in the preferences.** The tint amount of the frosted
    window background is the alpha of the theme's *Editor Border* color. The preferences only
    show that color's RGB, so the amount can only be changed in `apply_glassmesh_theme.py` (or the
    built-in default theme). The color itself can be edited as usual.
62. **A quieter 3D grid.** Blender draws grid lines with less than 10% opacity dashed, which assumes
    its opaque gray grid colors. The glass theme uses translucent white, so with glass the dashes
    follow how far a line has faded instead of its opacity, and the floor grid fades out sooner
    towards the horizon, where it otherwise becomes a dense mesh of lines over the wallpaper.

## Third pass: Liquid Glass, not Mica

Compared side by side with the mockups, the second pass was judged "more like Mica than Liquid
Glass", and making the 3D viewport see-through was the one thing that should not have been
copied: it changes the colors of what is being modeled, which defeats the purpose of the
software. The review also asked for strong edge highlights, deeper shadows and lighter, more vivid
glass.

63. **Why the second pass looked like Mica.** Glass was used as the background of the whole
    application: one wallpaper, blurred once, under everything, with every editor filled with a
    flat translucent navy on top. That is how Mica works. A flat dark color with alpha can only
    darken and gray what is behind it (measured: properties editor (53, 90, 125) against the
    mockup's (85, 141, 172)), and nothing drew the things that make Apple's glass read as glass:
    bright rims along the edges, light caught in the thickness of the pane, refraction at the
    edge, and shadows that separate the layers. Liquid Glass is the reverse: content stays
    opaque and accurate, and glass is a layer of objects floating over it.
64. **Content is opaque** (reverts #47 and #59). The 3D viewport has a neutral gray gradient
    (#454545 to #2e2e2e), the image editor Blender's #303030, and the node canvas an opaque dark
    blue, like the mockups' node editors. The *Glass* viewport background type is removed,
    preferences that used it get a gradient. The landscapes behind the models in the mockups are
    the scenes' worlds, which the viewport shows as usual in material preview and rendered
    shading.
65. **Editors are panes of glass drawn by the window** (replaces the flat tints of #48). The window
    draws every editor as a card with a new shader in three steps: a soft drop shadow on the
    window background; the glass itself (the blurred wallpaper made more vivid and a little
    lighter, bent towards the edge where it also shows a sharper image of what is behind, and
    light along the inside of the edge); then, after the editor, the rounded corners and a
    specular rim, strongest on the edge facing the light (upper left) and again, weaker, on the
    opposite edge, like the reference images. Editor backgrounds are now mostly clear (28% to
    44%) and only tint the glass, the theme still decides their color.
66. **Margins.** The cards are 8 px from the window edges and 4 px below the top bar (Blender uses
    2 px and 1 px), so the window glass shows around them like in the mockups. This only moves
    the editors' edges by a few pixels.
67. **Vivid window glass.** The window background is more saturated (1.6x instead of 1.25x) and
    less tinted (28% instead of 40%), which was the source of the gray, milky look. The cards'
    glass has a light blue milk (16%) instead, so they stay light over the dark lake at the bottom
    of the wallpaper.
68. **Stronger edges on small glass.** Buttons, fields, tool tiles and panels have a rim twice as
    bright as before, nodes get the same rim along their outline, menus and popups are lighter
    (72% instead of 81%) with a more vivid blur behind and a deeper shadow. Panels without a
    header (like the properties editor's context path) drew the editor color fully opaque, with
    glass they use the panel glass instead.

## Fourth pass: polish

The third pass was judged "pretty good". Still to fix: the timeline and the preferences looked
more like Mica than frosted glass, a dark shadow at the top right of the outliner, tool buttons
that looked squashed instead of being squircles, and windows should show what is behind them.

69. **Continuous corners.** Glass widgets and editor cards have superellipse corners (exponent 4),
    like Apple's continuous corners, instead of circular arcs. Pill shaped widgets (a radius of
    half their height) keep their round ends.
70. **Square tool tiles.** Icon-only tools are square tiles (they were 38 x 31 px), like app
    icons. Tools with labels (a wide tool-bar) keep the width of the button.
71. **Frosted, not tinted.** The glass of the cards replaces half of what is behind it with its
    average color, which is how frosted glass scatters light. Big light and dark shapes of the
    wallpaper no longer show through the timeline and other large editors almost unchanged,
    which is what made them look like Mica.
72. **The dark shadow** was Blender's hint that a header has more buttons than fit: a fade to the
    header color at full opacity. With glass it fades into a light frost instead.
73. **Every window is glass.** Windows with a single editor (preferences, file browser, render
    view) had no card and only showed the blurred wallpaper, which is exactly Mica. Their editor is
    now a glass card with the window glass around it, like in the main window.
74. **Windows show what is behind them.** A window in front of its parent GlassMesh window shows a
    frosted copy of it where it covers it, and the wallpaper around it, so the preferences over
    the 3D viewport look gray and over the properties editor look blue. While a window has child
    windows it keeps a small blurred copy of its last frame, and redraws them when it changes. The
    wallpaper is placed on the desktop instead of on each window, so it lies still behind moving
    windows. This needs window positions, which Wayland doesn't give, there the wallpaper covers
    each window as before.
75. **Tab bar.** The workspace tabs of the top bar sit in one glass capsule, like the tab bars of
    all the mockups. Hovered menu and search items are accent blue instead of an opaque gray bar,
    file browser rows are a light veil instead of opaque stripes, and the text editor and Python
    console are 84% opaque so code stays easy to read.
76. **Not done: the real desktop behind GlassMesh.** Showing the actual desktop and other
    applications behind the GlassMesh window needs a transparent window from the operating
    system: `NSVisualEffectView` and a non-opaque Metal layer on macOS, DWM backdrops on Windows,
    an ARGB window and a compositor on Linux. Each needs the renderer to hand over alpha, and a
    mistake gives an invisible or black window. None of it can be run in the environment
    GlassMesh is developed in (no compositor, no Mac or Windows), so it was left for a pass that
    can be tested on each system. The wallpaper stays the stand-in for the desktop.
