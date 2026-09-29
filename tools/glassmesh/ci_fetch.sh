#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Used by `.github/workflows/glassmesh_build.yml`: downloads what a build needs besides the Git
# checkout, like `make update` does, but without updating the checkout itself:
# - the Git LFS files of the Blender sources (from projects.blender.org, see `.lfsconfig`),
# - Blender's pre-compiled libraries for this platform (the Git submodule in `lib/`).
#
# Also sets `GLASSMESH_VERSION` for the following workflow steps.

set -euo pipefail

if [[ "${RUNNER_OS:-}" == "Windows" ]]; then
  PYTHON=python
else
  PYTHON=python3
fi

git lfs install

echo "Downloading Git LFS files..."
git lfs pull

echo "Downloading pre-compiled libraries..."
"${PYTHON}" build_files/utils/make_update.py --no-blender --no-lfs-fallback

# The build only uses the checked out files, remove the second copy Git LFS keeps of them.
for lib in lib/*/; do
  if [[ -e "${lib}.git" ]]; then
    lfs_objects="$(git -C "${lib}" rev-parse --absolute-git-dir)/lfs/objects"
    if [[ -d "${lfs_objects}" ]]; then
      echo "Removing ${lfs_objects}"
      rm -rf "${lfs_objects}"
    fi
  fi
done

version="$(awk '/^#define BLENDER_VERSION / { v = $3 }
                /^#define BLENDER_VERSION_PATCH / { p = $3 }
                END { printf "%d.%d.%d", int(v / 100), v % 100, p }' \
           source/blender/blenkernel/BKE_blender_version.h)"
echo "GlassMesh version: ${version}"
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "GLASSMESH_VERSION=${version}" >> "${GITHUB_ENV}"
fi

df -h . || true
