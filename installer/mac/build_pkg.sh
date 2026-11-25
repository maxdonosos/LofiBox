#!/usr/bin/env bash
# =============================================================================
#  Lo-Fi Box — instalador .pkg para macOS (VST3 + AU + App standalone)
#
#  Uso:  installer/mac/build_pkg.sh <carpeta_artefacts_Release> <version>
#  Ej.:  installer/mac/build_pkg.sh build/LoFiBox_artefacts/Release 1.0.0
#
#  Firma opcional (si están definidas estas variables de entorno):
#    MAC_APP_SIGN_ID        "Developer ID Application: Tu Nombre (TEAMID)"
#    MAC_INSTALLER_SIGN_ID  "Developer ID Installer: Tu Nombre (TEAMID)"
#    NOTARY_APPLE_ID, NOTARY_TEAM_ID, NOTARY_PASSWORD (app-specific password)
#  Sin ellas genera un pkg sin firmar (macOS pedirá "Abrir igualmente").
# =============================================================================
set -euo pipefail

ART="${1:?carpeta de artefactos}"
VERSION="${2:-1.0.0}"
NAME="Lo-Fi Box"
BUNDLE_ID="com.lofiboxaudio.lofibox"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(pwd)/dist"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

VST3="$ART/VST3/$NAME.vst3"
AU="$ART/AU/$NAME.component"
APP="$ART/Standalone/$NAME.app"
for f in "$VST3" "$AU" "$APP"; do [[ -e "$f" ]] || { echo "Falta: $f"; exit 1; }; done

# ---------- firma de código de los bundles
sign_bundle() {
  if [[ -n "${MAC_APP_SIGN_ID:-}" ]]; then
    codesign --force --deep --timestamp --options runtime --sign "$MAC_APP_SIGN_ID" "$1"
  else
    codesign --force --deep --sign - "$1"     # ad-hoc (obligatorio en Apple Silicon)
  fi
}

stage() {   # stage <bundle> <subcarpeta>
  mkdir -p "$WORK/$2"
  ditto "$1" "$WORK/$2/$(basename "$1")"
  sign_bundle "$WORK/$2/$(basename "$1")"
}
stage "$VST3" vst3
stage "$AU"   au
stage "$APP"  app

# ---------- paquetes por componente (no reubicables)
component_pkg() {   # component_pkg <stage> <install-location> <id> <pkg>
  local plist="$WORK/$1.plist"
  pkgbuild --analyze --root "$WORK/$1" "$plist" >/dev/null
  plutil -replace BundleIsRelocatable -bool NO "$plist"
  pkgbuild --root "$WORK/$1" --component-plist "$plist" --install-location "$2" \
           --identifier "$3" --version "$VERSION" "$WORK/$4"
}
component_pkg vst3 "/Library/Audio/Plug-Ins/VST3"       "$BUNDLE_ID.vst3" vst3.pkg
component_pkg au   "/Library/Audio/Plug-Ins/Components" "$BUNDLE_ID.au"   au.pkg
component_pkg app  "/Applications"                      "$BUNDLE_ID.app"  app.pkg

# ---------- distribución con opciones (el usuario elige qué instalar)
sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/resources"
cp "$HERE/welcome.html" "$WORK/resources/"

PKG="$OUT/LoFiBox-$VERSION-mac.pkg"
UNSIGNED="$WORK/unsigned.pkg"
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK" "$UNSIGNED"

if [[ -n "${MAC_INSTALLER_SIGN_ID:-}" ]]; then
  productsign --sign "$MAC_INSTALLER_SIGN_ID" "$UNSIGNED" "$PKG"
  if [[ -n "${NOTARY_APPLE_ID:-}" ]]; then
    xcrun notarytool submit "$PKG" --apple-id "$NOTARY_APPLE_ID" --team-id "$NOTARY_TEAM_ID" \
          --password "$NOTARY_PASSWORD" --wait
    xcrun stapler staple "$PKG"
  fi
else
  cp "$UNSIGNED" "$PKG"
fi

rm -rf "$WORK"
echo "Instalador: $PKG"
