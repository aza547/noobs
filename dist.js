const fs = require('fs');
const path = require('path');

const packageName = 'noobs.node';
const distRoot = path.resolve(__dirname, 'dist');
const distBin = path.join(distRoot, 'bin');

// Detect platform
const platform = process.platform;
const isWindows = platform === 'win32';
const isLinux = platform === 'linux';

if (!isWindows && !isLinux) {
  throw new Error(`Unsupported platform: ${platform}`);
}

const platformDir = isWindows ? 'win64' : 'linux';

// Clean the dist directory if it exists.
if (fs.existsSync(distRoot)) {
  fs.rmSync(distRoot, { recursive: true, force: true });
}

// Remake the dist directory structure.
fs.mkdirSync(distRoot);
fs.mkdirSync(distBin);

// Copy the compiled .node file.
const addonSrc = path.resolve(__dirname, 'build', 'Release', packageName);
const addonDest = path.join(distRoot, packageName);
fs.copyFileSync(addonSrc, addonDest);

// Copy platform-specific binaries
const binSrc = path.resolve(__dirname, 'bin', 'bin', platformDir);
const binDst = path.resolve(__dirname, 'dist', 'bin');

if (isWindows) {
  // Copy .dll files on Windows
  fs.readdirSync(binSrc)
    .filter((file) => file.endsWith('.dll'))
    .forEach((file) => {
      const src = path.join(binSrc, file);
      const dst = path.join(binDst, file);
      fs.copyFileSync(src, dst);
    });

  // Copy executable files required on Windows
  const exeFiles = [
    'obs-ffmpeg-mux.exe', // Required for any sort of recording.
    'obs-amf-test.exe',   // For getting AMF encoding capabilities.
    'obs-nvenc-test.exe', // For getting NVENC encoding capabilities.
    'obs-qsv-test.exe'    // For getting QSV encoding capabilities.
  ];

  exeFiles.forEach((file) => {
    const srcPath = path.resolve(__dirname, 'bin', 'bin', platformDir, file);
    const destPath = path.resolve(__dirname, 'dist', 'bin', file);
    if (fs.existsSync(srcPath)) {
      fs.copyFileSync(srcPath, destPath);
    }
  });
} else if (isLinux) {
  // Copy everything from the Linux bin directory (libraries, symlinks, executables)
  fs.cpSync(binSrc, binDst, { recursive: true });
}

// Copy plugins themselves.
const pluginSrc = path.resolve(__dirname, 'bin', 'obs-plugins', platformDir);
const pluginDst = path.resolve(__dirname, 'dist', 'obs-plugins', platformDir);

fs.cpSync(pluginSrc, pluginDst, { 
  recursive: true,
  filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
});

// Copy data, including effects and plugin data.
const dataSrc = path.resolve(__dirname, 'bin', 'data');
const dataDst = path.resolve(__dirname, 'dist', 'data');

fs.cpSync(dataSrc, dataDst, { 
  recursive: true,  
  filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
});


