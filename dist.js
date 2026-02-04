const fs = require('fs');
const path = require('path');

const packageName = 'noobs.node';
const distRoot = path.resolve(__dirname, 'dist');
const distBin = path.join(distRoot, 'bin');

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

// Copy Windows binaries to dist/bin/win64
const win64BinSrc = path.resolve(__dirname, 'bin', 'native', 'win64');
const win64BinDst = path.join(distBin, 'win64');

if (fs.existsSync(win64BinSrc)) {
  fs.mkdirSync(win64BinDst);

  // Copy .dll files
  fs.readdirSync(win64BinSrc)
    .filter((file) => file.endsWith('.dll'))
    .forEach((file) => {
      const src = path.join(win64BinSrc, file);
      const dst = path.join(win64BinDst, file);
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
    const srcPath = path.join(win64BinSrc, file);
    const destPath = path.join(win64BinDst, file);
    if (fs.existsSync(srcPath)) {
      fs.copyFileSync(srcPath, destPath);
    }
  });
}

// Copy Linux binaries to dist/bin/linux
const linuxBinSrc = path.resolve(__dirname, 'bin', 'native', 'linux');
const linuxBinDst = path.join(distBin, 'linux');

if (fs.existsSync(linuxBinSrc)) {
  fs.cpSync(linuxBinSrc, linuxBinDst, { recursive: true });
}

// Copy plugins for both platforms.
// Windows plugins
const win64PluginSrc = path.resolve(__dirname, 'bin', 'obs-plugins', 'win64');
const win64PluginDst = path.resolve(__dirname, 'dist', 'obs-plugins', 'win64');

if (fs.existsSync(win64PluginSrc)) {
  fs.cpSync(win64PluginSrc, win64PluginDst, { 
    recursive: true,
    filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
  });
}

// Linux plugins
const linuxPluginSrc = path.resolve(__dirname, 'bin', 'obs-plugins', 'linux');
const linuxPluginDst = path.resolve(__dirname, 'dist', 'obs-plugins', 'linux');

if (fs.existsSync(linuxPluginSrc)) {
  fs.cpSync(linuxPluginSrc, linuxPluginDst, { 
    recursive: true,
    filter: (src) => !src.endsWith('.pdb')
  });
}

// Copy data, including effects and plugin data.
const dataSrc = path.resolve(__dirname, 'bin', 'data');
const dataDst = path.resolve(__dirname, 'dist', 'data');

fs.cpSync(dataSrc, dataDst, { 
  recursive: true,  
  filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
});


