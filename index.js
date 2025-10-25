const path = require('path');

process.env.Path = path
  .resolve(__dirname, 'dist', 'bin')
  .replace('app.asar', 'app.asar.unpacked');

const packageName = 'noobs.node';
const noobs = require(`./dist/${packageName}`);
module.exports = noobs;
