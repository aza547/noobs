const path = require('path');

// Resolve platform-specific bin directory
const platformDir = process.platform === 'win32' ? 'win64' : 'linux';

// Path to the OBS binaries. We want this to come first so that the correct
// DLLs are loaded. It's possible other copies are on the system PATH already.
let binPath = path
  .resolve(__dirname, 'dist', 'bin', platformDir)
  .replace('app.asar', 'app.asar.unpacked');

if (process.platform === 'win32') {
  // Windows: prepend to PATH for DLL loading
  if (process.env.PATH) {
    binPath += ';';
    binPath += process.env.PATH;
  }
  // Set the updated PATH for this process.
  process.env.Path = binPath;
} else if (process.platform === 'linux') {
  // Linux: prepend to LD_LIBRARY_PATH for .so loading
  if (process.env.LD_LIBRARY_PATH) {
    binPath += ':';
    binPath += process.env.LD_LIBRARY_PATH;
  }
  process.env.LD_LIBRARY_PATH = binPath;
}

const packageName = 'noobs.node';
const noobs = require(`./dist/${packageName}`);
module.exports = noobs;