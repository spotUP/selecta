#!/bin/sh
# Wrap a desktop build of selecta in a macOS app bundle and install it.
#
# Usage: scripts/install-mac-app.sh [build-dir] [destination]
#   build-dir    default build/app (configure with -DSELECTA_UI_DESKTOP=ON, Release; GPL plugins optional)
#   destination  default /Applications
#
# Bundle layout:
#   Selecta.app/Contents/MacOS/Selecta   launcher: runs selecta with --ui desktop --backend sdl
#   Selecta.app/Contents/MacOS/selecta-bin   the real binary (APFS is case-insensitive: "selecta" would collide with the launcher)
#   Selecta.app/Contents/PlugIns/        loadable sel_*.dylib modules (loader_dl.c looks here)
#   Selecta.app/Contents/Frameworks/     non-system dylibs (SDL3), rpath-patched
# The app reads the normal config dir ($XDG_CONFIG_HOME/selecta, else ~/.config/selecta).
set -eu

BUILD_DIR=${1:-build/app}
DEST=${2:-/Applications}
BIN="$BUILD_DIR/apps/selecta/Selecta.app/Contents/MacOS/Selecta" # I1023: the build itself makes a bundle; this script re-wraps its binary with the launcher, plugins and SDL3
APP="$DEST/Selecta.app"

if [ ! -x "$BIN" ]; then
    echo "[ERROR] $BIN not found; build the desktop target first" >&2
    exit 1
fi

VERSION=$(git describe --always --dirty 2>/dev/null || echo unknown)
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/selecta-app.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT
C="$STAGE/Selecta.app/Contents"
mkdir -p "$C/MacOS" "$C/PlugIns" "$C/Frameworks"

cp "$BIN" "$C/MacOS/selecta-bin"

# Loadable plugin modules; skip test fixtures.
find "$BUILD_DIR" -name 'sel_*.dylib' -not -path '*/tests/*' -exec cp {} "$C/PlugIns/" \;

# Copy non-system dylibs the binary links and point the binary at the bundled copies.
otool -L "$C/MacOS/selecta-bin" | awk 'NR > 1 { print $1 }' | while read -r lib; do
    case "$lib" in
        /System/* | /usr/lib/* | @*) continue ;;
    esac
    name=$(basename "$lib")
    cp "$lib" "$C/Frameworks/$name"
    chmod u+w "$C/Frameworks/$name"
    install_name_tool -id "@rpath/$name" "$C/Frameworks/$name"
    install_name_tool -change "$lib" "@rpath/$name" "$C/MacOS/selecta-bin"
done
# Drop build-tree rpaths (e.g. /opt/homebrew/lib) so the bundled copies win over the system ones.
otool -l "$C/MacOS/selecta-bin" | awk '/cmd LC_RPATH/ { getline; getline; print $2 }' | while read -r rp; do
    case "$rp" in
        @*) continue ;;
    esac
    install_name_tool -delete_rpath "$rp" "$C/MacOS/selecta-bin"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$C/MacOS/selecta-bin" 2>/dev/null || true

cat > "$C/MacOS/Selecta" <<'EOF'
#!/bin/sh
exec "$(dirname "$0")/selecta-bin" --ui desktop --backend sdl "$@"
EOF
chmod +x "$C/MacOS/Selecta"

cat > "$C/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Selecta</string>
    <key>CFBundleDisplayName</key><string>Selecta</string>
    <key>CFBundleIdentifier</key><string>se.spotup.selecta</string>
    <key>CFBundleExecutable</key><string>Selecta</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>0.1</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

# Ad-hoc signature so Gatekeeper on Apple silicon runs the patched binaries.
codesign --force --sign - "$C/Frameworks/"*.dylib "$C/PlugIns/"*.dylib 2>/dev/null || true
codesign --force --sign - "$C/MacOS/selecta-bin"
codesign --force --sign - "$STAGE/Selecta.app"

rm -rf "$APP"
cp -R "$STAGE/Selecta.app" "$APP"
echo "[OK] installed $APP ($VERSION)"
