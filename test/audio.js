const noobs = require('../index.js');
const path = require('path');

async function test() {
  console.log('Starting obs...');

  let volmeter_callbacks = 0;

  const cb = (msg) => {
    if (msg.type === 'volmeter') {
      volmeter_callbacks++;
    } else {
      console.log('Callback received:', msg);
    }
  };

  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  noobs.Init(distPath, logPath, cb);
  noobs.SetRecordingCfg(recordingPath, 'mp4');

  console.log('Creating source...');
// [INFO] 	- wasapi_input_capture
// [INFO] 	- wasapi_output_capture
// [INFO] 	- wasapi_process_output_capture
  noobs.CreateSource('Test Speaker', 'wasapi_output_capture');
  const s = noobs.GetSourceProperties('Test Speaker');
  console.log('Speakers:', s);
  console.log('Speakers:', s[0].items);

  noobs.CreateSource('Test Mic', 'wasapi_input_capture');
  const ms = noobs.GetSourceSettings('Test Mic');
  const mp = noobs.GetSourceProperties('Test Mic');
  console.log('ms:', ms);
  console.log('mp:', mp);
  console.log('Mics:', mp[0].items);

  // noobs.CreateSource('Test App', 'wasapi_process_output_capture');
  // const a = noobs.GetSourceProperties('Test App');
  // console.log('Apps:', a[0].items);

  noobs.AddSourceToScene('Test Mic');
  noobs.AddSourceToScene('Test Speaker');
  // noobs.AddSourceToScene('Test App');

  // Vary the volumes.
  noobs.SetSourceVolume('Test Mic', 0.25);
  noobs.SetSourceVolume('Test Speaker', 0.5);
  // noobs.SetSourceVolume('Test App', 0.75);

  // Enable the volmeter callback.
  noobs.SetVolmeterEnabled(true);

  // Make a token change to the settings.
  const ss = noobs.GetSourceSettings('Test Speaker');
  noobs.SetSourceSettings('Test Speaker', ss);

  noobs.StartRecording(0);
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log("Mute all audio inputs for 2 seconds...");
  noobs.SetMuteAudioInputs(true);
  await new Promise((resolve) => setTimeout(resolve, 2000));
  console.log("Unmute all audio inputs...");
  noobs.SetMuteAudioInputs(false);
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log("Set force mono")
  noobs.SetForceMono(true);
  noobs.CreateSource('Test Force Mono Source', 'wasapi_input_capture');
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log("Unset force mono")
  noobs.SetForceMono(false);
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log("Set Audio suppression")
  noobs.SetAudioSuppression(true);
  noobs.CreateSource('Test Audio Suppression Source', 'wasapi_input_capture');
  await new Promise((resolve) => setTimeout(resolve, 2000));

  console.log("Unset Audio suppression")
  noobs.SetAudioSuppression(false);
  await new Promise((resolve) => setTimeout(resolve, 2000));

  noobs.StopRecording();
  await new Promise((resolve) => setTimeout(resolve, 2000));
  
  console.log('Volmeter callbacks received:', volmeter_callbacks);
  noobs.Shutdown();
  console.log('Test Done');
}

console.log('Starting test...');
test();
console.log('Test now running async');
