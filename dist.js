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

if (process.platform === 'win32') {
  // Now copy the .dll files we need.
  const binSrc = path.resolve(__dirname, 'bin', '64bit');
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
    'obs-qsv-test.exe',    // For getting QSV encoding capabilities.
    'ffmpeg.exe', // Dynamically linked ffmpeg exe.
    'ffprobe.exe' // Dynamically linked ffprobe exe.
  ];

  exeFiles.forEach((file) => {
    const srcPath = path.resolve(__dirname, 'bin', '64bit', file);
    const destPath = path.resolve(__dirname, 'dist', 'bin', file);
    fs.copyFileSync(srcPath, destPath);
  });

  // Copy plugins themselves.
  const pluginSrc = path.resolve(__dirname, 'bin', 'obs-plugins');
  const pluginDst = path.resolve(__dirname, 'dist', 'obs-plugins');

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
} else if (process.platform === 'darwin') {
  // Mac dist layout (Phase 5 / 6 of the macOS port):
  //   dist/noobs.node                  the native addon
  //   dist/Frameworks/*                libobs.framework + dylibs +
  //                                    obs-ffmpeg-mux helper
  //   dist/PlugIns/*.plugin            Mac plugin bundles
  //   dist/data/effects/*              libobs effect files
  //
  // The consuming app (wow-recorder) passes <dist> to noobs.Init as
  // `distPath`; init_obs reads PlugIns/, data/effects/, and
  // Frameworks/ from there, and obs_interface_mac.mm hands the
  // contentView to obs_display_create.
  const macSources = [
    { name: 'Frameworks', src: path.resolve(__dirname, 'Frameworks') },
    { name: 'PlugIns',   src: path.resolve(__dirname, 'PlugIns')   },
    { name: 'data',      src: path.resolve(__dirname, 'data')      },
  ];
  // fs.cpSync rewrites symlink targets to absolute paths in older
  // node versions. The libobs.framework needs its symlinks to stay
  // RELATIVE (`Versions/Current/libobs`, etc.) for both the canonical
  // bundle layout and so consumers like electron-builder can copy the
  // result without ENOENT errors. Shell out to `cp -R` which
  // preserves symlinks verbatim.
  const { execFileSync } = require('child_process');
  for (const { name, src } of macSources) {
    if (!fs.existsSync(src)) {
      console.warn(`[dist] missing ${name}/ — run scripts/build-libobs-mac.sh first`);
      continue;
    }
    const dst = path.join(distRoot, name);
    execFileSync('cp', ['-R', src, dst], { stdio: 'inherit' });
    console.log(`[dist] copied ${name}/ → dist/${name}/`);
  }

  // libobs.framework was baked with rpath @executable_path/../Frameworks
  // on the OBS build assumption that it lives inside a self-contained
  // .app bundle. Embedded in noobs/dist/Frameworks the consumer's
  // executable is Node or Electron (wrong location). Add @loader_path/.
  // so libobs's own dlopen calls (libavcodec, libavformat, etc.) find
  // the sibling dylibs we ship next to it.
  // Same rpath story for each plugin bundle. Plugin binaries live at
  //   dist/PlugIns/<name>.plugin/Contents/MacOS/<name>
  // Their @rpath/lib*.dylib lookups (libavcodec, libavdevice, ...)
  // need to find dist/Frameworks/, four levels up from their own
  // @loader_path. Without this they fall through to libobs's rpath
  // which points at libobs.framework/Versions/A/ — only libobs.dylib
  // is found there, sibling lookups fail.
  const pluginsDir = path.join(distRoot, 'PlugIns');
  if (fs.existsSync(pluginsDir)) {
    for (const entry of fs.readdirSync(pluginsDir)) {
      if (!entry.endsWith('.plugin')) continue;
      const name = entry.replace(/\.plugin$/, '');
      const bin = path.join(pluginsDir, entry, 'Contents', 'MacOS', name);
      if (!fs.existsSync(bin)) continue;
      try {
        execFileSync('install_name_tool', ['-add_rpath', '@loader_path/../../../../Frameworks', bin], { stdio: 'inherit' });
        console.log(`[dist] patched ${name} plugin rpath`);
      } catch (err) {
        console.warn(`[dist] install_name_tool on ${name} failed:`, err.message);
      }
    }
  }

  const libobs = path.join(distRoot, 'Frameworks', 'libobs.framework', 'Versions', 'A', 'libobs');
  if (fs.existsSync(libobs)) {
    // libobs lives at dist/Frameworks/libobs.framework/Versions/A/.
    // Its dlopens of @rpath/lib*.dylib (libavcodec, libavformat, etc.)
    // need to resolve to dist/Frameworks/, three levels up from
    // libobs's own @loader_path. Add that rpath explicitly so libobs
    // doesn't rely on the broken @executable_path/../Frameworks rpath
    // it was originally baked with.
    try {
      execFileSync('install_name_tool', ['-add_rpath', '@loader_path/../../..', libobs], { stdio: 'inherit' });
      console.log('[dist] patched libobs rpath to @loader_path/../../..');
    } catch (err) {
      console.warn('[dist] install_name_tool failed (already patched?):', err.message);
    }
  }

  // Flatten libobs.framework: remove the convenience symlinks
  // (libobs/Headers/Resources at root, Versions/Current → A) and
  // leave only the real Versions/A/* tree. Required because
  // electron-builder's recursive copier walks targets alphabetically
  // and ensureSymlink calls on `Versions/Current/...` references
  // fail with ENOENT during the copy. libobs's install_name embeds
  // `@rpath/libobs.framework/Versions/A/libobs` directly, so the
  // canonical .framework symlink layout is unnecessary at runtime —
  // it's only useful at LINK time (-framework libobs).
  const fwRoot = path.join(distRoot, 'Frameworks', 'libobs.framework');
  if (fs.existsSync(fwRoot)) {
    for (const link of ['Headers', 'libobs', 'Resources']) {
      const p = path.join(fwRoot, link);
      try {
        const st = fs.lstatSync(p);
        if (st.isSymbolicLink()) {
          fs.unlinkSync(p);
        }
      } catch { /* not present, fine */ }
    }
    const currentLink = path.join(fwRoot, 'Versions', 'Current');
    try {
      const st = fs.lstatSync(currentLink);
      if (st.isSymbolicLink()) {
        fs.unlinkSync(currentLink);
        console.log('[dist] stripped libobs.framework/Versions/Current symlink');
      }
    } catch { /* not present, fine */ }
  }
} else {
  console.warn('[dist] platform not supported:', process.platform);
}


