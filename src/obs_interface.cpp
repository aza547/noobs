#ifdef _WIN32
#include <windows.h>
#endif
#include <obs.h>
#include "utils.h"
#include "obs_interface.h"
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <util/platform.h>

void call_jscb(Napi::Env env, Napi::Function cb, SignalData* sd) {
  Napi::Object obj = Napi::Object::New(env);

  obj.Set("type", Napi::String::New(env, sd->type));
  obj.Set("id", Napi::String::New(env, sd->id));
  obj.Set("code", Napi::Number::New(env, sd->code));

  if (sd->value.has_value()) {
    obj.Set("value", Napi::Number::New(env, sd->value.value()));
  }

  if (sd->error.has_value()) {
    obj.Set("error", Napi::String::New(env, sd->error.value()));
  }

  cb.Call({ obj });
  delete sd;
}

void ObsInterface::list_encoders(obs_encoder_type type)
{
  blog(LOG_INFO, "Encoders:");
  size_t idx = 0;
  const char *encoder_type;

  while (obs_enum_encoder_types(idx++, &encoder_type)) {
    blog(LOG_INFO, "\t- %s (%s)", encoder_type, obs_encoder_get_display_name(encoder_type));
  }
};

void ObsInterface::list_source_types()
{
  blog(LOG_INFO, "Sources:");
  size_t idx = 0;
  const char *src = nullptr;

  while (obs_enum_source_types(idx++, &src)) {
    blog(LOG_INFO, "\t- %s", src);
  }
}

void ObsInterface::list_output_types()
{
  blog(LOG_INFO, "Outputs:");
  size_t idx = 0;
  const char *src = nullptr;

  while (obs_enum_output_types(idx++, &src)) {
    blog(LOG_INFO, "\t- %s", src);
  }
}

void ObsInterface::load_module(const char* module, const char* data, bool allowFail) {
  blog(LOG_INFO, "Loading module: %s", module);
  blog(LOG_INFO, "Data path: %s", data);
  blog(LOG_INFO, "Allow fail: %d", allowFail);

  obs_module_t *ptr = NULL;
  int openmod = obs_open_module(&ptr, module, data);

  if (openmod != MODULE_SUCCESS) {
    blog(LOG_ERROR, "Failed to open module: %s, %d", module, openmod);
    throw std::runtime_error("Failed to open module!");
  }

  bool initmod = obs_init_module(ptr);

  if (initmod) {
    blog(LOG_INFO, "Module initialized successfully!");
  } else if (allowFail) {
    blog(LOG_INFO, "Module initialization failed, but allowed to fail: %s", module);
  } else {
    blog(LOG_ERROR, "Failed to initialize module: %s", module);
    throw std::runtime_error("Module initialization failed!");
  }
}

void ObsInterface::setVideoContext(int fps, int width, int height) {
  blog(LOG_INFO, "Reset video context");

  blog(LOG_INFO, "FPS: %d", fps);
  blog(LOG_INFO, "Width: %d", width);
  blog(LOG_INFO, "Height: %d", height);

  if (fps <= 10) {
    blog(LOG_WARNING, "Invalid FPS provided for reset, using default 10");
    fps = 60;
  }

  if (width <= 32 || height <= 32) {
    blog(LOG_WARNING, "Invalid width or height provided for reset, using default 1920x1080");
    width = 1920;
    height = 1080;
  }

  int ret = reset_video(fps, width, height);

  if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
    blog(LOG_WARNING, "Can't reset video as currently active");
    return;
  }

  if (ret != OBS_VIDEO_SUCCESS) {
    blog(LOG_ERROR, "Failed to reset video context: %d", ret);
    throw std::runtime_error("Failed to reset video context");
  }

  // Recreate the encoders as they are tied to the video context.
  create_video_encoders();
}

int ObsInterface::reset_video(int fps, int width, int height) {
  blog(LOG_INFO, "Reset video");
  obs_video_info ovi = {};

  ovi.base_width = width;
  ovi.base_height = height;
  ovi.output_width = width;
  ovi.output_height = height;
  ovi.fps_num = fps;
  ovi.fps_den = 1;

  ovi.output_format = VIDEO_FORMAT_NV12;
  ovi.colorspace = VIDEO_CS_709;
  ovi.range = VIDEO_RANGE_PARTIAL;
  ovi.scale_type = OBS_SCALE_BILINEAR;
  ovi.adapter = 0;
  ovi.gpu_conversion = true;
#ifdef _WIN32
  ovi.graphics_module = "libobs-d3d11.dll";
#else
  ovi.graphics_module = "libobs-opengl.so";
#endif 

  int rc = obs_reset_video(&ovi);

  if (rc == OBS_VIDEO_SUCCESS) {
    // Without this HDR doesn't work.
    obs_set_video_levels(300.0f, 1000.0f); 
  }

  return rc;
}

bool ObsInterface::reset_audio() {
  struct obs_audio_info oai = {0};
  oai.samples_per_sec = 48000;
  oai.speakers = SPEAKERS_STEREO;
  return obs_reset_audio(&oai);
}

void ObsInterface::init_obs(const std::string& distPath) {
  blog(LOG_INFO, "Initializing OBS");
  auto success = obs_startup("en-US", NULL, NULL);

  if (!success) {
    blog(LOG_ERROR, "Failed to start OBS!");
    throw std::runtime_error("OBS startup failed");
  }

  if (!obs_initialized()) {
    blog(LOG_ERROR, "OBS not initialized!");
    throw std::runtime_error("OBS initialization failed");
  }

#ifdef _WIN32
  std::string basePath = distPath;

  if (basePath.back() != '/' && basePath.back() != '\\') {
    basePath += '/';
  }

  std::string effectsPath = basePath + "data/effects/";
  std::string pluginPath = basePath + "obs-plugins/";
  std::string pluginDataPath = basePath + "data/obs-plugins/";
#else
  (void)distPath;
  std::string effectsPath = "/usr/share/obs/libobs/";
  std::string pluginDataPath = "/usr/share/obs/obs-plugins/";

  // Plugin library path varies by distro.
  std::string pluginPath;
  const char* pluginCandidates[] = {
    "/usr/lib64/obs-plugins/",
    "/usr/lib/x86_64-linux-gnu/obs-plugins/",
    "/usr/lib/obs-plugins/",
    nullptr
  };

  for (int i = 0; pluginCandidates[i]; i++) {
    struct stat st;
    if (stat(pluginCandidates[i], &st) == 0 && S_ISDIR(st.st_mode)) {
      pluginPath = pluginCandidates[i];
      break;
    }
  }

  if (pluginPath.empty()) {
    blog(LOG_WARNING, "OBS plugin directory not found, falling back to /usr/lib64/obs-plugins/");
    pluginPath = "/usr/lib64/obs-plugins/";
  }
#endif

  blog(LOG_INFO, "Effects path: %s", effectsPath.c_str());
  blog(LOG_INFO, "Plugin path: %s", pluginPath.c_str());
  blog(LOG_INFO, "Plugin data path: %s", pluginDataPath.c_str());

  // Add the effects path. We need this before resetting video and audio
  // to ensure the effects are available. The function is deprecated in
  // libobs but it works for now.
  obs_add_data_path(effectsPath.c_str());

  // This must come before loading modules to initialize D3D11.
  // Choose some sensible defaults that can be reconfigured.
  int rc = reset_video(60, 1920, 1080);

  if (rc != OBS_VIDEO_SUCCESS) {
    blog(LOG_ERROR, "Failed to reset video!");
    throw std::runtime_error("Failed to reset video!");
  }

  if (!reset_audio()) {
    blog(LOG_ERROR, "Failed to reset audio!");
    throw std::runtime_error("Failed to reset audio!");
  }

#ifdef _WIN32
  std::vector<std::string> modules = {
    "obs-x264",     // Software encoder.
    "obs-ffmpeg",   // Contains AMF (AMD) encoder support.
    "win-capture",  // Required for basically all forms of capture on Windows.
    "image-source", // Required for image sources.
    "win-wasapi",   // Required for WASAPI audio input.
    "obs-nvenc",    // Required for NVENC video encoding.
    "obs-qsv11",    // Required for QSV video encoding.
    "obs-filters"   // Required for audio filters.
  };
  std::string moduleExt = ".dll";
#else
  std::vector<std::string> modules = {
    "obs-x264",           // Software encoder.
    "obs-ffmpeg",         // Contains VAAPI/AMD encoder support and muxers.
    "obs-nvenc",          // NVIDIA NVENC hardware encoder.
    "linux-capture",      // Required for capture on Linux (X11).
    "linux-pipewire",     // Required for PipeWire screen capture (Wayland/X11).
    "image-source",       // Required for image sources.
    "linux-pulseaudio",   // Required for PulseAudio audio input.
    "obs-filters",        // Required for audio filters.
    "obs-libfdk"          // FDK-AAC audio encoder, avoids FFmpeg ABI issues on Linux.
  };
  std::string moduleExt = ".so";
#endif

  for (const auto& module : modules) {
    std::string modulePath = pluginPath + module + moduleExt;
    std::string moduleDataPath = pluginDataPath + module;

    // NVENC/QSV fail if there is no hardware support.
    // On Linux, capture plugins may fail depending on display server setup.
    // obs-libfdk may not be installed on all distributions.
    bool allowFail = (module == "obs-nvenc") || (module == "obs-qsv11") ||
                     (module == "linux-capture") || (module == "linux-pipewire") ||
                     (module == "obs-libfdk");
    load_module(modulePath.c_str(), moduleDataPath.c_str(), allowFail);
  }

  obs_post_load_modules();
#ifdef _WIN32
  register_preview_window_class();
#endif

  list_encoders();
  list_source_types();
  list_output_types();

  blog(LOG_INFO, "Initializing complete");
}


void ObsInterface::create_output() {
  blog(LOG_INFO, "Create outputs");

  const char* type = buffering ? "replay_buffer" : "ffmpeg_muxer";
  const char* name = buffering ? "Buffer Output" : "File Output";

  if (output) {
    blog(LOG_DEBUG, "Releasing existing output");
    obs_output_release(output);
  }

  blog(LOG_INFO, "Creating replay buffer output");
  output = obs_output_create(type, name, NULL, NULL);

  if (!output) {
    blog(LOG_ERROR, "Failed to create output!");
    throw std::runtime_error("Failed to create output!");
  }

  obs_data_t *settings = obs_data_create();

  if (buffering) {
    blog(LOG_INFO, "Set replay buffer settings");
#ifdef _WIN32
    obs_data_set_int(settings, "max_time_sec", 60);
    obs_data_set_int(settings, "max_size_mb", 1024);
#else
    // Larger buffer since we save at encounter end on Linux.
    obs_data_set_int(settings, "max_time_sec", 1200);
    obs_data_set_int(settings, "max_size_mb", 4096);
#endif
    obs_data_set_string(settings, "directory", recording_path.c_str());
    obs_data_set_string(settings, "format", "%CCYY-%MM-%DD %hh-%mm-%ss");
    obs_data_set_string(settings, "extension", file_extension.c_str());
  } else {
    blog(LOG_INFO, "Set ffmpeg_muxer settings");
    // Need to specify the exact path for ffmpeg_muxer. We will write this again at start recording.
#ifdef _WIN32
    std::string pathSep = "\\";
#else
    std::string pathSep = "/";
#endif
    std::string filename = recording_path + pathSep + get_current_date_time() + "." + file_extension;
    obs_data_set_string(settings, "path", filename.c_str());
    unbuffered_output_filename = filename;
  }

  obs_output_update(output, settings);
  obs_data_release(settings);
  connect_signal_handlers(output);
}

void ObsInterface::setRecordingCfg(
  const std::string& recordingPath, 
  const std::string& fileExtension
) {
  blog(LOG_INFO, "Set recording config. Path: %s. Ext %s", 
    recordingPath.c_str(), 
    fileExtension.c_str()
  );

  if (obs_output_active(output)) {
    blog(LOG_ERROR, "Output is active, cannot update recording path");
    throw std::runtime_error("Output is active, cannot update recording path");
  }

  recording_path = recordingPath;
  file_extension = fileExtension;
  create_output();

  create_video_encoders();
  create_audio_encoders();
}


void ObsInterface::create_video_encoders() {
  blog(LOG_INFO, "Set video encoder: %s", video_encoder_id.c_str());

  if (video_encoder) {
    blog(LOG_DEBUG, "Releasing file video encoder");
    obs_encoder_release(video_encoder);
    video_encoder = nullptr;
  }

  video_encoder = obs_video_encoder_create(
    video_encoder_id.c_str(), 
    "noobs_file_encoder", 
    video_encoder_settings, 
    NULL
  );

  if (!video_encoder) {
    blog(LOG_ERROR, "Failed to create video encoder!");
    throw std::runtime_error("Failed to create video encoder!");
  }

  obs_output_set_video_encoder(output, video_encoder);
  obs_encoder_set_video(video_encoder, obs_get_video());
}

void ObsInterface::create_audio_encoders() {
  blog(LOG_INFO, "Create audio encoder");

  if (audio_encoder) {
    blog(LOG_DEBUG, "Releasing audio encoder");
    obs_encoder_release(audio_encoder);
    audio_encoder = nullptr;
  }

  // Create encoder settings with explicit sample rate
  obs_data_t *enc_settings = obs_data_create();
  obs_data_set_int(enc_settings, "bitrate", 128);
  obs_data_set_int(enc_settings, "samplerate", 48000);

#ifndef _WIN32
  // Prefer FDK-AAC on Linux to avoid FFmpeg ABI incompatibilities.
  // Falls back to ffmpeg_aac or ffmpeg_opus if unavailable.
  blog(LOG_INFO, "Linux: Trying FDK-AAC encoder");
  audio_encoder = obs_audio_encoder_create(
    "libfdk_aac",
    "fdk_file",
    enc_settings,
    0,
    NULL
  );

  if (audio_encoder) {
    blog(LOG_INFO, "FDK-AAC encoder created successfully");
  } else {
    blog(LOG_INFO, "FDK-AAC not available, trying FFmpeg AAC");
  }
#endif

  if (!audio_encoder) {
    audio_encoder = obs_audio_encoder_create(
      "ffmpeg_aac",
      "aac_file",
      enc_settings,
      0,
      NULL
    );
    if (audio_encoder) {
      blog(LOG_INFO, "FFmpeg AAC encoder created successfully");
    }
  }

  if (!audio_encoder) {
    blog(LOG_INFO, "AAC encoder not available, trying Opus");
    audio_encoder = obs_audio_encoder_create(
      "ffmpeg_opus",
      "opus_file",
      enc_settings,
      0,
      NULL
    );
    if (audio_encoder) {
      blog(LOG_INFO, "FFmpeg Opus encoder created successfully");
    }
  }

  obs_data_release(enc_settings);

  if (!audio_encoder) {
    blog(LOG_WARNING, "No audio encoder available - recording will be video-only!");
    blog(LOG_WARNING, "On Linux, try installing: fdk-aac-free (Fedora) or libfdk-aac2 (Debian/Ubuntu)");
    audio_disabled = true;
    return;
  }

  audio_disabled = false;
  blog(LOG_INFO, "Audio encoder created successfully");

  obs_output_set_audio_encoder(output, audio_encoder, 0);
  obs_encoder_set_audio(audio_encoder, obs_get_audio());
}

void ObsInterface::create_scene() {
  blog(LOG_INFO, "Create scene");
  scene = obs_scene_create("Base Scene");

  if (!scene) {
    blog(LOG_ERROR, "Failed to create scene!");
    throw std::runtime_error("Failed to create scene!");
  }

  obs_source_t *scene_source = obs_scene_get_source(scene);

  if (!scene_source) {
    blog(LOG_ERROR, "Failed to get scene source!");
    throw std::runtime_error("Failed to get scene source!");
  }

  obs_set_output_source(0, scene_source); // 0 = video track
}

void ObsInterface::volmeter_callback(void *data, 
  const float magnitude[MAX_AUDIO_CHANNELS],
  const float peak[MAX_AUDIO_CHANNELS], 
  const float inputPeak[MAX_AUDIO_CHANNELS])
{
  // blog(LOG_DEBUG, "Volmeter callback triggered: %f %f %f", 
  //   obs_db_to_mul(magnitude[0]), 
  //   obs_db_to_mul(peak[0]), 
  //   obs_db_to_mul(inputPeak[0])
  // );

  SignalContext* ctx = static_cast<SignalContext*>(data);
  ObsInterface* self = ctx->self;

  if (!self->volmeter_enabled) {
    return;
  }

  SignalData* sd = new SignalData{ "volmeter", ctx->id.c_str(), 0, obs_db_to_mul(peak[0]) };
  self->jscb.NonBlockingCall(sd, call_jscb);
}

std::string ObsInterface::createSource(std::string name, std::string type) {
  blog(LOG_INFO, "Create source: %s of type %s", name.c_str(), type.c_str());

  obs_source_t *source = obs_source_create(
    type.c_str(), // Type of source, e.g. "wasapi_input_capture"
    name.c_str(), // Name of the source, e.g. "My Audio Input"
    NULL, // No settings.
    NULL  // No hotkey data.
  );

  if (!source) {
    blog(LOG_ERROR, "Failed to create source: %s", name.c_str());
    throw std::runtime_error("Failed to create source!");
  }

  // The name might not match what we asked for if there is a duplicate.
  // So pass it back to the client to avoid potential for a mismatch.
  std::string real_name = obs_source_get_name(source);

  if (type == AUDIO_OUTPUT || type == AUDIO_INPUT || type == AUDIO_PROCESS) {
    blog(LOG_INFO, "Creating volmeter for source: %s", real_name.c_str());

    obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_CUBIC);
    obs_volmeter_attach_source(volmeter, source);

    SignalContext* ctx = new SignalContext{ this, real_name };
    obs_volmeter_add_callback(volmeter, volmeter_callback, ctx);

    // Store the volmeter in the volmeters map.
    volmeters[real_name] = volmeter;
    volmeter_cb_ctx[real_name] = ctx; // Track this so we can free it later.
  }

  if (type == AUDIO_INPUT && force_mono) {
    blog(LOG_INFO, "Setting force mono for new source: %s", real_name.c_str());
    uint32_t flags = obs_source_get_flags(source);
    obs_source_set_flags(source, flags | OBS_SOURCE_FLAG_FORCE_MONO);
  }

  if (type == AUDIO_INPUT && audio_suppression) {
    blog(LOG_INFO, "Setting up filter for new source: %s", real_name.c_str());
    std::string filter_name = "Filter for " + real_name;


    obs_source_t *filter = obs_source_create(
      "noise_suppress_filter_v2",   
      filter_name.c_str(),   
      nullptr, // Defaults are sensible.
      nullptr
    );

    if (!filter) {
      blog(LOG_ERROR, "Failed to create filter for source: %s", real_name.c_str());
      throw std::runtime_error("Failed to create filter!");
    }

    filters[real_name] = filter;
    obs_source_filter_add(source, filter);
  }

  // Store the source in the sources map.
  sources[real_name] = source;

  // Store the dimensions so we can fire a callback if they change.
  uint32_t w = obs_source_get_width(source);
  uint32_t h = obs_source_get_height(source);
  sizes[real_name] = { w, h };

  return real_name;
}

void ObsInterface::deleteSource(std::string name) {
  blog(LOG_INFO, "Delete source: %s", name.c_str());

  // First release a volmeter if there is one present.
  // Only audio sources have volmeters ofcourse.
  auto vol_it = volmeters.find(name);
  
  if (vol_it != volmeters.end()) {
    obs_volmeter_t* volmeter = vol_it->second;
    obs_volmeter_remove_callback(volmeter, volmeter_callback, this);
    obs_volmeter_detach_source(volmeter);
    obs_volmeter_destroy(volmeter);
    blog(LOG_INFO, "Volmeter deleted for source: %s", name.c_str());
    volmeters.erase(name);
  }

  // Now deal with the callback context.
  auto ctx_it = volmeter_cb_ctx.find(name);

  if (ctx_it != volmeter_cb_ctx.end()) {
    SignalContext* ctx = ctx_it->second;
    delete ctx;
    volmeter_cb_ctx.erase(ctx_it);
  }

  // Now deal with the source itself.
  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when deleting", name.c_str());
    return;
  }

  obs_source_t* source = it->second;

  // Remove and release any filters.
  auto filter_it = filters.find(name);
  
  if (filter_it != filters.end()) {
    obs_source_t* filter = filter_it->second;
    obs_source_filter_remove(source, filter);
    obs_source_release(filter);
    filters.erase(name);
    blog(LOG_INFO, "Filter deleted for source: %s", name.c_str());
  }

  obs_source_remove(source); // ???
  obs_source_release(source);
  sources.erase(name);
  sizes.erase(name);
  blog(LOG_INFO, "Source deleted: %s", name.c_str());
}

obs_data_t* ObsInterface::getSourceSettings(std::string name) {
  blog(LOG_INFO, "Get source settings for: %s", name.c_str());

  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when getting settings", name.c_str());
    throw std::runtime_error("Source not found!");
  }

  obs_source_t* source = it->second;
  obs_data_t *settings = obs_source_get_settings(source);
  
  if (!settings) {
    blog(LOG_ERROR, "Failed to get settings for source: %s", name.c_str());
    throw std::runtime_error("Failed to get source settings!");
  }

  return settings;
}

void ObsInterface::setSourceSettings(std::string name, obs_data_t* settings) {
  blog(LOG_INFO, "Set source settings for: %s", name.c_str());
  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when setting settings", name.c_str());
    throw std::runtime_error("Source not found!");
  }

  obs_source_t* source = it->second;
  obs_source_update(source, settings);

  // If this is an audio source, it may have an attached volmeter.
  auto vol_it = volmeters.find(name);
  
  if (vol_it != volmeters.end()) {
    // Rebind it. This avoids leaving it attached to stale audio stream
    // in the event of a device change.
    blog(LOG_INFO, "Rebinding volmeter for source: %s", name.c_str());
    obs_volmeter_t* volmeter = vol_it->second;
    obs_volmeter_attach_source(volmeter, source);

    // Flush the volmeter: send a zero signal in-case it never triggers any
    // more callbacks. That can happen on selecting a device with no audio.
    zeroVolmeter(name);
  }
}

obs_properties_t* ObsInterface::getSourceProperties(std::string name) {
  blog(LOG_INFO, "Get source properties for: %s", name.c_str());
  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when getting properties", name.c_str());
    throw std::runtime_error("Source not found!");
  }

  obs_source_t* source = it->second;
  obs_properties_t *props = obs_source_properties(source);

  if (!props) {
    blog(LOG_ERROR, "Failed to get properties for source: %s", name.c_str());
    throw std::runtime_error("Failed to get source properties!");
  }

  return props;
}

SourceSize ObsInterface::getSourceDimensions(std::string name) {
  blog(LOG_INFO, "Get source dimensions for: %s", name.c_str());
  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when getting dimensions", name.c_str());
    throw std::runtime_error("Source not found!");
  }

  obs_source_t* source = it->second;
  uint32_t width = obs_source_get_width(source);
  uint32_t height = obs_source_get_height(source);

  blog(LOG_INFO, "Source %s dimensions: %dx%d", name.c_str(), width, height);
  return { width, height };
}

void ObsInterface::output_signal_handler(void *data, calldata_t *cd) {
  long long code = calldata_int(cd, "code");
  const char *err = calldata_string(cd, "last_error");

  std::optional<std::string> error;

  if (err) {
    error = std::string(err);
  }

  SignalContext* ctx = static_cast<SignalContext*>(data);
  ObsInterface* self = ctx->self;

  SignalData* sd = new SignalData{ 
    "output", 
    ctx->id.c_str(), 
    code, 
    std::nullopt, // No value, that's only used for volmeters.
    error,
  };

  self->jscb.NonBlockingCall(sd, call_jscb);
}

void ObsInterface::connect_signal_handlers(obs_output_t *output) {
  signal_handler_t *sh = obs_output_get_signal_handler(output);
  signal_handler_connect(sh, "start", output_signal_handler,  start_ctx);
  signal_handler_connect(sh, "starting", output_signal_handler,  starting_ctx);
  signal_handler_connect(sh, "stopping", output_signal_handler,  stopping_ctx);
  signal_handler_connect(sh, "stop", output_signal_handler,  stop_ctx);
  signal_handler_connect(sh, "activate", output_signal_handler, activate_ctx);
  signal_handler_connect(sh, "deactivate", output_signal_handler, deactivate_ctx);
}

void ObsInterface::disconnect_signal_handlers(obs_output_t *output) {
  signal_handler_t *sh = obs_output_get_signal_handler(output);
  signal_handler_disconnect(sh, "starting", output_signal_handler,  starting_ctx);
  signal_handler_disconnect(sh, "start", output_signal_handler,  start_ctx);
  signal_handler_disconnect(sh, "stopping", output_signal_handler,  stopping_ctx);
  signal_handler_disconnect(sh, "stop", output_signal_handler,  stop_ctx);
  signal_handler_disconnect(sh, "activate", output_signal_handler, activate_ctx);
  signal_handler_disconnect(sh, "deactivate ", output_signal_handler, deactivate_ctx);
}

bool draw_source_outline(obs_scene_t *scene, obs_sceneitem_t *item, void *p) {
  // Get the item position and size
  vec2 pos; vec2 scale; obs_sceneitem_crop crop;
  obs_sceneitem_get_pos(item, &pos);
  obs_sceneitem_get_scale(item, &scale);
  obs_sceneitem_get_crop(item, &crop);

  // Calculate actual size, accounting for scaling and cropping.
  obs_source_t *src = obs_sceneitem_get_source(item);
  float width =  (obs_source_get_width(src) - crop.left - crop.right) * scale.x;
  float height = (obs_source_get_height(src) - crop.top - crop.bottom) * scale.y;

  if (width <= 0 || height <= 0) {
    // Don't want to call gs_draw_sprite with zero width or height.
    // It is obviously nonsense and leads to log spam. Just return early.
    return true;
  }

  // Draw rectangle around the source using the position and size
  gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
  gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
  gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

  vec4 col = {0.733f, 0.267f, 0.125f, 1.0f}; // #BB4420
  gs_effect_set_vec4(color, &col);

  gs_technique_begin(tech);
  gs_technique_begin_pass(tech, 0);

  gs_matrix_push();
  gs_matrix_identity();

  // Top border
  gs_matrix_push();
  gs_matrix_translate3f(pos.x, pos.y, 0.0f);
  gs_draw_sprite(nullptr, 0, width, 4.0f);
  gs_matrix_pop();

  // Bottom border
  gs_matrix_push();
  gs_matrix_translate3f(pos.x, pos.y  + height - 4.0f, 0.0f);
  gs_draw_sprite(nullptr, 0, width, 4.0f);
  gs_matrix_pop();

  // Left border
  gs_matrix_push();
  gs_matrix_translate3f(pos.x, pos.y, 0.0f);
  gs_draw_sprite(nullptr, 0, 4.0f, height);
  gs_matrix_pop();

  // Right border
  gs_matrix_push();
  gs_matrix_translate3f(pos.x + width - 4.0f, pos.y, 0.0f);
  gs_draw_sprite(nullptr, 0, 4.0f, height);
  gs_matrix_pop();

  // Dragging point box (25x25 pixels in bottom-right corner)
  gs_matrix_push();
  gs_matrix_translate3f(pos.x + width - 25.0f, pos.y  + height - 25.0f, 0.0f);
  gs_draw_sprite(nullptr, 0, 25.0f, 25.0f);
  gs_matrix_pop();

  gs_matrix_pop();

  gs_technique_end_pass(tech);
  gs_technique_end(tech);

  return true;
}

void draw_callback(void* data, uint32_t cx, uint32_t cy) {
  ObsInterface* obsInterface = (ObsInterface*)data;

  obs_video_info ovi;
  obs_get_video_info(&ovi);

  float scaleX = float(cx) / float(ovi.base_width);
  float scaleY = float(cy) / float(ovi.base_height);

  float previewScale;

  // Pick the limiting scale factor.
  if (scaleX < scaleY) {
    previewScale = scaleX;
  } else {
    previewScale = scaleY;
  }

  int previewCX = int(previewScale * ovi.base_width);
  int previewCY = int(previewScale * ovi.base_height);
  int previewX = (cx - previewCX) / 2;
  int previewY = (cy - previewCY) / 2;

  gs_viewport_push();
	gs_projection_push();

  gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
  gs_set_viewport(previewX, previewY, previewCX, previewCY);

  // Renders the scene now the graphics context is setup.
  // obs_render_main_texture();
  obs_source_t *source = obs_scene_get_source(obsInterface->scene);
  if (source)
    obs_source_video_render(source);

  // Draw boxes around sources, if enabled.
  if (obsInterface->getDrawSourceOutlineEnabled()) {
    obs_scene_t* scene = obs_get_scene_by_name("Base Scene");
    obs_scene_enum_items(scene, draw_source_outline, NULL);
    obs_scene_release(scene);
  }

	gs_projection_pop();
	gs_viewport_pop();

  // Iterate over the sources and check for changes to size.
  for (const auto& [name, source] : obsInterface->sources) {
    SourceSize last = obsInterface->sizes[name];

    uint32_t w = obs_source_get_width(source);
    uint32_t h = obs_source_get_height(source);

    if (w != last.width || h != last.height) {
      blog(LOG_INFO, "Source %s changed size from (%d x %d) to (%d x %d)",
            name.c_str(), last.width, last.height, w, h);
      obsInterface->sourceCallback(name);
      obsInterface->sizes[name] = { w, h };
    }
  }
}

void ObsInterface::initPreview(NativeWindowHandle parent) {
  blog(LOG_INFO, "ObsInterface::initPreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    blog(LOG_INFO, "Creating preview child window");

    preview_hwnd = CreateWindowEx(
      0,
      TEXT("PreviewWindowClass"),   // Window class we already registered earlier
      TEXT("OBS Preview"),          // Window name
      WS_POPUP,
      0, 0,                   // Initial position (x, y)
      0, 0,                   // Initial size (width, height)
      NULL,                   // No parent yet
      NULL,                   // No menu
      GetModuleHandle(NULL),
      NULL
    );

    if (!preview_hwnd) {
      blog(LOG_ERROR, "Failed to create preview child window");
      return;
    }

    SetParent(preview_hwnd, parent);

    LONG_PTR style = GetWindowLongPtr(preview_hwnd, GWL_STYLE);
    style &= ~WS_POPUP;
    style |= WS_CHILD;
    SetWindowLongPtr(preview_hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(preview_hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TRANSPARENT;
    SetWindowLongPtr(preview_hwnd, GWL_EXSTYLE, exStyle);
  }

  if (!display) {
    blog(LOG_INFO, "Create OBS display in child window");

    gs_init_data gs_data = {};
    gs_data.adapter = 0;
    gs_data.cx = 1920; // Gets overwritten when we call configurePreview().
    gs_data.cy = 1080; // Gets overwritten when we call configurePreview().
    gs_data.format = GS_BGRA;
    gs_data.zsformat = GS_ZS_NONE;
    gs_data.num_backbuffers = 1;
    gs_data.window.hwnd = preview_hwnd;

    display = obs_display_create(&gs_data, 0x0);

    if (!display) {
      blog(LOG_ERROR, "Failed to create OBS display");
      return;
    }

    obs_display_add_draw_callback(display, draw_callback, this);
  }

  obs_display_set_enabled(display, false);
#else
  blog(LOG_INFO, "Initializing Linux preview with parent window: %u", parent);

  if (parent == 0) {
    blog(LOG_ERROR, "Invalid parent window ID");
    return;
  }

  // Detect display server type
  display_server = detectDisplayServer();

  switch (display_server) {
    case LinuxDisplayServer::X11:
      blog(LOG_INFO, "Using X11 display server");
      if (!initX11Preview(parent)) {
        blog(LOG_ERROR, "Failed to initialize X11 preview");
        return;
      }
      break;

    case LinuxDisplayServer::XWAYLAND:
      blog(LOG_INFO, "Using XWayland (Wayland session with X11 compatibility)");
      // XWayland provides X11 compatibility, so we can use the X11 path
      if (!initX11Preview(parent)) {
        blog(LOG_ERROR, "Failed to initialize XWayland preview");
        return;
      }
      break;

    case LinuxDisplayServer::WAYLAND:
      blog(LOG_WARNING, "Pure Wayland detected, attempting XWayland fallback");
      if (!initX11Preview(parent)) {
        blog(LOG_ERROR, "Preview not available on pure Wayland without XWayland");
        blog(LOG_INFO, "Consider running the app with: GDK_BACKEND=x11 or enabling XWayland");
        return;
      }
      break;

    default:
      blog(LOG_WARNING, "Unknown display server, attempting X11");
      if (!initX11Preview(parent)) {
        blog(LOG_ERROR, "Failed to initialize preview");
        return;
      }
      break;
  }

  obs_display_set_enabled(display, false);
#endif
}

#ifndef _WIN32
LinuxDisplayServer ObsInterface::detectDisplayServer() {
  // Check XDG_SESSION_TYPE first (most reliable on modern systems)
  const char* session_type = std::getenv("XDG_SESSION_TYPE");
  const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
  const char* x11_display_env = std::getenv("DISPLAY");

  blog(LOG_INFO, "Display server detection:");
  blog(LOG_INFO, "  XDG_SESSION_TYPE: %s", session_type ? session_type : "(not set)");
  blog(LOG_INFO, "  WAYLAND_DISPLAY: %s", wayland_display ? wayland_display : "(not set)");
  blog(LOG_INFO, "  DISPLAY: %s", x11_display_env ? x11_display_env : "(not set)");

  if (session_type) {
    if (strcmp(session_type, "wayland") == 0) {
      // Running on Wayland session
      if (x11_display_env) {
        // DISPLAY is set, XWayland is available
        blog(LOG_INFO, "Detected: Wayland session with XWayland");
        return LinuxDisplayServer::XWAYLAND;
      } else {
        // Pure Wayland, no XWayland
        blog(LOG_INFO, "Detected: Pure Wayland (no XWayland)");
        return LinuxDisplayServer::WAYLAND;
      }
    } else if (strcmp(session_type, "x11") == 0) {
      blog(LOG_INFO, "Detected: X11 session");
      return LinuxDisplayServer::X11;
    }
  }

  // Fallback detection
  if (wayland_display && x11_display_env) {
    blog(LOG_INFO, "Detected: Wayland with XWayland (fallback)");
    return LinuxDisplayServer::XWAYLAND;
  } else if (wayland_display) {
    blog(LOG_INFO, "Detected: Wayland (fallback)");
    return LinuxDisplayServer::WAYLAND;
  } else if (x11_display_env) {
    blog(LOG_INFO, "Detected: X11 (fallback)");
    return LinuxDisplayServer::X11;
  }

  blog(LOG_WARNING, "Could not detect display server");
  return LinuxDisplayServer::UNKNOWN;
}

bool ObsInterface::initX11Preview(NativeWindowHandle parent) {
  // Store the parent window ID
  x11_parent_window = parent;

  // Open connection to X11 display
  if (!x11_display) {
    x11_display = XOpenDisplay(NULL);
    if (!x11_display) {
      blog(LOG_ERROR, "Failed to open X11 display connection");
      return false;
    }
    blog(LOG_INFO, "X11 display connection opened");
  }

  // Create a child window for the preview
  if (!x11_preview_window) {
    blog(LOG_INFO, "Creating X11 preview child window for parent: %u", parent);

    // Get the parent window's attributes to understand its properties
    XWindowAttributes parent_attrs;
    if (!XGetWindowAttributes(x11_display, x11_parent_window, &parent_attrs)) {
      blog(LOG_ERROR, "Failed to get parent window attributes - window may not exist or be accessible");
      return false;
    }

    blog(LOG_INFO, "Parent window: screen=%d, visual depth=%d, width=%d, height=%d",
         XScreenNumberOfScreen(parent_attrs.screen),
         parent_attrs.depth,
         parent_attrs.width,
         parent_attrs.height);

    // Create a simple window as a child of the parent
    // Initial size 1x1, will be resized by configurePreview
    x11_preview_window = XCreateSimpleWindow(
      x11_display,
      x11_parent_window,  // Parent window
      0, 0,               // Position (x, y)
      1, 1,               // Size (width, height) - will be configured later
      0,                  // Border width
      BlackPixel(x11_display, DefaultScreen(x11_display)),  // Border color
      BlackPixel(x11_display, DefaultScreen(x11_display))   // Background color
    );

    if (!x11_preview_window) {
      blog(LOG_ERROR, "Failed to create X11 preview window");
      return false;
    }

    blog(LOG_INFO, "X11 preview window created: %lu", x11_preview_window);

    // Select events we want to receive
    XSelectInput(x11_display, x11_preview_window, ExposureMask | StructureNotifyMask);

    // Flush to ensure window is created
    XFlush(x11_display);
  }

  // Create OBS display with the X11 window
  if (!display) {
    blog(LOG_INFO, "Creating OBS display for X11 window");

    gs_init_data gs_data = {};
    gs_data.adapter = 0;
    gs_data.cx = 1920; // Gets overwritten when we call configurePreview()
    gs_data.cy = 1080; // Gets overwritten when we call configurePreview()
    gs_data.format = GS_BGRA;
    gs_data.zsformat = GS_ZS_NONE;
    gs_data.num_backbuffers = 1;
    gs_data.window.id = x11_preview_window;
    gs_data.window.display = x11_display;

    display = obs_display_create(&gs_data, 0x0);

    if (!display) {
      blog(LOG_ERROR, "Failed to create OBS display - check OBS/OpenGL initialization");
      return false;
    }

    blog(LOG_INFO, "OBS display created successfully");
    obs_display_add_draw_callback(display, draw_callback, this);
  }

  return true;
}
#endif

void ObsInterface::configurePreview(int x, int y, int width, int height) {
  blog(LOG_INFO, "ObsInterface::configurePreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }

  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

  blog(LOG_INFO, "Moving preview child window to (%d, %d) with size (%d x %d)", x, y, width, height);

  // Resize and move the existing child window.
  bool success = SetWindowPos(
    preview_hwnd,                  // Handle to the child window
    NULL,                          // No Z-order change
    x, y,                          // New position (x, y)
    width, height,                 // New size (width, height)
    SWP_NOACTIVATE                 // Flags
  );

  if (!success) {
    blog(LOG_ERROR, "Failed to resize preview window to (%d x %d)", width, height);
    return;
  }

  obs_display_resize(display, width, height);
  obs_display_set_enabled(display, true);
#else
  if (!x11_display || !x11_preview_window) {
    blog(LOG_ERROR, "X11 preview not initialized");
    return;
  }

  if (!display) {
    blog(LOG_ERROR, "OBS display not initialized");
    return;
  }

  blog(LOG_INFO, "Configuring X11 preview: position (%d, %d), size (%d x %d)", x, y, width, height);

  // Move and resize the X11 preview window
  XMoveResizeWindow(x11_display, x11_preview_window, x, y, width, height);
  XFlush(x11_display);

  // Resize the OBS display
  obs_display_resize(display, width, height);
  obs_display_set_enabled(display, true);
#endif
}

void ObsInterface::showPreview() {
  blog(LOG_INFO, "ObsInterface::showPreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }

  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

  ShowWindow(preview_hwnd, SW_SHOW);
  obs_display_set_enabled(display, true);
#else
  if (!x11_display || !x11_preview_window) {
    blog(LOG_ERROR, "X11 preview not initialized");
    return;
  }

  if (!display) {
    blog(LOG_ERROR, "OBS display not initialized");
    return;
  }

  blog(LOG_INFO, "Showing X11 preview window");
  XMapWindow(x11_display, x11_preview_window);
  XFlush(x11_display);
  obs_display_set_enabled(display, true);
#endif
}

void ObsInterface::hidePreview() {
  blog(LOG_INFO, "ObsInterface::hidePreview");

#ifdef _WIN32
  if (preview_hwnd) {
    ShowWindow(preview_hwnd, SW_HIDE);
    blog(LOG_INFO, "Preview child window hidden");
  }
#else
  if (x11_display && x11_preview_window) {
    blog(LOG_INFO, "Hiding X11 preview window");
    XUnmapWindow(x11_display, x11_preview_window);
    XFlush(x11_display);
  }
#endif
}

void ObsInterface::disablePreview() {
  blog(LOG_INFO, "ObsInterface::disablePreview");

#ifdef _WIN32
  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

  hidePreview();
  obs_display_set_enabled(display, false);
#else
  if (!display) {
    blog(LOG_ERROR, "OBS display not initialized");
    return;
  }

  hidePreview();
  obs_display_set_enabled(display, false);
#endif
}

PreviewInfo ObsInterface::getPreviewInfo() {
  obs_video_info ovi;
  obs_get_video_info(&ovi);

#ifdef _WIN32
  if (!display) {
    blog(LOG_WARNING, "Display not initialized when calling getPreviewInfo");
    return { ovi.base_width, ovi.base_height, ovi.base_width, ovi.base_height };
  }

  uint32_t width, height;
  obs_display_size(display, &width, &height);

  PreviewInfo info = {
    ovi.base_width,
    ovi.base_height,
    width,
    height,
  };

  return info;
#else
  if (!display) {
    blog(LOG_WARNING, "Display not initialized when calling getPreviewInfo");
    return { ovi.base_width, ovi.base_height, ovi.base_width, ovi.base_height };
  }

  uint32_t width, height;
  obs_display_size(display, &width, &height);

  PreviewInfo info = {
    ovi.base_width,
    ovi.base_height,
    width,
    height,
  };

  return info;
#endif
}

void ObsInterface::setDrawSourceOutline(bool enabled) {
  drawSourceOutline = enabled;
}

bool ObsInterface::getDrawSourceOutlineEnabled() {
  return drawSourceOutline;
}

ObsInterface::ObsInterface(
  const std::string& distPath, 
  const std::string& logPath, 
  Napi::ThreadSafeFunction cb
) {
  // Setup logs first so we have logs for the initialization.
  base_set_log_handler(log_handler, (void*)logPath.c_str());
  blog(LOG_DEBUG, "Creating ObsInterface");

  // Initialize OBS and load required modules.
  init_obs(distPath);

  // Setup callback function.
  jscb = cb;

  // Contexts for signal callbacks.
  starting_ctx = new SignalContext{ this, "starting" };
  start_ctx = new SignalContext{ this, "start" };
  stopping_ctx = new SignalContext{ this, "stopping" };
  stop_ctx = new SignalContext{ this, "stop" };
  activate_ctx = new SignalContext{this, "activate"};
  deactivate_ctx = new SignalContext{this, "deactivate"};

  // Create the resources we rely on.
  create_scene();
  create_output();
  create_video_encoders();
  create_audio_encoders();
}

ObsInterface::~ObsInterface() {
  blog(LOG_DEBUG, "Shutting down");

  for (auto& kv : volmeters) {
    obs_volmeter_t* volmeter = kv.second;
    obs_volmeter_remove_callback(volmeter, volmeter_callback, this);
    obs_volmeter_detach_source(volmeter);
    obs_volmeter_destroy(volmeter);
    blog(LOG_INFO, "Volmeter deleted for source: %s", kv.first.c_str());
    volmeters.erase(kv.first);
  }

  for (auto& kv : volmeter_cb_ctx) {
    SignalContext* ctx = kv.second;
    delete ctx;
    volmeter_cb_ctx.erase(kv.first);
  }

  delete starting_ctx;
  delete start_ctx;
  delete stopping_ctx;
  delete stop_ctx;
  delete activate_ctx;
  delete deactivate_ctx;

  for (auto& kv : sources) {
    std::string name = kv.first;
    obs_source_t* source = kv.second;

    auto filter_it = filters.find(name);

    if (filter_it != filters.end()) {
      obs_source_t* filter = filter_it->second;
      obs_source_filter_remove(source, filter);
      obs_source_release(filter);
      filters.erase(name);
      blog(LOG_INFO, "Filter removed for source: %s on shutdown", name.c_str());
    }

    blog(LOG_DEBUG, "Releasing source: %s", name.c_str());
    obs_source_release(source);
    sources.erase(name);
  }

  if (scene) {
    blog(LOG_DEBUG, "Releasing scene");
    obs_scene_release(scene);
  }

  if (output) {
    if (obs_output_active(output)) {
      blog(LOG_DEBUG, "Force stopping output");
      obs_output_force_stop(output);
    }
      
    blog(LOG_DEBUG, "Releasing output");
    obs_output_release(output);
  }

  // if (video_encoder) {
  //   blog(LOG_DEBUG, "Releasing video encoder");
  //   obs_encoder_release(video_encoder);
  // }

  // if (audio_encoder) {
  //   blog(LOG_DEBUG, "Releasing audio encoder");
  //   obs_encoder_release(audio_encoder);
  // }

  // Clean up display before shutting down OBS
  if (display) {
    blog(LOG_DEBUG, "Destroying OBS display");
    obs_display_destroy(display);
    display = nullptr;
  }

#ifndef _WIN32
  // Clean up X11 resources on Linux
  if (x11_preview_window && x11_display) {
    blog(LOG_DEBUG, "Destroying X11 preview window");
    XDestroyWindow(x11_display, x11_preview_window);
    x11_preview_window = 0;
  }

  if (x11_display) {
    blog(LOG_DEBUG, "Closing X11 display connection");
    XCloseDisplay(x11_display);
    x11_display = nullptr;
  }
#endif

  blog(LOG_DEBUG, "Now shutting down OBS");
  obs_shutdown();

  if (jscb) {
    blog(LOG_DEBUG, "Releasing JavaScript callback");
    jscb.Release();
  }

  blog(LOG_DEBUG, "Shutdown complete");
}

void ObsInterface::setBuffering(bool value) {
  if (obs_output_active(output)) {
    blog(LOG_ERROR, "Cannot change buffering state while output is active");
    throw new std::runtime_error("Cannot change buffering state while output is active");
  }

  buffering = value;
  create_output();
}

void ObsInterface::startBuffering() {
  blog(LOG_INFO, "ObsInterface::startBuffering called");

  if (!buffering) {
    blog(LOG_ERROR, "Buffering is not enabled!");
    throw std::runtime_error("Buffering is not enabled!");
  }

  if (!output) {
    blog(LOG_ERROR, "Output is not initialized!");
    throw std::runtime_error("Output is not initialized!");
  }

  bool is_active = obs_output_active(output);

  if (is_active) {
    blog(LOG_WARNING, "Output is already active");
    return;
  }

  blog(LOG_INFO, "Validating OBS state before starting buffer");

  // Check video output is configured
  video_t *video = obs_get_video();
  if (!video) {
    blog(LOG_ERROR, "No video output configured!");
    throw std::runtime_error("No video output configured!");
  }
  blog(LOG_INFO, "  Video output: configured");

  // Check audio output is configured
  audio_t *audio = obs_get_audio();
  if (!audio) {
    blog(LOG_ERROR, "No audio output configured!");
    throw std::runtime_error("No audio output configured!");
  }
  blog(LOG_INFO, "  Audio output: configured");

  // Check scene is set as output source
  obs_source_t *scene_source = obs_get_output_source(0);
  if (!scene_source) {
    blog(LOG_ERROR, "No scene source set as output!");
    throw std::runtime_error("No scene source set as output!");
  }
  uint32_t scene_width = obs_source_get_width(scene_source);
  uint32_t scene_height = obs_source_get_height(scene_source);
  blog(LOG_INFO, "  Scene source: %s (%ux%u)", obs_source_get_name(scene_source), scene_width, scene_height);
  obs_source_release(scene_source);

  // Check encoders
  blog(LOG_INFO, "About to call obs_output_start...");
  blog(LOG_INFO, "  Output type: %s", obs_output_get_id(output));
  blog(LOG_INFO, "  Video encoder: %s", video_encoder ? obs_encoder_get_id(video_encoder) : "NULL");
  blog(LOG_INFO, "  Audio encoder: %s", audio_encoder ? obs_encoder_get_id(audio_encoder) : "NULL");

  if (!video_encoder) {
    blog(LOG_ERROR, "No video encoder set!");
    throw std::runtime_error("No video encoder set!");
  }

  if (!audio_encoder) {
    if (audio_disabled) {
      blog(LOG_WARNING, "Audio encoder disabled - recording will be video-only");
    } else {
      blog(LOG_ERROR, "No audio encoder set!");
      throw std::runtime_error("No audio encoder set!");
    }
  }

  // Check if encoders are active
  blog(LOG_INFO, "  Video encoder active: %s", obs_encoder_active(video_encoder) ? "yes" : "no");
  blog(LOG_INFO, "  Audio encoder active: %s", audio_encoder ? (obs_encoder_active(audio_encoder) ? "yes" : "no") : "disabled");

  blog(LOG_INFO, "Calling obs_output_start NOW");
  bool success = obs_output_start(output);
  blog(LOG_INFO, "obs_output_start returned: %s", success ? "true" : "false");

  if (!success) {
    const char *err = obs_output_get_last_error(output);
    blog(LOG_ERROR, "Failed to start buffering! Error: %s", err ? err : "Unknown");
    throw std::runtime_error("Failed to start buffering!");
  }

  blog(LOG_INFO, "ObsInterface::startBuffering exited");
}

void ObsInterface::startRecording(int offset) {
  blog(LOG_INFO, "ObsInterface::startRecording enter");

  if (recording_path == "") {
    blog(LOG_ERROR, "Recording path is not set");
    throw std::runtime_error("Recording path is not set");
  }

  if (buffering) {
    bool is_active = obs_output_active(output);

    if (!is_active) {
      blog(LOG_WARNING, "Buffer is not active");
      throw std::runtime_error("Buffer is not active");
    }

#ifdef _WIN32
    blog(LOG_INFO, "Saving replay buffer to file with offset %d", offset);
    calldata cd;
    calldata_init(&cd);
    proc_handler_t *ph = obs_output_get_proc_handler(output);

    if (!ph) {
      blog(LOG_ERROR, "Failed to get proc handler from output");
      throw std::runtime_error("Failed to get proc handler from output");
    }

    calldata_set_int(&cd, "offset_seconds", offset);
    bool success = proc_handler_call(ph, "convert", &cd);
    calldata_free(&cd);

    if (!success) {
      const char *last_error = obs_output_get_last_error(output);
      blog(LOG_ERROR, "Failed to save replay buffer. Error: %s", last_error ? last_error : "unknown");
      blog(LOG_ERROR, "Output active: %s", obs_output_active(output) ? "yes" : "no");
      blog(LOG_ERROR, "Output type: %s", obs_output_get_id(output));
      throw std::runtime_error("Failed to save replay buffer");
    }

    blog(LOG_INFO, "Replay buffer save triggered successfully");
#else
    // On Linux, the buffer is saved at encounter end instead.
    blog(LOG_INFO, "Linux: Recording started, will save buffer at encounter end (offset %d ignored)", offset);
#endif
  } else {
    obs_data_t *ffmpeg_settings = obs_data_create();
#ifdef _WIN32
    std::string pathSep = "\\";
#else
    std::string pathSep = "/";
#endif
    std::string filename = recording_path + pathSep + get_current_date_time() + "." + file_extension;
    obs_data_set_string(ffmpeg_settings,  "path", filename.c_str());
    obs_output_update(output, ffmpeg_settings);
    obs_data_release(ffmpeg_settings);
    unbuffered_output_filename = filename;

    blog(LOG_INFO, "Starting ffmpeg_muxer output");

    bool is_active = obs_output_active(output);

    if (is_active) {
      blog(LOG_WARNING, "Output already active");
      return;
    }

    blog(LOG_WARNING, "Call start");
    bool success = obs_output_start(output);

    if (!success) {
      const char *err = obs_output_get_last_error(output);
      blog(LOG_ERROR, "Failed to start recording: %s", err ? err : "Unknown error");
      throw std::runtime_error("Failed to start recording");
    }
  }

  blog(LOG_INFO, "ObsInterface::startRecording exit");
}

void ObsInterface::stopRecording() {
  blog(LOG_INFO, "ObsInterface::stopRecording enter");
  bool is_active = obs_output_active(output);

  if (!is_active) {
    blog(LOG_WARNING, "Output is not active");
    return;
  }

#ifndef _WIN32
  if (buffering) {
    blog(LOG_INFO, "Linux: Saving replay buffer at encounter end");
    proc_handler_t *ph = obs_output_get_proc_handler(output);

    if (ph) {
      calldata old_cd;
      calldata_init(&old_cd);
      proc_handler_call(ph, "get_last_replay", &old_cd);
      const char* old_path_ptr = calldata_string(&old_cd, "path");
      std::string old_path = old_path_ptr ? old_path_ptr : "";
      calldata_free(&old_cd);
      blog(LOG_INFO, "Linux: Previous last_replay path: %s", old_path.c_str());

      // Trigger the save
      calldata cd;
      calldata_init(&cd);
      bool success = proc_handler_call(ph, "save", &cd);
      calldata_free(&cd);

      if (success) {
        blog(LOG_INFO, "Linux: Replay buffer save triggered, waiting for completion");
        for (int i = 0; i < 100; i++) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));

          // Check if the file path has CHANGED (not just exists)
          calldata check_cd;
          calldata_init(&check_cd);
          proc_handler_call(ph, "get_last_replay", &check_cd);
          const char* check_path = calldata_string(&check_cd, "path");
          std::string new_path = check_path ? check_path : "";
          calldata_free(&check_cd);

          if (!new_path.empty() && new_path != old_path) {
            blog(LOG_INFO, "Linux: Save completed, new file: %s", new_path.c_str());
            break;
          }

          if (i == 99) {
            blog(LOG_WARNING, "Linux: Timed out waiting for save to complete (path unchanged)");
          }
        }
      } else {
        const char *last_error = obs_output_get_last_error(output);
        blog(LOG_ERROR, "Linux: Failed to save replay buffer. Error: %s",
             last_error ? last_error : "unknown");
      }
    } else {
      blog(LOG_ERROR, "Linux: Failed to get proc handler for save");
    }
  }
#endif

  obs_output_stop(output);
  blog(LOG_INFO, "ObsInterface::stopRecording exited");
}

void ObsInterface::forceStopRecording() {
  blog(LOG_INFO, "ObsInterface::forceStopRecording enter");
  bool is_active = obs_output_active(output);

  if (!is_active) {
    blog(LOG_WARNING, "Output is not active");
    return;
  }

  obs_output_force_stop(output);
  blog(LOG_INFO, "ObsInterface::forceStopRecording exited");
}

std::string ObsInterface::getLastRecording() {
  blog(LOG_INFO, "calling get last replay proc handler");
  calldata cd;
  calldata_init(&cd);

  proc_handler_t *ph = obs_output_get_proc_handler(output);

  const char* type = obs_output_get_id(output);

  if (!buffering) {
    blog(LOG_INFO, "Getting last recording path from ffmpeg_muxer");
    return unbuffered_output_filename;
  }

  bool success = proc_handler_call(ph, "get_last_replay", &cd);

  if (!success) {
    blog(LOG_ERROR, "Failed to call procedure handler");
    const char *err = obs_output_get_last_error(output);
    blog(LOG_ERROR, "%s", err ? err : "Unknown error");
    calldata_free(&cd);
    return "";
  }

  const char* p = calldata_string(&cd, "path");
  std::string path = p ? p : "" ;
  calldata_free(&cd);
  
  blog(LOG_INFO, "return path: %s", path.c_str());
  return path;
}

void ObsInterface::addSourceToScene(std::string name) {
  blog(LOG_INFO, "ObsInterface::addSourceToScene called for source: %s", name.c_str());

  obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());

  if (item) {
    blog(LOG_WARNING, "Source %s already in scene", name.c_str());
    return;
  }

  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when adding to scene", name.c_str());
    return;
  }

  obs_source_t* source = it->second;
  item = obs_scene_add(scene, source);

  if (!item) {
    blog(LOG_ERROR, "Failed to add source to scene: %s", name.c_str());
  }
  
  blog(LOG_INFO, "ObsInterface::addSourceToScene exited");
}

void ObsInterface::removeSourceFromScene(std::string name) {
  blog(LOG_INFO, "ObsInterface::removeSourceFromScene called for source: %s", name.c_str());

  obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());
  
  if (!item) {
    blog(LOG_WARNING, "Did not find scene item for video source: %s", name.c_str());
    return;
  }

  obs_sceneitem_remove(item);
  blog(LOG_INFO, "ObsInterface::removeSourceFromScene exited");
}

void ObsInterface::getSourcePos(std::string name, vec2* pos, vec2* size, vec2* scale, obs_sceneitem_crop* crop) 
{
  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when getting source position", name.c_str());
    throw std::runtime_error("Source not found!");
  }

  obs_source_t* source = it->second;

  if (!source) {
    blog(LOG_WARNING, "Did not find source for video source: %s", name.c_str());
    return;
  }

  obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());

  if (!item) {
    blog(LOG_WARNING, "Did not find scene item for video source: %s", name.c_str());
    return;
  }

  obs_sceneitem_get_pos(item, pos);
  obs_sceneitem_get_scale(item, scale);
  obs_sceneitem_get_crop(item, crop);

  // Pre-scaled sizes.
  size->x = obs_source_get_width(source);
  size->y = obs_source_get_height(source);
}

void ObsInterface::setSourcePos(std::string name, vec2* pos, vec2* scale, obs_sceneitem_crop* crop) {
  obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());

  if (!item) {
    blog(LOG_WARNING, "Did not find scene item for video source: %s", name.c_str());
    return;
  }

  obs_sceneitem_set_pos(item, pos);
  obs_sceneitem_set_scale(item, scale);
  obs_sceneitem_set_crop(item, crop);
}

std::vector<std::string> ObsInterface::listAvailableVideoEncoders()
{
  std::vector<std::string> encoders;
  size_t idx = 0;
  const char *encoder_type;

  while (obs_enum_encoder_types(idx++, &encoder_type)) {
    bool video = obs_get_encoder_type(encoder_type) == OBS_ENCODER_VIDEO;

    if (video)
      encoders.emplace_back(encoder_type);
  }

  return encoders;
}

void ObsInterface::setVideoEncoder(std::string id, obs_data_t* settings) {
  if (obs_output_active(output)) {
    blog(LOG_WARNING, "Cannot change video encoder while output is active");
    throw new std::runtime_error("Output is active when trying to change encoder");
  }

  video_encoder_id = id;
  obs_data_release(video_encoder_settings);
  video_encoder_settings = settings;
  create_video_encoders();
}

void ObsInterface::setMuteAudioInputs(bool mute) {
  // Loop over all sources, and set the mute state if they are of type "wasapi_input_capture".
  for (const auto& kv : sources) {
    const std::string& name = kv.first;
    obs_source_t* source = kv.second;

    if (!source) {
      blog(LOG_WARNING, "Source %s not found when muting audio inputs", name.c_str());
      continue;
    }

    const char* type = obs_source_get_id(source);

    if (strcmp(type, AUDIO_INPUT) == 0) {
      obs_source_set_muted(source, mute);
    }
  }
}

void ObsInterface::setSourceVolume(std::string name, float volume) {
  blog(LOG_INFO, "Setting source %s volume to %f", name.c_str(), volume);

  auto it = sources.find(name);

  if (it == sources.end()) {
    blog(LOG_WARNING, "Source %s not found when setting volume", name.c_str());
    return;
  }

  obs_source_t* source = it->second;
  const char* type = obs_source_get_id(source);
  
  bool audio = 
    strcmp(type, AUDIO_OUTPUT) == 0 || 
    strcmp(type, AUDIO_INPUT) == 0  || 
    strcmp(type, AUDIO_PROCESS) == 0;

  if (!audio) {
    blog(LOG_WARNING, "Source %s is not a valid audio source", name.c_str());
    return;
  }

  obs_source_set_volume(source, volume);
}

void ObsInterface::setVolmeterEnabled(bool enabled) {
  blog(LOG_INFO, "Setting volmeter enabled: %d", enabled);
  volmeter_enabled = enabled;
}

void ObsInterface::setForceMono(bool enabled) {
  blog(LOG_INFO, "%s force mono on all input sources", enabled ? "Enabling" : "Disabling");
  force_mono = enabled;

  // Loop over existing sources and update the force mono flags.
  for (const auto& kv : sources) {
    const std::string& name = kv.first;
    obs_source_t* source = kv.second;

    if (!source) {
      blog(LOG_WARNING, "Source %s not found when setting force mono", name.c_str());
      continue;
    }

    const char* type = obs_source_get_id(source);

    if (strcmp(type, AUDIO_INPUT) != 0) {
      // Force mono is only applicable to microphones, skip other types.
      continue;
    }

    if (enabled) {
      blog(LOG_INFO, "Setting force mono flag on source %s", name.c_str());
      uint32_t flags = obs_source_get_flags(source);
      obs_source_set_flags(source, flags | OBS_SOURCE_FLAG_FORCE_MONO);
    } else {
      blog(LOG_INFO, "Unsetting force mono flag on source %s", name.c_str());
      uint32_t flags = obs_source_get_flags(source);
      obs_source_set_flags(source, flags & ~OBS_SOURCE_FLAG_FORCE_MONO);
    }
  }
}

void ObsInterface::setAudioSuppression(bool enabled) {
  blog(LOG_INFO, "%s audio suppression on all input devices", enabled ? "Enabling" : "Disabling");
  audio_suppression = enabled;

  // Loop over existing sources and add filters to any that need it.
  for (const auto& kv : sources) {
    const std::string& name = kv.first;
    obs_source_t* source = kv.second;

    if (!source) {
      blog(LOG_WARNING, "Source %s not found when adding filters", name.c_str());
      continue;
    }

    const char* type = obs_source_get_id(source);

    if (strcmp(type, AUDIO_INPUT) != 0) {
      // Don't care about non-input sources. This is purely for suppressing
      // microphone background noise.
      continue;
    }

    // Check for a filter existing and add or remove it as appropriate.
    auto filter_it = filters.find(name);
    
    if (audio_suppression && filter_it == filters.end()) {
      blog(LOG_INFO, "Setting up filter for source: %s", name.c_str());
      
      std::string filter_name = "Filter for " + name;

      obs_source_t *filter = obs_source_create(
        "noise_suppress_filter_v2",   
        filter_name.c_str(),   
        nullptr, // Defaults are sensible.
        nullptr
      );

      if (!filter) {
        blog(LOG_ERROR, "Failed to create filter for source: %s", name.c_str());
        throw std::runtime_error("Failed to create filter!");
      }

      filters[name] = filter;
      obs_source_filter_add(source, filter);
    } else if (!audio_suppression && filter_it != filters.end()) {
      blog(LOG_INFO, "Removing filters for source: %s", name.c_str());
      obs_source_t* filter = filter_it->second;
      obs_source_filter_remove(source, filter);
      filters.erase(name);
      obs_source_release(filter);
    }
  }
}

void ObsInterface::sourceCallback(std::string name) {
  blog(LOG_INFO, "Source callback triggered for %s", name.c_str());
  SignalData* sd = new SignalData{ "source", name.c_str(), 0 };
  jscb.NonBlockingCall(sd, call_jscb);
}

void ObsInterface::zeroVolmeter(std::string name) {
  blog(LOG_INFO, "Zeroing volmeter for %s", name.c_str());
  SignalData* sd = new SignalData{ "volmeter", name.c_str(), 0, 0 };
  jscb.NonBlockingCall(sd, call_jscb);
}
