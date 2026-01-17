const path = require('path');

// Path to the OBS binaries. We want this to come first so that the correct
// DLLs are loaded. It's possible other copies are on the system PATH already.
let binPath = path
  .resolve(__dirname, 'dist', 'bin')
  .replace('app.asar', 'app.asar.unpacked');

// TODO: [linux-port] Set up shared library path for win32/linux
if (process.platform === 'win32') {
  // Windows: prepend to PATH for DLL loading
  if (process.env.PATH) {
    binPath += ';';
    binPath += process.env.PATH;
  }
  process.env.Path = binPath;
} else if (process.platform === 'linux') {
  // Linux: prepend to LD_LIBRARY_PATH for .so loading
  if (process.env.LD_LIBRARY_PATH) {
    binPath += ':';
    binPath += process.env.LD_LIBRARY_PATH;
  }
  process.env.LD_LIBRARY_PATH = binPath;
}
// TODO [linux-port] END

// Now set the updated PATH for this process.
process.env.Path = binPath;

const packageName = 'noobs.node';
const noobs = require(`./dist/${packageName}`);
module.exports = noobs;