const noobs = require('../index.js');
const path = require('path');

async function test() {
  console.log('Starting obs...');

  const cb = (msg) => {
    console.log('Callback received:', msg);
  };

  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  noobs.Init(distPath, logPath, cb);
  noobs.SetRecordingCfg(recordingPath, 0);

  console.log('Creating source...');
  const name = noobs.CreateSource('Test Source', 'monitor_capture');
  console.log("Created source with name", name);

  const settings1 = noobs.GetSourceSettings('Test Source');

  console.log(settings1);

  const p = noobs.GetSourceProperties('Test Source');
  console.log(p[1].items);

  // We need method 2 here which is WGC. We can't use 0 (Auto) or 1 (DXGI) unless we have a graphics context.
  // Or atlaest that's what ChatGPT thinks. This is only an issue when running as a CLI job. If we run in the context of an
  // electron process it just works fine.
  noobs.SetSourceSettings('Test Source', {
    ...settings1,
    monitor_id: p[1].items[1].value,
    method: 2,
  });

  const settings2 = noobs.GetSourceSettings('Test Source');
  console.log(settings2);

  console.log('Adding source to scene...');
  noobs.AddSourceToScene('Test Source');

  const recordingNames = new Set();
  for (let i = 0; i < 2; i++) {

    if (i > 0) {
      console.log('MKV Test');
      noobs.SetRecordingCfg(recordingPath, 1);
    } else {
      console.log('MP4 Test');
      noobs.SetRecordingCfg(recordingPath, 0);
    }

    // Start the recording, with 1s offset into the past.
    noobs.StartRecording();
    await new Promise((resolve) => setTimeout(resolve, 5000));

    // Stop the recording.
    noobs.StopRecording();
    await new Promise((resolve) => setTimeout(resolve, 1000));

    // Get the path to the last recording.
    const last = noobs.GetLastRecording();
    recordingNames.add(last);
    if (recordingNames.size != i + 1) {
      console.log(recordingNames);
      throw new Error('Recording names not unique');
    }

    console.log('Last recording:', last);
    if (last == '') {
      throw new Error(`No apparent recording`);
    }
    if (last.endsWith('noobs.mp4')) {
      throw new Error(`Last recording name not correct - ${last}`);
    }
  }

  console.log('Stopping obs...');
  noobs.Shutdown();

  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
