#!/bin/bash
# ============================================
# MAC Lookup - AppImage Deploy Script (Linux/ARM)
# Uses linuxdeployqt + appimagetool -> AppImage
# Usage: ./deploy-mac_lookup.sh [release|debug]
# ============================================

set -e

BUILD_TYPE=${1:-release}
APP_NAME="mac_lookup"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
SRC_DIR="${PROJECT_DIR}/${APP_NAME}"
BUILD_DIR="${PROJECT_DIR}/build-${APP_NAME}"
DEPLOY_DIR="${PROJECT_DIR}/deploy-${APP_NAME}"
APP_DIR="${DEPLOY_DIR}/AppDir"

# --- find tools ---
for candidate in "linuxdeployqt-continuous-x86_64.AppImage" "linuxdeployqt"; do
    if [ -x "${SCRIPT_DIR}/${candidate}" ]; then
        LINUXDEPLOYQT="${LINUXDEPLOYQT:-${SCRIPT_DIR}/${candidate}}"
        break
    fi
done
LINUXDEPLOYQT="${LINUXDEPLOYQT:-linuxdeployqt}"

for candidate in "appimagetool-x86_64.AppImage" "appimagetool"; do
    if [ -x "${SCRIPT_DIR}/${candidate}" ]; then
        APPIMAGETOOL="${APPIMAGETOOL:-${SCRIPT_DIR}/${candidate}}"
        break
    fi
done
APPIMAGETOOL="${APPIMAGETOOL:-appimagetool}"

QMAKE="${QMAKE:-$(command -v qmake || true)}"

echo "============================================"
echo "Deploying ${APP_NAME} -> AppImage"
echo "Build type    : ${BUILD_TYPE}"
echo "linuxdeployqt : $(command -v $LINUXDEPLOYQT || echo 'NOT FOUND')"
echo "appimagetool  : $(command -v $APPIMAGETOOL || echo 'NOT FOUND')"
echo "============================================"

# --- find executable ---
EXE_PATH=""
for candidate in \
    "${BUILD_DIR}/${APP_NAME}" \
    "${BUILD_DIR}/release/${APP_NAME}" \
    "${BUILD_DIR}/debug/${APP_NAME}"; do
    if [ -f "$candidate" ]; then
        EXE_PATH="$candidate"
        break
    fi
done

if [ -z "$EXE_PATH" ]; then
    echo "ERROR: Executable not found in:"
    echo "  ${BUILD_DIR}/"
    echo "  ${BUILD_DIR}/release/"
    echo "Run: ./build.sh ${BUILD_TYPE}"
    exit 1
fi

if ! command -v "$LINUXDEPLOYQT" &>/dev/null; then
    echo "ERROR: linuxdeployqt not found."
    echo "Place linuxdeployqt-continuous-x86_64.AppImage in ${SCRIPT_DIR}"
    echo "Or set: export LINUXDEPLOYQT=/path/to/linuxdeployqt"
    exit 1
fi

if ! command -v "$APPIMAGETOOL" &>/dev/null; then
    echo "ERROR: appimagetool not found."
    echo "Place appimagetool-x86_64.AppImage in ${SCRIPT_DIR}"
    echo "Or set: export APPIMAGETOOL=/path/to/appimagetool"
    exit 1
fi

# --- prepare AppDir ---
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/${APP_NAME}/data"
mkdir -p "$APP_DIR/usr/share/applications"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# 1) executable
echo "[1/4] Copying executable..."
cp "$EXE_PATH" "$APP_DIR/usr/bin/"

# 2) data files
echo "[2/4] Copying data files..."
DATA_SRC="${SRC_DIR}/data"
if [ -d "$DATA_SRC" ]; then
    cp -r "$DATA_SRC"/* "$APP_DIR/usr/share/${APP_NAME}/data/"
    mkdir -p "$APP_DIR/usr/bin/data"
    cp -r "$DATA_SRC"/* "$APP_DIR/usr/bin/data/"
fi

# 3) .desktop file
echo "[3/4] Creating .desktop file..."
DESKTOP_FILE="$APP_DIR/usr/share/applications/${APP_NAME}.desktop"
cat > "$DESKTOP_FILE" << EOF
[Desktop Entry]
Type=Application
Name=MAC Lookup
Comment=MAC address lookup with LAN and WiFi scanning
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Network;Utility;
Terminal=false
EOF

cp "$DESKTOP_FILE" "$APP_DIR/"

# 4) icon
echo "[4/4] Copying icon..."
ICON_SRC=""
for candidate in \
    "${SRC_DIR}/data/app_icon.png" \
    "${SRC_DIR}/data/mac_lookup.png" \
    "${SRC_DIR}/data/mac_arp_wifi_arm.png"; do
    if [ -f "$candidate" ]; then
        ICON_SRC="$candidate"
        break
    fi
done

if [ -n "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$APP_DIR/${APP_NAME}.png"
    cp "$ICON_SRC" "$APP_DIR/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
else
    echo "WARNING: icon not found. Create one manually."
fi

# === Step 1: linuxdeployqt ===
echo ""
echo "=== Step 1: Bundling Qt libraries (linuxdeployqt) ==="
cd "$DEPLOY_DIR"
OPTS=""
[ -n "$QMAKE" ] && OPTS="-qmake=${QMAKE}"

$LINUXDEPLOYQT "$DESKTOP_FILE" $OPTS -bundle-non-qt-libs

echo ""
echo "=== linuxdeployqt bundle complete. ==="

# === Step 2: appimagetool ===
echo ""
echo "=== Step 2: Creating AppImage (appimagetool) ==="
APPIMAGE_OUT="${DEPLOY_DIR}/${APP_NAME}-$(uname -m).AppImage"

if [[ "$APPIMAGETOOL" == *.AppImage ]]; then
    chmod +x "$APPIMAGETOOL"
    $APPIMAGETOOL --no-appstream "$APP_DIR" "$APPIMAGE_OUT"
else
    $APPIMAGETOOL --no-appstream "$APP_DIR" "$APPIMAGE_OUT"
fi

chmod +x "$APPIMAGE_OUT" 2>/dev/null || true

echo ""
echo "============================================"
echo "Done!"
echo "============================================"
echo "AppImage: ${APPIMAGE_OUT}"
echo ""
echo "Run it anywhere:"
echo "  ${APPIMAGE_OUT}"
echo "============================================"
