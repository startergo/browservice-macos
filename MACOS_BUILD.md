# Building Browservice Natively on macOS (Apple Silicon)

Native build provides significantly better performance than Docker:
- ~27% CPU vs ~91% CPU for WebGL Aquarium at 60fps
- 10-15% GPU utilization (direct Metal access)
- 60fps WebGL performance
- Native CoreAudio (no PulseAudio needed)
- No Docker required

## Prerequisites

### Xcode Command Line Tools
```bash
xcode-select --install
```

### Homebrew dependencies
```bash
brew install cmake pkg-config pango libpng jpeg-turbo poco dylibbundler
```

## Build

### 1. Clone the repository
```bash
git clone https://github.com/startergo/browservice-macos.git
cd browservice-macos
```

### 2. Download and setup CEF
```bash
./setup_cef_mac.sh
```

### 3. Build
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
  -DJPEG_LIBRARY=/opt/homebrew/opt/jpeg-turbo/lib/libjpeg.dylib \
  -DJPEG_INCLUDE_DIR=/opt/homebrew/opt/jpeg-turbo/include \
  ..
make -j$(sysctl -n hw.logicalcpu)
```

### 4. Create self-contained app bundle
```bash
../create_bundle.sh
```

### 5. Run
```bash
open browservice.app
```

Or from terminal with custom port:
```bash
./browservice.app/Contents/MacOS/browservice \
  --vice-opt-http-listen-addr=0.0.0.0:5555 \
  --vice-opt-default-quality=75 \
  --vice-opt-quality-selector=YES
```

## Install prebuilt release (recommended)

```bash
curl -sL https://raw.githubusercontent.com/startergo/browservice-macos/master/install.sh | bash
```

No Gatekeeper issues — curl does not set the quarantine flag.

## Connect from Win98/Snow Leopard in QEMU

In IE/Safari go to: `http://10.0.2.2:8080`

## Performance

Tested on Apple Silicon with WebGL Aquarium at 60fps:
- CPU: ~27%
- GPU: 10-15%

Compared to Docker approach:
- CPU: ~91%
- GPU: 0% (software rendering only)

## Running the prebuilt DMG manually

Since the app is ad-hoc signed (not notarized), macOS Gatekeeper will block it
if downloaded via a browser. Use `install.sh` above to avoid this, or:

```bash
xattr -cr /path/to/browservice.app
open /path/to/browservice.app
```

## Notes

- No Docker required
- No PulseAudio required — uses native CoreAudio
- Direct Metal GPU access for WebGL
- Requires rebuilding when Browservice releases a new version
- For Intel Macs, use the Docker approach in startergo/browservice
