<!--
Keep this document short & concise,
linking to external resources instead of including content in-line.
See 'release/text/readme.html' for the end user read-me.
-->

GlassMesh
=========

**GlassMesh is an unofficial fork of Blender and is not affiliated with or endorsed by the
Blender Foundation.**

GlassMesh is [Blender](https://www.blender.org) 5.2 LTS with a translucent, Apple
"Liquid Glass" inspired interface: frosted glass editors, headers, panels and menus, a soft
background blur behind toolbars, headers and popups, larger rounded corners, bright rim
highlights and soft drop shadows. Everything else is Blender: the same tools, the same Python
API, the same add-ons and full `.blend` file compatibility.

- The glass effect can be switched off in **Preferences > Interface > Display > Glass**
  (the background blur can be switched off separately, for slower graphics cards).
- GlassMesh keeps its settings in its own `GlassMesh` configuration folder, it never reads or
  overwrites the preferences of a regular Blender installation.
- The classic look is one click away: pick the **Blender Dark** theme preset and turn the glass
  effect off.

Screenshots
-----------

*Screenshots coming soon.*

<!-- Add screenshots here, for example:
![GlassMesh - Shading workspace](docs/screenshots/shading.png "GlassMesh")
-->

Get GlassMesh
-------------

**Buy ready-to-use builds on Gumroad: [GUMROAD LINK]**

The paid builds are a convenience: GlassMesh is free software, licensed under the GNU General
Public License, like Blender. The complete source code is in this repository, and you can build it
yourself for free by following the instructions below. The builds contain no license checks,
activation or copy protection of any kind, you may copy and share them under the terms of the GPL.

Build GlassMesh yourself (free)
-------------------------------

GlassMesh builds exactly like Blender. These steps follow Blender's
[official build instructions](https://developer.blender.org/docs/handbook/building_blender/),
which have more details and troubleshooting tips.

The GlassMesh sources are on the `glassmesh` branch. The first build downloads Blender's
pre-compiled libraries (several GB) and takes a while, later builds are much faster.

> **Note on Git LFS:** some files (the default startup file, icons, studio lights...) are stored
> with Git LFS. This repository is configured (`.lfsconfig`) to download them from Blender's own
> server, `projects.blender.org`, since they are unchanged from Blender. `make update` takes care
> of this for you.

### Linux

1. Install the build tools. On Ubuntu 24.04 or newer, for example:

   ```sh
   sudo apt update
   sudo apt install build-essential git git-lfs cmake gcc-14 g++-14 \
       libx11-dev libxxf86vm-dev libxcursor-dev libxi-dev libxrandr-dev libxinerama-dev \
       libegl-dev libwayland-dev wayland-protocols libxkbcommon-dev libdbus-1-dev \
       linux-libc-dev libdecor-0-dev
   ```

   Blender 5.2 requires GCC 14 or newer (or a recent Clang). On other distributions see
   [Blender's Linux build page](https://developer.blender.org/docs/handbook/building_blender/linux/).

2. Get the source code:

   ```sh
   mkdir ~/glassmesh-git && cd ~/glassmesh-git
   git lfs install
   git clone --branch glassmesh https://github.com/omranabdulaziz/GlassMesh.git
   cd GlassMesh
   ```

3. Download the pre-compiled libraries and LFS files, then build:

   ```sh
   make update
   CC=gcc-14 CXX=g++-14 make
   ```

4. Run it:

   ```sh
   ../build_linux/bin/glassmesh
   ```

### macOS

1. Install [Xcode](https://developer.apple.com/xcode/) (or at least the Command Line Tools:
   `xcode-select --install`), and [Homebrew](https://brew.sh), then:

   ```sh
   brew install cmake git git-lfs
   git lfs install
   ```

2. Get the source code:

   ```sh
   mkdir ~/glassmesh-git && cd ~/glassmesh-git
   git clone --branch glassmesh https://github.com/omranabdulaziz/GlassMesh.git
   cd GlassMesh
   ```

3. Download the pre-compiled libraries and LFS files, then build:

   ```sh
   make update
   make
   ```

4. Run it: open `../build_darwin/bin/GlassMesh.app`.

   Builds you make yourself are not signed by Apple. If macOS refuses to open the app, right-click
   it and choose **Open**, or allow it in **System Settings > Privacy & Security**.

### Windows

1. Install:
   - [Visual Studio 2022](https://visualstudio.microsoft.com/) (the free Community edition works)
     with the **Desktop development with C++** workload.
   - [Git for Windows](https://git-scm.com/download/win) (it includes Git LFS).
   - [CMake](https://cmake.org/download/) (add it to the `PATH` when asked).

2. Open a **Command Prompt** and get the source code:

   ```bat
   mkdir C:\glassmesh-git
   cd C:\glassmesh-git
   git lfs install
   git clone --branch glassmesh https://github.com/omranabdulaziz/GlassMesh.git
   cd GlassMesh
   ```

3. Download the pre-compiled libraries and LFS files, then build:

   ```bat
   make update
   make
   ```

4. Run it: `C:\glassmesh-git\build_windows_x64_vc17_Release\bin\Release\glassmesh.exe`.

### Updating

To update an existing checkout later, run `make update` followed by `make` again.

Test builds (GitHub Actions)
----------------------------

The [GlassMesh Build](.github/workflows/glassmesh_build.yml) workflow builds GlassMesh for
Linux (x64), macOS (Apple Silicon) and Windows (x64) on GitHub's servers. It runs on every push
to the `glassmesh` branch and on `v*` tags, and can be started by hand from the **Actions** tab
(*GlassMesh Build > Run workflow*). A build takes a few hours.

The builds are attached to the workflow run as **Artifacts** (kept for 14 days). They are test
builds and are not code-signed: on macOS, confirm opening the app in
**System Settings > Privacy & Security**, on Windows click **More info > Run anyway**.

Project status & documentation
------------------------------

- [GLASSMESH_NOTES.md](GLASSMESH_NOTES.md): what GlassMesh changes, where, known limitations and
  what to test.
- [DECISIONS.md](DECISIONS.md): design and implementation decisions.
- Tools used to generate the GlassMesh theme and artwork are in [tools/glassmesh](tools/glassmesh).

About Blender
-------------

GlassMesh is based on Blender, the free and open source 3D creation suite by the Blender
Foundation and the Blender community. It supports the entirety of the 3D pipeline—modeling,
rigging, animation, simulation, rendering, compositing, motion tracking and video editing.
All credit for Blender goes to its authors, please consider
[supporting Blender's development](https://fund.blender.org).

- [Main Website](https://www.blender.org)
- [Reference Manual](https://docs.blender.org/manual/en/latest/index.html) (it applies to
  GlassMesh too)
- [User Community](https://www.blender.org/community/)

Please report GlassMesh specific problems to
[this repository](https://github.com/omranabdulaziz/GlassMesh/issues), not to Blender.

Development:

- [Build Instructions](https://developer.blender.org/docs/handbook/building_blender/)
- [Code Review & Bug Tracker](https://projects.blender.org)
- [Developer Forum](https://devtalk.blender.org)
- [Developer Documentation](https://developer.blender.org/docs/)

License
-------

Blender, and GlassMesh with it, is licensed as a whole under the GNU General Public License,
Version 3. Individual files may have a different but compatible license.

See [blender.org/about/license](https://www.blender.org/about/license) for details, the license
texts are in [COPYING](COPYING) and [doc/license](doc/license).

"Blender" is a trademark of the Blender Foundation. GlassMesh is not a Blender product and is not
affiliated with or endorsed by the Blender Foundation.
