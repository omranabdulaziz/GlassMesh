#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Wraps a packaged build in an AES-256 encrypted 7-Zip archive (file names encrypted too) before it
# is uploaded as a workflow artifact, see `.github/workflows/glassmesh_build.yml`.
#
#   ARCHIVE_PASSWORD=... bash tools/glassmesh/ci_encrypt.sh <package>
#
# Without a password nothing is written and `encrypted=false` is reported, so the workflow never
# uploads a readable build.

set -euo pipefail

package="$1"
out="${GITHUB_OUTPUT:-/dev/null}"

if [ -z "${ARCHIVE_PASSWORD:-}" ]; then
  echo "::warning::The GLASSMESH_ARCHIVE_PASSWORD secret is not set, the build is not uploaded."
  echo "encrypted=false" >> "${out}"
  exit 0
fi

sevenzip=""
for candidate in 7zz 7z 7za; do
  if command -v "${candidate}" > /dev/null; then
    sevenzip="${candidate}"
    break
  fi
done
if [ -z "${sevenzip}" ] && command -v brew > /dev/null; then
  brew install sevenzip > /dev/null
  sevenzip="7zz"
fi
if [ -z "${sevenzip}" ]; then
  echo "::error::7-Zip is not available, the build can't be encrypted."
  exit 1
fi

# The package is already compressed: store it (-mx=0), encrypt data and headers (-mhe=on).
"${sevenzip}" a -t7z -mx=0 -mhe=on "-p${ARCHIVE_PASSWORD}" "${package%.*}.7z" "${package}" > /dev/null
rm -f "${package}"
ls -lh "${package%.*}.7z"
echo "encrypted=true" >> "${out}"
