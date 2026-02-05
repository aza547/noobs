# Linux Dependencies for WoW Recorder

This document outlines the dependencies required to run WoW Recorder on various Linux distributions.

## Required Dependencies

### Core OBS Libraries

| Package | Fedora | Ubuntu/Debian | Arch Linux | openSUSE |
|---------|--------|---------------|------------|----------|
| libobs | `obs-studio` | `libobs-dev`, `obs-studio` | `obs-studio` | `obs-studio` |
| obs-ffmpeg-mux | included in `obs-studio` | included in `obs-studio` | included in `obs-studio` | included in `obs-studio` |

### Audio Encoding (FDK-AAC)

| Distribution | Package Name | Repository |
|--------------|--------------|------------|
| Fedora | `fdk-aac-free` | RPM Fusion Free |
| Ubuntu | `libfdk-aac2`, `libfdk-aac-dev` | Universe |
| Debian | `libfdk-aac2`, `libfdk-aac-dev` | Non-free (Bookworm+) |
| Arch Linux | `libfdk-aac` | Extra |
| openSUSE | `fdk-aac-free` | Packman |

### PulseAudio/PipeWire

| Distribution | Package Name |
|--------------|--------------|
| Fedora | `pipewire-pulseaudio` (default), `pulseaudio` |
| Ubuntu/Debian | `pipewire` or `pulseaudio` |
| Arch Linux | `pipewire-pulse` or `pulseaudio` |
| openSUSE | `pipewire` or `pulseaudio` |

### Build Dependencies (for noobs-linux)

| Package | Fedora | Ubuntu/Debian | Arch Linux | openSUSE |
|---------|--------|---------------|------------|----------|
| Node.js | `nodejs` | `nodejs` | `nodejs` | `nodejs18` |
| npm | `npm` | `npm` | `npm` | `npm18` |
| node-gyp | `npm install -g node-gyp` | `npm install -g node-gyp` | `npm install -g node-gyp` | `npm install -g node-gyp` |
| C++ compiler | `gcc-c++` | `build-essential` | `base-devel` | `gcc-c++` |
| CMake | `cmake` | `cmake` | `cmake` | `cmake` |

## File Locations

### obs-ffmpeg-mux Location

| Distribution | Path |
|--------------|------|
| Fedora | `/usr/bin/obs-ffmpeg-mux` |
| Ubuntu/Debian | `/usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux` |
| Arch Linux | `/usr/bin/obs-ffmpeg-mux` |
| openSUSE | `/usr/bin/obs-ffmpeg-mux` |

**Note:** On Ubuntu/Debian, the muxer is in a non-standard location. The application attempts to find it automatically, but you may need to create a symlink:

```bash
# Ubuntu/Debian only - if muxer not found automatically
sudo ln -sf /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux /usr/bin/obs-ffmpeg-mux
```

### OBS Plugin Paths

| Distribution | Path |
|--------------|------|
| Fedora | `/usr/lib64/obs-plugins/` |
| Ubuntu/Debian (x86_64) | `/usr/lib/x86_64-linux-gnu/obs-plugins/` |
| Arch Linux | `/usr/lib/obs-plugins/` |
| openSUSE | `/usr/lib64/obs-plugins/` |

### OBS Data Path

| Distribution | Path |
|--------------|------|
| Fedora | `/usr/share/obs/` |
| Ubuntu/Debian | `/usr/share/obs/` |
| Arch Linux | `/usr/share/obs/` |
| openSUSE | `/usr/share/obs/` |

## Installation Instructions

### Fedora

```bash
# Enable RPM Fusion (if not already enabled)
sudo dnf install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm

# Install dependencies
sudo dnf install obs-studio fdk-aac-free pipewire-pulseaudio

# Build dependencies (for development)
sudo dnf install nodejs npm gcc-c++ cmake obs-studio-devel
```

### Ubuntu / Linux Mint

```bash
# Ubuntu 22.04+ / Linux Mint 21+
sudo apt update
sudo apt install obs-studio libfdk-aac2 libfdk-aac-dev pipewire

# Build dependencies (for development)
sudo apt install nodejs npm build-essential cmake libobs-dev

# Create symlink for obs-ffmpeg-mux (if needed)
if [ -f /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux ]; then
    sudo ln -sf /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux /usr/bin/obs-ffmpeg-mux
fi
```

### Debian

```bash
# Debian 12 (Bookworm) and later
# Enable non-free repository for FDK-AAC
sudo apt edit-sources
# Add "non-free" to end of your deb lines

sudo apt update
sudo apt install obs-studio libfdk-aac2 libfdk-aac-dev pipewire

# Build dependencies (for development)
sudo apt install nodejs npm build-essential cmake libobs-dev

# Create symlink for obs-ffmpeg-mux (if needed)
if [ -f /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux ]; then
    sudo ln -sf /usr/lib/x86_64-linux-gnu/obs-plugins/obs-ffmpeg/obs-ffmpeg-mux /usr/bin/obs-ffmpeg-mux
fi
```

### Arch Linux / Manjaro

```bash
# Install dependencies
sudo pacman -S obs-studio libfdk-aac pipewire-pulse

# Build dependencies (for development)
sudo pacman -S nodejs npm base-devel cmake
```

### openSUSE

```bash
# Enable Packman repository for FDK-AAC
sudo zypper ar -cfp 90 'https://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Tumbleweed/' packman

# Install dependencies
sudo zypper install obs-studio fdk-aac-free pipewire

# Build dependencies (for development)
sudo zypper install nodejs18 npm18 gcc-c++ cmake obs-studio-devel
```

## Troubleshooting

### No Audio in Recordings

1. **Check FDK-AAC is installed:**
   ```bash
   # Look for the obs-libfdk module
   find /usr -name "*libfdk*" 2>/dev/null
   ```

2. **Check audio devices:**
   ```bash
   # List PulseAudio/PipeWire sources
   pactl list sources short
   ```

3. **Verify OBS can access audio:**
   - Open regular OBS Studio
   - Add an audio source
   - If it works there, it should work in WoW Recorder

### "Failed to create process pipe" Error

This means `obs-ffmpeg-mux` wasn't found. Solutions:

1. **Find the muxer:**
   ```bash
   find /usr -name "obs-ffmpeg-mux" 2>/dev/null
   ```

2. **Create symlink to `/usr/bin/`:**
   ```bash
   sudo ln -sf /path/to/obs-ffmpeg-mux /usr/bin/obs-ffmpeg-mux
   ```

### Video Recording Works but No File Saved

1. Check that the storage path exists and is writable
2. Check logs for errors during the save operation
3. Ensure sufficient disk space

### Screen Capture Not Working (Wayland)

On Wayland, screen capture requires PipeWire. Install it:

```bash
# Fedora
sudo dnf install pipewire

# Ubuntu/Debian
sudo apt install pipewire

# Arch
sudo pacman -S pipewire
```

You may need to grant screen sharing permissions through your desktop environment's settings.

## Hardware Acceleration

### NVIDIA (NVENC)

```bash
# Fedora
sudo dnf install nvidia-driver cuda

# Ubuntu
sudo apt install nvidia-driver-xxx  # Replace xxx with version

# Arch
sudo pacman -S nvidia nvidia-utils cuda
```

### AMD (VAAPI)

```bash
# Fedora
sudo dnf install mesa-va-drivers libva-utils

# Ubuntu
sudo apt install mesa-va-drivers vainfo

# Arch
sudo pacman -S libva-mesa-driver libva-utils
```

### Intel (QSV/VAAPI)

```bash
# Fedora
sudo dnf install intel-media-driver libva-utils

# Ubuntu
sudo apt install intel-media-va-driver vainfo

# Arch
sudo pacman -S intel-media-driver libva-utils
```

## Verified Working Configurations

| Distribution | Version | Desktop | Status |
|--------------|---------|---------|--------|
| Fedora | 43 | GNOME (Wayland) | ✅ Working |
| Ubuntu | 24.04 | GNOME | 🔄 Untested |
| Arch Linux | Rolling | Various | 🔄 Untested |
| Debian | 12 | GNOME | 🔄 Untested |
| openSUSE | Tumbleweed | KDE | 🔄 Untested |

## Notes

- **Wayland vs X11:** PipeWire screen capture works on both Wayland and X11. X11 capture (`linux-capture` module) only works on X11.
- **Flatpak OBS:** If using Flatpak version of OBS Studio, paths will be different (`~/.var/app/com.obsproject.Studio/...`). The Flatpak version is NOT recommended for WoW Recorder.
- **Wine/Proton:** WoW runs via Wine/Proton (Lutris, Heroic, Steam). Make sure your Wine prefix logs are accessible at the configured path.
