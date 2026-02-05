# Linux Testing Checklist for WoW Recorder

## Test Environment

- [ ] **OS**: Fedora 43
- [ ] **Desktop**: GNOME (Wayland)
- [ ] **WoW Version**: Retail via Heroic Launcher
- [ ] **OBS Version**: (run `obs --version`)
- [ ] **Audio System**: PipeWire

---

## 1. Application Startup

### 1.1 Clean Start
- [ ] App launches without errors
- [ ] No crash on startup
- [ ] UI displays correctly
- [ ] Tray icon appears

### 1.2 OBS Initialization
Check logs for:
- [ ] `[Recorder] OBS initialized successfully`
- [ ] No `obs-ffmpeg-mux` errors
- [ ] Audio encoder created (look for `FDK-AAC` or `AAC`)

### 1.3 PipeWire Screen Capture
- [ ] PipeWire portal dialog appears when configuring capture
- [ ] Can select WoW window
- [ ] Preview shows capture source

---

## 2. Audio Configuration

### 2.1 Audio Device Detection
- [ ] Output devices (speakers/headphones) detected
- [ ] Input devices (microphone) detected
- [ ] Volume meters respond to audio

### 2.2 Audio Recording
- [ ] Game audio recorded in video
- [ ] Microphone audio recorded (if enabled)
- [ ] No audio distortion or choppy audio
- [ ] Audio sync with video

---

## 3. Recording Tests

### 3.1 Raid Encounter Recording
- [ ] Pull a raid boss
- [ ] Recording starts automatically on `ENCOUNTER_START`
- [ ] Recording continues during fight
- [ ] Recording stops on `ENCOUNTER_END`
- [ ] Video saved with correct metadata

Verify in video:
- [ ] ~15 seconds pre-roll before pull
- [ ] Entire fight captured
- [ ] ~15 seconds overrun after kill
- [ ] Audio present and synced

### 3.2 Mythic+ Dungeon Recording
- [ ] Start a M+ key
- [ ] Recording starts on timer begin
- [ ] Recording continues through dungeon
- [ ] Recording stops on completion/abandon

### 3.3 Manual Recording (if enabled)
- [ ] Hotkey starts recording
- [ ] Hotkey stops recording
- [ ] Sound alerts play (if enabled)

---

## 4. Video Processing

### 4.1 Post-Processing
- [ ] Video queued for processing after recording
- [ ] FFmpeg processes without errors
- [ ] MKV converted to MP4
- [ ] Video appears in library

### 4.2 Video Playback
- [ ] Video plays in app
- [ ] Seeking works correctly
- [ ] Audio plays
- [ ] Death markers (if applicable) display at correct times

### 4.3 Video Metadata
- [ ] Encounter name correct
- [ ] Duration correct
- [ ] Result (kill/wipe) correct
- [ ] Player name captured

---

## 5. Edge Cases

### 5.1 Quick Wipe and Repull
- [ ] Wipe on boss
- [ ] First video saved correctly
- [ ] Repull immediately
- [ ] Second video saved correctly
- [ ] Both videos distinct

### 5.2 Long Fight
- [ ] Fight longer than 10 minutes
- [ ] Entire fight recorded
- [ ] No buffer issues
- [ ] Audio remains synced

### 5.3 App Backgrounded
- [ ] Minimize app
- [ ] Recording continues normally
- [ ] Bring app back to foreground
- [ ] Recording still works

### 5.4 WoW Client Restart
- [ ] Close WoW
- [ ] Buffer stops (check logs)
- [ ] Reopen WoW
- [ ] Buffer restarts
- [ ] New recording works

---

## 6. Error Handling

### 6.1 Missing Audio Encoder
- [ ] If FDK-AAC not installed, app logs warning
- [ ] Recording continues (video-only)

### 6.2 Screen Capture Permission Denied
- [ ] App shows appropriate error/guidance
- [ ] Can retry capture selection

### 6.3 Disk Space
- [ ] Low disk space warning (if applicable)

---

## 7. Performance

### 7.1 Resource Usage
During recording:
- [ ] CPU usage reasonable (<20% when idle)
- [ ] Memory usage stable (not growing)
- [ ] GPU encoder used (check logs for encoder type)

### 7.2 Game Performance
- [ ] No noticeable FPS drop in WoW while recording
- [ ] No audio crackling or stuttering

---

## Test Results Log

| Test | Pass/Fail | Notes |
|------|-----------|-------|
| 1.1 Clean Start | | |
| 1.2 OBS Init | | |
| 1.3 PipeWire | | |
| 2.1 Audio Devices | | |
| 2.2 Audio Recording | | |
| 3.1 Raid Recording | | |
| 3.2 M+ Recording | | |
| 4.1 Post-Processing | | |
| 4.2 Video Playback | | |
| 5.1 Quick Repull | | |
| 5.2 Long Fight | | |

---

## How to Run Tests

1. **Start the app in development mode:**
   ```bash
   cd wow-recorder
   npm run start
   ```

2. **Check logs in real-time:**
   The logs appear in the terminal where you started the app.

3. **Check log files:**
   ```bash
   cat ~/.config/WarcraftRecorder/logs/WarcraftRecorder-$(date +%Y-%m-%d).log
   ```

4. **Verify recordings:**
   Default storage path: `~/WarcraftRecorder/`

---

## Known Issues to Watch For

1. **Audio choppy on first recording**: May need to restart app after initial setup
2. **Buffer size warning**: Large buffer (20 min) may show warnings, can be ignored
3. **PipeWire dialog not appearing**: Restart app if capture source not selectable
