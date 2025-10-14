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
  noobs.SetBuffering(true);
  noobs.SetRecordingDir(recordingPath);
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

  await new Promise((resolve) => setTimeout(resolve, 5000));

  const recordingNames = new Set();
  for (let i = 0; i < 1; i++) {
    console.log('Test Recording Loop:', i + 1);

    // Start the buffer.
    console.log('Starting buffer.');
    noobs.StartBuffer();
    await new Promise((resolve) => setTimeout(resolve, 5000));

    // Start the recording, with 1s offset into the past.
    console.log('Starting recording.');
    noobs.StartRecording(1);
    await new Promise((resolve) => setTimeout(resolve, 5000));

    // Add the source to the scene for a second.
    console.log('Adding source to scene...');
    noobs.AddSourceToScene('Test Source');
    await new Promise((resolve) => setTimeout(resolve, 1000));

    // Remove the source from the scene.
    console.log('Removing source from scene...');
    noobs.RemoveSourceFromScene('Test Source');
    await new Promise((resolve) => setTimeout(resolve, 1000));

    someGlobalState.shouldRebuffer = true;
    // Stop the recording.
    console.log('Stopping recording.');
    noobs.StopRecording();
    await new Promise((resolve) => setTimeout(resolve, 5000));

    console.log('Starting recording.');
    noobs.StartRecording(0);
    await new Promise((resolve) => setTimeout(resolve, 5000));
    noobs.StopRecording();

    // Get the path to the last recording.
    const last = noobs.GetLastRecording();
    recordingNames.add(last);
    if (recordingNames.size != i + 1) {
      console.log(recordingNames);
      throw new Error('Recording names not unique');
    }
    console.log('Last recording:', last);

    // Sleep a bit more for good measure.
    await new Promise((resolve) => setTimeout(resolve, 1000));
  }

  console.log('Stopping obs...');
  noobs.Shutdown();

  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
