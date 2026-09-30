#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 GlassMesh Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Compiles the GlassMesh app icon into an asset catalog (`Assets.car`) inside the application
# bundle and names it in `Info.plist` (`CFBundleIconName`), the way macOS 11 and newer (and the
# Liquid Glass look of macOS 26) prefer app icons. Compiling needs Xcode's `actool`, so this runs in
# the macOS CI job, after building and before signing. Without it, the bundle's
# `glassmesh_icon.icns` is the icon, so a failure here is not fatal.
#
# Usage: ci_macos_icon.sh path/to/GlassMesh.app

set -euo pipefail

app="$1"
resources="${app}/Contents/Resources"
plist="${app}/Contents/Info.plist"
icns="${resources}/glassmesh_icon.icns"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

# The icon images, from the largest image of the `.icns` file.
iconutil --convert iconset --output "${work}/source.iconset" "${icns}"
largest=""
largest_width=0
for png in "${work}"/source.iconset/*.png; do
  width="$(sips --getProperty pixelWidth "${png}" | awk '/pixelWidth/ {print $2}')"
  if [ "${width}" -gt "${largest_width}" ]; then
    largest="${png}"
    largest_width="${width}"
  fi
done
echo "Icon source: ${largest} (${largest_width} px)"

catalog="${work}/Assets.xcassets"
iconset="${catalog}/AppIcon.appiconset"
mkdir -p "${iconset}"
printf '{"info": {"author": "xcode", "version": 1}}\n' > "${catalog}/Contents.json"

images=""
for size in 16 32 128 256 512; do
  for scale in 1 2; do
    pixels=$((size * scale))
    if [ "${scale}" = 2 ]; then
      file="icon_${size}x${size}@2x.png"
    else
      file="icon_${size}x${size}.png"
    fi
    sips --resampleHeightWidth "${pixels}" "${pixels}" "${largest}" --out "${iconset}/${file}" >/dev/null
    images="${images}{\"filename\": \"${file}\", \"idiom\": \"mac\", \"scale\": \"${scale}x\", \"size\": \"${size}x${size}\"},"
  done
done
printf '{"images": [%s], "info": {"author": "xcode", "version": 1}}\n' "${images%,}" > "${iconset}/Contents.json"

mkdir -p "${work}/compiled"
xcrun actool \
  --compile "${work}/compiled" \
  --platform macosx \
  --minimum-deployment-target 11.0 \
  --app-icon AppIcon \
  --output-partial-info-plist "${work}/partial.plist" \
  --errors --warnings --notices \
  "${catalog}"

if [ ! -f "${work}/compiled/Assets.car" ]; then
  echo "::warning::actool did not write Assets.car, the app keeps the .icns icon"
  exit 1
fi

cp "${work}/compiled/Assets.car" "${resources}/Assets.car"
/usr/libexec/PlistBuddy -c "Delete :CFBundleIconName" "${plist}" 2>/dev/null || true
/usr/libexec/PlistBuddy -c "Add :CFBundleIconName string AppIcon" "${plist}"
echo "Compiled the app icon into ${resources}/Assets.car"
