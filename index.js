const path = require('path');

// Resolve platform-specific paths
const platform = process.platform;  // 'win32' or 'linux'
const arch = process.arch;          // 'x64'
const binDir = platform === 'win32' ? 'win64' : 'linux';

// Path to the OBS binaries. We want this to come first so that the correct
// DLLs are loaded. It's possible other copies are on the system PATH already.
if (platform === 'win32') {
  let binPath = path
    .resolve(__dirname, 'dist', 'bin', binDir)
    .replace('app.asar', 'app.asar.unpacked');

  if (process.env.PATH) {
    binPath += ';';
    binPath += process.env.PATH;
  }

  process.env.Path = binPath;
}

// Load the platform-specific binary
const binaryName = `noobs-${platform}-${arch}.node`;
const binaryPath = path.resolve(__dirname, 'dist', binaryName)
  .replace('app.asar', 'app.asar.unpacked');

module.exports = require(binaryPath);