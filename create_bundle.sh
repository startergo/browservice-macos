#!/bin/bash
set -e

APP_NAME="browservice"
APP_DIR="$APP_NAME.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"

echo "Creating App Bundle structure..."
rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR"
mkdir -p "$FRAMEWORKS_DIR"

echo "Copying executable..."
cp "$APP_NAME" "$MACOS_DIR/"

echo "Copying Framework..."
cp -R "Chromium Embedded Framework.framework" "$FRAMEWORKS_DIR/"

echo "Copying Helper Apps..."
HELPER_BASE="libcef_dll_wrapper/tests/cefsimple/Release"

# List of suffixes for helpers
SUFFIXES=("" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)")

for SUFFIX in "${SUFFIXES[@]}"; do
    SRC_NAME="cefsimple Helper${SUFFIX}"
    DEST_NAME="browservice Helper${SUFFIX}"
    HELPER_SRC="$HELPER_BASE/$SRC_NAME.app"
    HELPER_DEST="$FRAMEWORKS_DIR/$DEST_NAME.app"

    # Convert to lowercase and remove non-alphanumeric chars
    ID_SUFFIX=$(echo "$SUFFIX" | tr '[:upper:]' '[:lower:]' | tr -d ' ()')
    if [ -z "$ID_SUFFIX" ]; then
        BUNDLE_ID="org.browservice.app.helper"
    else
        BUNDLE_ID="org.browservice.app.helper.$ID_SUFFIX"
    fi

    if [ -d "$HELPER_SRC" ]; then
        echo "Processing $DEST_NAME..."
        cp -R "$HELPER_SRC" "$HELPER_DEST"
        
        # Update Infoplist
        /usr/libexec/PlistBuddy -c "Set :CFBundleName $DEST_NAME" "$HELPER_DEST/Contents/Info.plist"
        /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable $DEST_NAME" "$HELPER_DEST/Contents/Info.plist"
        /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$HELPER_DEST/Contents/Info.plist"
        
        # Fix Framework load path for Helper
        install_name_tool -change "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "@executable_path/../../../Chromium Embedded Framework.framework/Chromium Embedded Framework" "$HELPER_DEST/Contents/MacOS/$SRC_NAME"

        # Rename executable
        mv "$HELPER_DEST/Contents/MacOS/$SRC_NAME" "$HELPER_DEST/Contents/MacOS/$DEST_NAME"

        # Re-sign helper app
        codesign --force --deep --sign - "$HELPER_DEST"
    else
        echo "WARNING: Helper app not found at $HELPER_SRC"
    fi
done

echo "Copying Plugins..."
PLUGINS_DIR="$CONTENTS_DIR/PlugIns"
mkdir -p "$PLUGINS_DIR"
# Check where plugin is located
if [ -f "viceplugins/retrojsvice/retrojsvice.so" ]; then
    cp "viceplugins/retrojsvice/retrojsvice.so" "$PLUGINS_DIR/"
elif [ -f "retrojsvice.so" ]; then
    cp "retrojsvice.so" "$PLUGINS_DIR/"
else
    echo "WARNING: retrojsvice.so not found!"
fi

echo "Creating Info.plist..."
cat > "$CONTENTS_DIR/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>org.browservice.app</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
</dict>
</plist>
EOF

echo "App Bundle created at $APP_DIR"
echo "You can run it via: open $APP_DIR"
echo "Or executable directly: $MACOS_DIR/$APP_NAME"

echo "Bundling dependencies with dylibbundler..."
if ! command -v dylibbundler &> /dev/null; then
    echo "Installing dylibbundler..."
    brew install dylibbundler
fi

LIBS_DIR="$CONTENTS_DIR/libs"
mkdir -p "$LIBS_DIR"

# Bundle main executable dependencies
dylibbundler -od -b \
  -x "$MACOS_DIR/$APP_NAME" \
  -d "$LIBS_DIR/" \
  -p @executable_path/../libs/ \
  -i /usr/lib \
  -i /System/Library

# Bundle plugin dependencies
dylibbundler -od -b \
  -x "$PLUGINS_DIR/retrojsvice.so" \
  -d "$LIBS_DIR/" \
  -p @executable_path/../../../libs/ \
  -i /usr/lib \
  -i /System/Library

echo "Re-signing app bundle..."
codesign --force --deep --sign - "$APP_DIR"

echo "Self-contained app bundle created at $APP_DIR"
echo "All Homebrew dependencies bundled - no external libraries required"
