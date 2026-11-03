const noobs = require('../index.js');
const path = require('path');

process.on('uncaughtException', (err) => {
  console.log('Uncaught exception:', err);
});

const someGlobalState = {
  shouldRebuffer: false,
};

o = console.log;
console.log = (...args) => {
  o(`[${new Date().toISOString()}] ` + args[0], args[1]);
};

async function test() {
  console.log('Starting obs...');

  const cb = (msg) => {
    console.log('Callback received:', msg);
    if (someGlobalState.shouldRebuffer && msg.id === 'deactivate') {
      console.log('Starting buffer.');
      noobs.StartBuffer();
      someGlobalState.shouldRebuffer = false;
    }
  };

  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  noobs.Init(distPath, logPath, cb);
  noobs.SetBuffering(true, 55, 500);
  noobs.SetRecordingCfg(recordingPath, 'mp4');
  await new Promise((resolve) => setTimeout(resolve, 1000));

  console.log('Creating source...');
  noobs.CreateSource('Test Source', 'monitor_capture');

  const settings1 = noobs.GetSourceSettings('Test Source');
  console.log(settings1);
  noobs.SetSourceSettings('Test Source', { ...settings1, monitor: 1 });

  const settings2 = noobs.GetSourceSettings('Test Source');
  console.log(settings2);

  const properties = noobs.GetSourceProperties('Test Source');
  console.log('Source properties:', properties);
  console.log('Source properties:', properties[0].items);

  await new Promise((resolve) => setTimeout(resolve, 2000));

  // Start the buffer.
  console.log('Starting buffer.');
  noobs.StartBuffer();
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log('Replay buffer length 1 (sec):', noobs.GetReplayBufferLengthSec());
  await new Promise((resolve) => setTimeout(resolve, 1000));

  console.log('Replay buffer length 2 (sec):', noobs.GetReplayBufferLengthSec());
  await new Promise((resolve) => setTimeout(resolve, 1000));

  console.log('Replay buffer length 3 (sec):', noobs.GetReplayBufferLengthSec());
  await new Promise((resolve) => setTimeout(resolve, 1000));

  console.log('Stopping obs...');
  noobs.Shutdown();

  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
