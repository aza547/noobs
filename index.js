const path = require('path');

// Path to the OBS binaries. We want this to come first so that the correct
// DLLs are loaded. It's possible other copies are on the system PATH already.
let binPath = path
  .resolve(__dirname, 'dist', 'bin')
  .replace('app.asar', 'app.asar.unpacked');

if (process.env.PATH) {
  // Almost certainly there is an existing PATH. We don't want to drop
  // things off the path as it might make other things fail.
  binPath += ';';
  binPath += process.env.PATH;
}

// Now set the updated PATH for this process.
process.env.Path = binPath;

const packageName = 'noobs.node';
const noobs = require(`./dist/${packageName}`);
module.exports = noobs;