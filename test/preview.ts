import noobs from '../index.js';
const path = require('path');

async function test() {
  console.log('Starting obs...');

  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');
  const cb = () => {};

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  noobs.Init(distPath, logPath, recordingPath, cb);

  // TODO - work out how to get a HWND to actually launch this. Maybe need some CPP code.
  // noobs.InitPreview(null); // Pass null for now, as we don't have a HWND yet.
  // noobs.ShowPreview(0, 0, 800, 600); // Show the preview at 800x600.
  const s = noobs.GetPreviewInfo();
  console.log('info:', s);

  noobs.Shutdown();
  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
