# Linux Packaging Guide for WoW Recorder

This document describes how to build and package WoW Recorder for Linux distribution.

## Build Architecture

WoW Recorder consists of two main components:

1. **noobs-linux**: Native Node.js module that interfaces with OBS (libobs)
2. **wow-recorder**: Electron application that uses noobs-linux

## Prerequisites

### Build Dependencies

```bash
# Fedora
sudo dnf install nodejs npm gcc-c++ cmake obs-studio-devel fdk-aac-free-devel

# Ubuntu/Debian
sudo apt install nodejs npm build-essential cmake libobs-dev libfdk-aac-dev

# Arch Linux
sudo pacman -S nodejs npm base-devel cmake obs-studio libfdk-aac
```

### Runtime Dependencies

Users must have these installed:
- `obs-studio` (provides libobs, obs-ffmpeg-mux, and OBS plugins)
- `fdk-aac-free` or `libfdk-aac` (for AAC audio encoding)
- `pipewire` or `pulseaudio` (for audio capture)

## Building

### 1. Build noobs-linux

```bash
cd noobs-linux
npm install
npm run rebuild
```

This compiles the native module (`noobs.node`) which links against libobs.

### 2. Install wow-recorder dependencies

```bash
cd wow-recorder
npm install
cd release/app
npm install
cd ../..
```

The `release/app/package.json` references noobs-linux via `file:../../../noobs-linux`.

### 3. Build the Electron app

```bash
cd wow-recorder
npm run build
```

## Packaging

### Using electron-builder

The project is configured to produce AppImage, deb, and rpm packages:

```bash
cd wow-recorder
npm run package
```

Or build specific formats:

```bash
# AppImage only
npx electron-builder --linux AppImage

# Debian package only
npx electron-builder --linux deb

# RPM only
npx electron-builder --linux rpm
```

Output files are placed in `wow-recorder/release/build/`.

### Package Configuration

The `build` section in `wow-recorder/package.json` configures packaging:

```json
{
  "build": {
    "linux": {
      "artifactName": "WarcraftRecorder-${version}-${arch}.${ext}",
      "target": ["AppImage", "deb", "rpm"],
      "category": "Game;Utility",
      "maintainer": "Warcraft Recorder"
    }
  }
}
```

## AppImage Specifics

### Bundled vs System Dependencies

The AppImage bundles the Electron runtime and the application code, but it does **NOT** bundle system OBS libraries. Users must have OBS Studio installed on their system.

This is by design because:
1. OBS libraries are large and frequently updated
2. System OBS plugins (like obs-pipewire) integrate with the desktop
3. Hardware encoders require matching system drivers

### Runtime Detection

The application automatically:
1. Searches for `obs-ffmpeg-mux` in known system paths
2. Creates a symlink if needed for the replay buffer to work
3. Falls back gracefully if audio encoders aren't available

### Known Issues with AppImage

1. **PipeWire Portal**: The first run may not show the screen capture dialog. Users may need to restart the app after the first launch.

2. **Sandbox Permissions**: AppImage sandboxing may interfere with screen capture. If capture doesn't work, try running with `--no-sandbox` flag.

3. **Audio Device Detection**: PulseAudio/PipeWire devices are detected at runtime. The app may need to be restarted after connecting new audio devices.

## Distribution-Specific Packages

### Debian/Ubuntu (.deb)

The deb package declares runtime dependencies:

```bash
# Add dependencies to electron-builder config if needed:
"deb": {
  "depends": [
    "obs-studio",
    "libfdk-aac2",
    "pipewire"
  ]
}
```

### Fedora/RHEL (.rpm)

```bash
# RPM dependencies:
"rpm": {
  "depends": [
    "obs-studio",
    "fdk-aac-free",
    "pipewire"
  ]
}
```

### Arch Linux (PKGBUILD)

For AUR packaging, create a PKGBUILD:

```bash
pkgname=warcraft-recorder
pkgver=7.3.0
pkgrel=1
pkgdesc="Record your World of Warcraft encounters"
arch=('x86_64')
url="https://www.warcraftrecorder.com/"
license=('custom:CC-BY-NC')
depends=('obs-studio' 'libfdk-aac' 'pipewire-pulse')
makedepends=('nodejs' 'npm' 'cmake')
source=("$pkgname-$pkgver.tar.gz::https://github.com/aza547/wow-recorder/archive/v$pkgver.tar.gz")
```

## Testing the Package

### AppImage Testing

```bash
# Make executable
chmod +x WarcraftRecorder-7.3.0-x86_64.AppImage

# Run
./WarcraftRecorder-7.3.0-x86_64.AppImage

# Run with verbose logging
./WarcraftRecorder-7.3.0-x86_64.AppImage --enable-logging

# Run without sandbox (if screen capture fails)
./WarcraftRecorder-7.3.0-x86_64.AppImage --no-sandbox
```

### Verify Native Module

The native module should be in the unpacked asar:

```bash
# For development/testing
ls -la wow-recorder/release/app/node_modules/noobs/

# For packaged app, extract and check
./WarcraftRecorder-*.AppImage --appimage-extract
ls -la squashfs-root/resources/app.asar.unpacked/node_modules/noobs/
```

## Troubleshooting

### "Cannot find module 'noobs'"

The native module wasn't properly built or packaged. Rebuild:

```bash
cd noobs-linux
npm run rebuild
cd ../wow-recorder/release/app
npm rebuild
```

### "Failed to create process pipe"

OBS can't find `obs-ffmpeg-mux`. The app should auto-detect it, but you can manually check:

```bash
# Verify obs-ffmpeg-mux exists
which obs-ffmpeg-mux || find /usr -name "obs-ffmpeg-mux" 2>/dev/null
```

### Screen capture not working

1. Ensure PipeWire is running: `systemctl --user status pipewire`
2. Grant screen sharing permission in your desktop settings
3. Try running with `--no-sandbox` flag

### No audio in recordings

1. Check FDK-AAC is installed: `find /usr -name "*libfdk*" 2>/dev/null`
2. Verify audio devices in settings
3. Check OBS can record audio independently

## Continuous Integration

For automated builds, you'll need:

```yaml
# GitHub Actions example
jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libobs-dev libfdk-aac-dev

      - name: Build noobs-linux
        run: |
          cd noobs-linux
          npm install
          npm run rebuild

      - name: Build and package
        run: |
          cd wow-recorder
          npm install
          npm run package

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: linux-packages
          path: wow-recorder/release/build/*.{AppImage,deb,rpm}
```

## Version Compatibility

| Component | Minimum Version | Notes |
|-----------|-----------------|-------|
| Node.js | 18.x | Required for native module build |
| Electron | 28.x | For PipeWire support |
| OBS Studio | 30.0 | For libobs compatibility |
| libobs | 30.0 | API compatibility |

## Architecture Support

Currently only **x86_64** is supported. ARM64 support would require:
1. Building noobs.node for ARM64
2. Ensuring OBS ARM64 packages are available
3. Testing hardware encoder support (VAAPI)
