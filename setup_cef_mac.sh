#!/bin/bash

set -e

# Using slightly older stable version
CEF_VERSION="140.1.15+gfaef09b+chromium-140.0.7339.214"

# Auto-detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    CEF_PLATFORM="macosarm64"
else
    CEF_PLATFORM="macosx64"
fi

echo "Detected architecture: $ARCH -> CEF platform: $CEF_PLATFORM"

CEF_DIR="cef"
CEF_TARBALL="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2"

CEF_URL_VERSION="${CEF_VERSION//+/%2B}"
DOWNLOAD_URL="https://cef-builds.spotifycdn.com/cef_binary_${CEF_URL_VERSION}_${CEF_PLATFORM}.tar.bz2"

if [ -d "${CEF_DIR}" ]; then
    echo "Directory '${CEF_DIR}' already exists. Skipping download and extraction."
else
    if [ ! -f "${CEF_TARBALL}" ]; then
        echo "Downloading CEF for ${CEF_PLATFORM}..."
        echo "URL: ${DOWNLOAD_URL}"
        curl -L -o "${CEF_TARBALL}" "${DOWNLOAD_URL}"
    fi

    echo "Extracting the downloaded CEF binary..."
    mkdir -p "${CEF_DIR}"
    tar -xjf "${CEF_TARBALL}" -C "${CEF_DIR}" --strip-components=1
fi

# Apple Silicon specific fixes
if [ "$ARCH" = "arm64" ]; then
    echo "Applying macOS Apple Silicon fixes..."

    if ! grep -q "libcef_loader_impl.cc" "${CEF_DIR}/libcef_dll/CMakeLists.txt"; then
        echo "Patching libcef_dll/CMakeLists.txt to remove conflicting libcef_dll_dylib.cc and add libcef_loader_impl.cc..."
        sed -i '' '/wrapper\/libcef_dll_dylib.cc/d' "${CEF_DIR}/libcef_dll/CMakeLists.txt"
        sed -i '' '/wrapper\/cef_scoped_sandbox_context_mac.mm/a\
  wrapper/libcef_loader_impl.cc
' "${CEF_DIR}/libcef_dll/CMakeLists.txt"
    fi

    if [ ! -f "${CEF_DIR}/libcef_dll/wrapper/libcef_loader_impl.cc" ]; then
        echo "Creating libcef_loader_impl.cc..."
        cat > "${CEF_DIR}/libcef_dll/wrapper/libcef_loader_impl.cc" <<CPPEOF
#include "include/wrapper/cef_library_loader.h"

int cef_load_library(const char* path) {
  return 1;
}

int cef_unload_library() {
  return 1;
}
CPPEOF
    fi
fi

echo "CEF setup and patching complete in '${CEF_DIR}'."
