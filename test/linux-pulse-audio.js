const noobs = require('../index.js');
const path = require('path');

const iterations = 500;

async function stressTestPulseInput() {
  console.log('Starting PulseAudio input stress test');

  const cb = (msg) => {
    if (msg.type !== 'volmeter') {
      console.log('Callback received:', msg);
    }
  };

  const distPath = path.resolve(__dirname, '../dist');
  const logPath = path.resolve(__dirname, '../logs');
  const recordingPath = path.resolve(__dirname, '../recordings');

  console.log('Dist path:', distPath);
  console.log('Log path:', logPath);
  console.log('Recording path:', recordingPath);

  // init obs, set up config
  noobs.Init(distPath, logPath, cb);
  noobs.SetBuffering(true);
  noobs.SetRecordingCfg(recordingPath, 'mkv');
  
  const encoders = noobs.ListVideoEncoders();
  if (encoders.length > 0) {
    noobs.SetVideoEncoder(encoders[0], { keyint_sec: 1 });
  }

  // start buffering to add pressure on PA
  noobs.StartBuffer();
  await new Promise(r => setTimeout(r, 1000));
  noobs.SetVolmeterEnabled(true);

  console.log(`Running ${iterations} iterations`);

  let currentSources = [];

  for (let i = 0; i < iterations; i++) {
    // configureAudioSources
    for (const sourceName of currentSources) {
      noobs.RemoveSourceFromScene(sourceName);
      noobs.DeleteSource(sourceName);
    }
    currentSources = [];

    noobs.SetForceMono(true);
    noobs.SetAudioSuppression(true);

    // --- Pulse sources
    const sourceNameInput = `test_audio_${i}_input`;
    noobs.CreateSource(sourceNameInput, 'pulse_input_capture');
    
    // GetSourceSettings and GetSourceProperties are called to find device
    const inputSettings = noobs.GetSourceSettings(sourceNameInput);
    noobs.GetSourceProperties(sourceNameInput);
    
    // SetSourceSettings with device_id
    inputSettings['device_id'] = 'default';
    noobs.SetSourceSettings(sourceNameInput, inputSettings);
    
    // SetSourceVolume
    noobs.SetSourceVolume(sourceNameInput, 1.0);
    
    // AddSourceToScene
    noobs.AddSourceToScene(sourceNameInput);
    currentSources.push(sourceNameInput);

    // output
    const sourceNameOutput = `test_audio_${i}_output`;
    noobs.CreateSource(sourceNameOutput, 'pulse_output_capture');
    
    const outputSettings = noobs.GetSourceSettings(sourceNameOutput);
    noobs.GetSourceProperties(sourceNameOutput);
    
    outputSettings['device_id'] = 'default';
    noobs.SetSourceSettings(sourceNameOutput, outputSettings);
    noobs.SetSourceVolume(sourceNameOutput, 1.0);
    noobs.AddSourceToScene(sourceNameOutput);
    currentSources.push(sourceNameOutput);

    await new Promise(r => setTimeout(r, 50 + Math.random() * 50));
    
    if (i % 10 === 0) {
      console.log(`Iteration ${i}/${iterations}`);
    }
  }

  // cleanup after successful test
  console.log('Final cleanup...');
  for (const sourceName of currentSources) {
    noobs.RemoveSourceFromScene(sourceName);
    noobs.DeleteSource(sourceName);
  }

  console.log('Stress test completed.');
  
  noobs.SetVolmeterEnabled(false);
  noobs.StopRecording();
  await new Promise(r => setTimeout(r, 1000));
  
  noobs.Shutdown();
  console.log('Test done');
}

stressTestPulseInput().catch(err => {
  console.error('Test failed:', err);
  process.exit(1);
});
