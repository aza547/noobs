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

// Now copy the .dll files we need.
const binSrc = path.resolve(__dirname, 'bin', 'bin', 'win64');
const binDst = path.resolve(__dirname, 'dist', 'bin');

fs.readdirSync(binSrc)
  .filter((file) => file.endsWith('.dll'))
  .forEach((file) => {
    const src = path.join(binSrc, file);
    const dst = path.join(binDst, file);
    fs.copyFileSync(src, dst);
  });

  // Copy executable files required.
const exeFiles = [
  'obs-ffmpeg-mux.exe', // Required for any sort of recording.
  'obs-amf-test.exe',   // For getting AMF encoding capabilities.
  'obs-nvenc-test.exe', // For getting NVENC encoding capabilities.
  'obs-qsv-test.exe'    // For getting QSV encoding capabilities.
];

exeFiles.forEach((file) => {
  const srcPath = path.resolve(__dirname, 'bin', 'bin', 'win64', file);
  const destPath = path.resolve(__dirname, 'dist', 'bin', file);
  fs.copyFileSync(srcPath, destPath);
});

// Copy plugins themselves.
const pluginSrc = path.resolve(__dirname, 'bin', 'obs-plugins', 'win64');
const pluginDst = path.resolve(__dirname, 'dist', 'obs-plugins', 'win64');

fs.cpSync(pluginSrc, pluginDst, { 
  recursive: true ,  
  filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
});

// Copy data, including effects and plugin data.
const dataSrc = path.resolve(__dirname, 'bin', 'data');
const dataDst = path.resolve(__dirname, 'dist', 'data');

fs.cpSync(dataSrc, dataDst, { 
  recursive: true,  
  filter: (src) => !src.endsWith('.pdb') // Exclude PDB files, they are debug files and they are huge.
});


