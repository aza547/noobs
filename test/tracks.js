const noobs = require('../index.js');
const path = require('path');

async function test() {
  console.log('Starting obs...');

  const cb = () => {};
  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  noobs.Init(distPath, logPath, cb);
  noobs.SetBuffering(true);
  noobs.SetRecordingCfg(recordingPath, 'mp4');

  console.log('Creating source...');
// [INFO] 	- wasapi_input_capture
// [INFO] 	- wasapi_output_capture
// [INFO] 	- wasapi_process_output_capture
  noobs.CreateSource('Test Speaker', 'wasapi_output_capture');
  const s = noobs.GetSourceProperties('Test Speaker');
  console.log('Speakers:', s);
  console.log('Speakers:', s[0].items);

  const tracks = noobs.GetSourceAudioTracks('Test Speaker');
  console.log('Speaker audio tracks initially:', tracks);

  // Bitmask constants for audio tracks.
  const track1 = 1 << 0;
  const track2 = 1 << 1;
  const track3 = 1 << 2;
  const track4 = 1 << 3;
  const track5 = 1 << 4;
  const track6 = 1 << 5;

  // Set to track 1 only.
  noobs.SetSourceAudioTracks('Test Speaker', track1);
  const after1 = noobs.GetSourceAudioTracks('Test Speaker');
  console.log('Speaker audio tracks after setting track 1:', after1);

  if (after1 !== track1) {
    console.error('Error1: Audio tracks not set correctly!');
    return;
  }

  // Set to track 2 only.
  noobs.SetSourceAudioTracks('Test Speaker', track2);
  const after2 = noobs.GetSourceAudioTracks('Test Speaker');
  console.log('Speaker audio tracks after setting track 2:', after2);

  if (after2 !== track2) {
    console.error('Error2: Audio tracks not set correctly!');
    return;
  }

  // Set to track 3, 4, 5.
  noobs.SetSourceAudioTracks('Test Speaker', track3 | track4 | track5);
  const afterMulti = noobs.GetSourceAudioTracks('Test Speaker');
  console.log('Speaker audio tracks after setting multiple tracks:', afterMulti);

  if (afterMulti !== (track3 | track4 | track5)) {
    console.error('Error3: Audio tracks not set correctly!');
    return;
  }
  
  console.log('Sleep 2s...');
  noobs.StartBuffer();

  console.log('Start recording');
  noobs.StartRecording(0);
  console.log('Sleep 2s...');
  await new Promise((resolve) => setTimeout(resolve, 2000));

  // Change mid recording
  console.log('Change tracks while recording...');
  noobs.SetSourceAudioTracks('Test Speaker', track6);
  const after6 = noobs.GetSourceAudioTracks('Test Speaker');
  console.log('Speaker audio tracks after setting track 6:', after6);

  if (after6 !== track6) {
    console.error('Error4: Audio tracks not set correctly!');
    return;
  }

  console.log('Sleep 2s...');
  await new Promise((resolve) => setTimeout(resolve, 2000));
  console.log('Stop recording');
  noobs.StopRecording();

  noobs.Shutdown();
  console.log('Test Done');
}

console.log('Starting test...');
test();
