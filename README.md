# No(de)obs
> Previously named "warcraft-recorder-obs-engine"

This library is still very young, you probably don't want to use it.

Native node bindings to libobs, built for Warcraft Recorder. 

An alternative but less mature `obs-studio-node` with a focus on recording gameplay from a replay buffer.

Uses a [custom](https://github.com/aza547/warcraft-recorder-obs-studio) fork of libobs to:
- Enable "replay_buffer to recording".
- Allow libobs to be used as a library easier.

Windows and Linux are supported.

## Installation

```bash
npm install noobs
```

## Usage

### Lifecycle
```javascript
import noobs from 'noobs';

const distPath = ...;
const logPath = ...;
const cb = (signal) => console.log(signal);

noobs.Init(distPath, logPath, cb);
...
noobs.Shutdown();
```

### Sources
```javascript
noobs.CreateSource('Test Source', 'monitor_capture'); // Creates a source
noobs.AddSourceToScene('Test Source'); // Add the source to the scene
const s = noobs.GetSourceSettings('Test Source'); // View current settings
const p = noobs.GetSourceProperties('Test Source'); // View settings schema
noobs.SetSourceSettings('Test Source', { ...s, monitor: 1 });
noobs.RemoveSourceFromScene('Test Source'); // Remove a source from the scene
noobs.DeleteSource('Test Source') // Release a source
```

###  Basic Recording Usage
```javascript
noobs.StartRecording();
...
noobs.StopRecording();
```

### Buffer Recording
```javascript
noobs.SetBuffering(true);

noobs.StartBuffering();
...
noobs.StartRecording(5); // include last 5 seconds of the recording buffer
...
noobs.StopRecording();
```

### Preview
```javascript
const hwnd = this.mainWindow.getNativeWindowHandle();
noobs.InitPreview(hwnd);
noobs.ShowPreview(x, y, width, height); // Use this for moving/resizing
// noobs.HidePreview();
```

See `test.js` for more.

### TypeScript

This package includes TypeScript definitions.

## Requirements

TODO

- Node.js ??
- A C++ compiler (Visual Studio Build Tools on Windows, Xcode on macOS, GCC on Linux) ??
- Node-gyp ??

## Building from Source

### Windows

```bash
npm install       # install deps
npm run build     # compile it
```

### Linux

Requires X11 development headers:

```bash
# Arch/CachyOS
sudo pacman -S libx11 libxext

# Fedora/Bazzite
sudo dnf install libX11-devel libXext-devel

npm install       # install deps
npm run build     # compile it
```

The pre-built OBS libraries are included in `bin/native/linux/`.

### Publishing

```bash
npm version patch # version bump
npm pack          # build tgz locally

npm login
npm publish       # publish to npm
```

## License

GPL-2.0
