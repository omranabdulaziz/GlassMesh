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
12. **Tabs** (workspace tabs, properties tabs, preferences navigation) are pills, the active one in
    the accent blue (mockups 1, 2, 4). The preferences navigation uses tab buttons when glass is on,
    because the mockup shows transparent items there. It still uses the original radio buttons when
    glass is off.
13. **Palette:** a blue frosted glass look with accent `#3A8EE6`, near-white text (`#F2F6FB`) and
    darker translucent fields. Editor glass is slightly darker than the mockups, so white text keeps
    good contrast (about 5:1 or better) on any background. The requirement "text must stay fully
    readable" won over matching the lightest mockup tones exactly.
14. **Checkboxes stay checkboxes.** The mockups show both checkboxes ("Region Overlap", "Cursor") and
    iOS-style switches ("Splash Screen", "Auto IK"), for what are the same kind of Blender setting.
    There's no rule in Blender's layouts to decide which one gets a switch, so switches were not
    added. The checkboxes are rounded and blue like in the mockups. See *Possible follow-ups* in
    `GLASSMESH_NOTES.md`.
15. **Editor gaps:** factory preferences use a border width of 4 (Blender: 2) so the wallpaper shows
    between editors, and corners are 12 px instead of 6. Both are ordinary preferences
    (Interface > Editors > Border Width).

## The "Glass" preferences

16. **Where:** Preferences > Interface > Display > **Glass** with *Glass Effect* and *Background
    Blur* (blur can be turned off separately for slow GPUs).
17. **Storage:** a new `UserDef.glass_flag` in existing struct padding. The struct size and
    `.blend`/`userpref.blend` layout are unchanged. The flags are negative (`USER_GLASS_DISABLE`,
    `USER_GLASS_NO_BLUR`), so zero-initialized/old preferences get the glass effect.
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
    Silicon, Windows Server 2025 with Visual Studio 2022), using the same steps as the README and
    Blender's pre-compiled libraries. Intel macOS is not built, Blender 5.x doesn't support it.
    Default build options, so the test builds match what users get from `make`.
40. **When it runs:** on pushes to `glassmesh` (not for documentation-only changes), on `v*` tags
    and by hand. The repository is public, so GitHub doesn't charge for the build minutes. A newer
    push cancels a build that's still running. Builds are kept for 14 days.
41. **Not signed:** the test builds are not code-signed (that needs paid Apple/Microsoft
    certificates). The macOS app gets an ad-hoc signature so it can be opened after confirming in
    the system settings. They're not meant to be the Gumroad builds, but can be a starting point.

## Local verification environment (not part of GlassMesh)

42. The session's network blocks `projects.blender.org`, so Blender's pre-compiled libraries and
    LFS files couldn't be downloaded. To still build and *look at* the result, a local "lite" build
    was made with Ubuntu system libraries, headers of the exact library versions, and the shared
    libraries from the official Blender 5.2.2 Linux release. Stand-ins for LFS files came from that
    release too (marked `skip-worktree`, never committed). The UI was checked in screenshots under
    Xvfb with Mesa's software OpenGL (llvmpipe). None of this is committed.
