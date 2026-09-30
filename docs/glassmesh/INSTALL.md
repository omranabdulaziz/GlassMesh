Installing GlassMesh
====================

GlassMesh comes as one file per system:

| System | File | Needs |
|---|---|---|
| macOS | `glassmesh-5.2.2-macos-arm64-….zip` | a Mac with Apple Silicon (M1 or newer), macOS 11.2 or newer |
| Windows | `glassmesh-5.2.2-windows-x64-….zip` | Windows 10 or 11, 64-bit |
| Linux | `glassmesh-5.2.2-linux-x64-….tar.xz` | a 64-bit distribution from 2024 or newer (glibc 2.39: Ubuntu 24.04, Debian 13, Fedora 40, …) |

All of them need a graphics card that supports OpenGL 4.3 (Metal on macOS), like Blender 5.2.

GlassMesh keeps its settings in its own `GlassMesh` folder, so it can be installed next to Blender
and never touches Blender's settings.

macOS
-----

1. Double-click the `.zip` file. It unpacks to `GlassMesh.app`.
2. Drag `GlassMesh.app` into the **Applications** folder.
3. The first time, open it like this (the app isn't notarized by Apple, so macOS asks once):
   - Double-click `GlassMesh.app`. macOS says it can't check it for malicious software: click
     **Done** (or **Cancel**).
   - Open **System Settings > Privacy & Security**, scroll down to *"GlassMesh" was blocked…* and
     click **Open Anyway**, then confirm with your password.

   From then on it opens normally. (On macOS 14 and older, right-clicking the app and choosing
   **Open** works too.)

Windows
-------

1. Right-click the `.zip` file and choose **Extract All…**. Pick a place for it, for example
   `C:\Program Files\GlassMesh` or a folder in your user folder.
2. Open the extracted folder and double-click `glassmesh.exe`. To have it in the Start menu,
   right-click `glassmesh.exe` and choose **Pin to Start**.
3. The first time, Windows may show *"Windows protected your PC"* (the app isn't code-signed):
   click **More info**, then **Run anyway**.

Linux
-----

1. Unpack it where you want to keep it:

   ```sh
   tar -xf glassmesh-5.2.2-linux-x64-*.tar.xz
   ```

2. Run it:

   ```sh
   ./glassmesh-5.2.2-linux-x64-*/glassmesh
   ```

3. Optional, a menu entry: copy `glassmesh.desktop` from the folder to `~/.local/share/applications/`
   and change its `Exec=` line to the full path of `glassmesh`.

Uninstalling
------------

Delete the app (macOS) or its folder (Windows, Linux). The settings are in
`~/Library/Application Support/GlassMesh` (macOS), `%APPDATA%\GlassMesh` (Windows) or
`~/.config/GlassMesh` (Linux), delete that folder too to remove them.

License
-------

GlassMesh is an unofficial fork of Blender and is not affiliated with or endorsed by the Blender
Foundation. Like Blender it is free software under the GNU General Public License (GPL), version 3
or later: you may use it for anything, and share or change it under the same license. The source
code is at <https://github.com/omranabdulaziz/GlassMesh>.

---

For the maintainers: getting the builds from GitHub
---------------------------------------------------

The [GlassMesh Build](../../.github/workflows/glassmesh_build.yml) workflow uploads the builds
encrypted, since anyone signed in to GitHub can download the artifacts of a public repository.

1. Open the workflow run in the **Actions** tab. Its **Artifacts** section (at the bottom of the
   run's summary) has one download per system, they are kept for 7 days.
2. Each download is a `.zip` holding `glassmesh-5.2.2-<system>-<commit>.7z`, an AES-256 encrypted
   7-Zip archive. Open it with the `GLASSMESH_ARCHIVE_PASSWORD` password (the repository secret):
   - macOS: `brew install sevenzip`, then `7zz x glassmesh-….7z`, or [Keka](https://www.keka.io).
   - Windows: [7-Zip](https://www.7-zip.org).
   - Linux: `7z x glassmesh-….7z` (package `7zip` or `p7zip-full`).
3. Inside is the file from the table above, the one to give to users.
