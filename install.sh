#!/bin/bash
set -e

echo "Downloading Browservice..."
curl -L https://github.com/startergo/browservice-macos/releases/download/latest/browservice-macos.dmg \
  -o /tmp/browservice-macos.dmg

echo "Mounting DMG..."
hdiutil attach /tmp/browservice-macos.dmg -quiet

echo "Installing..."
cp -r /Volumes/Browservice/browservice.app /Applications/

echo "Unmounting..."
hdiutil detach /Volumes/Browservice -quiet

echo "Cleaning up..."
rm /tmp/browservice-macos.dmg

echo "Done! Browservice installed to /Applications/browservice.app"
echo "Run it with: open /Applications/browservice.app"
echo "Or: /Applications/browservice.app/Contents/MacOS/browservice"
