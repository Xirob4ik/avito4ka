#!/usr/bin/env bash
# Builds Clipline-<version>-macOS.pkg from a finished macOS build (run on a Mac, after `cmake --build`).
#
#   Installer/macOS/build-pkg.sh <build dir> [config]      e.g.  Installer/macOS/build-pkg.sh build Release
#
# The package installs
#   /Library/Audio/Plug-Ins/VST3/Clipline.vst3          (VST3: FL Studio, Ableton Live, REAPER, Cubase, ...)
#   /Library/Audio/Plug-Ins/Components/Clipline.component (Audio Unit: Logic Pro, GarageBand, FL Studio, ...)
#
# Files installed by macOS Installer do not get the "downloaded from the internet" quarantine flag, so the
# plugins load without Gatekeeper prompts even though they are only ad-hoc signed. The .pkg itself is unsigned:
# the first time, open it with right-click > Open (see docs/INSTALL_MAC.md). With an Apple Developer ID, set
# CLIPLINE_SIGN_APP / CLIPLINE_SIGN_INSTALLER to sign (and notarize separately) for a prompt-free install.
set -euo pipefail

BUILD=${1:?usage: build-pkg.sh <build dir> [config]}
CONFIG=${2:-Release}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
VERSION=$(sed -n 's/^project(Clipline VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")
ID=com.cliplineaudio.clipline

ART="$BUILD/Clipline_artefacts/$CONFIG"
VST3="$ART/VST3/Clipline.vst3"
AU="$ART/AU/Clipline.component"
for b in "$VST3" "$AU"; do [ -d "$b" ] || { echo "missing $b - build the project first"; exit 1; }; done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/vst3" "$WORK/au" "$WORK/pkgs"
cp -R "$VST3" "$WORK/vst3/"
cp -R "$AU" "$WORK/au/"

# Signing: Developer ID if provided, otherwise ad-hoc (required for code to run on Apple Silicon).
SIGN=${CLIPLINE_SIGN_APP:--}
for b in "$WORK/vst3/Clipline.vst3" "$WORK/au/Clipline.component"; do
    xattr -cr "$b"
    codesign --force --deep --timestamp=none --sign "$SIGN" "$b"
    codesign --verify --deep --strict "$b"
    lipo -info "$b/Contents/MacOS/Clipline"
done

# Component packages; bundles must not be "relocated" to wherever an older copy happens to live.
make_component () {   # <name> <root> <install location>
    pkgbuild --analyze --root "$2" "$WORK/$1.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$1.plist"
    plutil -replace 0.BundleOverwriteAction -string upgrade "$WORK/$1.plist"
    pkgbuild --root "$2" --component-plist "$WORK/$1.plist" --identifier "$ID.$1" --version "$VERSION" \
             --install-location "$3" "$WORK/pkgs/Clipline-$1.pkg"
}
make_component vst3 "$WORK/vst3" "/Library/Audio/Plug-Ins/VST3"
make_component au   "$WORK/au"   "/Library/Audio/Plug-Ins/Components"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"

OUT="$BUILD/Clipline-$VERSION-macOS.pkg"
SIGN_ARGS=()
[ -n "${CLIPLINE_SIGN_INSTALLER:-}" ] && SIGN_ARGS=(--sign "$CLIPLINE_SIGN_INSTALLER")
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" \
             --package-path "$WORK/pkgs" "${SIGN_ARGS[@]}" "$OUT"
echo "Created $OUT"
