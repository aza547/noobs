#include "obs_interface.h"

#include "utils.h"

// OBS/library headers
#include <cstdarg>
#include <cstring>
#include <obs.h>
#include <obs-audio-controls.h>
#include <obs-data.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <util/base.h>
#include <util/platform.h>

// Platform headers
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

// std
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

  if (sd->path.has_value()) {
    obj.Set("path", Napi::String::New(env, sd->path.value()));
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
#elif defined(__linux__)
  ovi.graphics_module = "libobs-opengl";
#else
  #error "Unsupported platform"
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
  
  std::string basePath = distPath;

  if (basePath.back() != '/' && basePath.back() != '\\') {
    // Add a trailing slash if not present.
    basePath += '/';
  }

#ifdef _WIN32
    std::string pluginPath = basePath + "obs-plugins/win64/";
#elif defined(__linux__)
    std::string pluginPath = basePath + "obs-plugins/linux/";
#else
    #error "Unsupported platform"
#endif

  std::string effectsPath = basePath + "data/effects/";
  std::string pluginDataPath = basePath + "data/obs-plugins/";

  blog(LOG_INFO, "Base path: %s", basePath.c_str());
  blog(LOG_INFO, "Effects path: %s", effectsPath.c_str());
  blog(LOG_INFO, "Plugin path: %s", pluginPath.c_str());
  blog(LOG_INFO, "Data path: %s", pluginDataPath.c_str());

  // Add the effects path. We need this before resetting video and audio
  // to ensure the effects are available. The function is deprecated in
  // libobs but it works for now.
  obs_add_data_path(effectsPath.c_str());

  // This must come before loading modules to initialize D3D11/OpenGL
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
    "obs-x264",     // Software encoder
    "obs-ffmpeg",   // Contains AMF (AMD) encoder support
    "win-capture",  // Required for basically all forms of capture on Windows
    "image-source", // Required for image sources
    "win-wasapi",   // Required for WASAPI audio input
    "obs-nvenc",    // Required for NVENC video encoding
    "obs-qsv11",    // Required for QSV video encoding
    "obs-filters"   // Required for audio filters
  };
#elif defined(__linux__)
  std::vector<std::string> modules = { 
    "obs-x264",             // Software encoder
    "obs-ffmpeg",           // Contains AMF (AMD) encoder support
    "image-source",         // Required for image sources
    "linux-pipewire",       // Required for Pipewire window/screen capture
    "linux-pulseaudio",     // Required for PulseAudio audio input
    "obs-nvenc",            // Required for NVENC video encoding
    "obs-qsv11",            // Required for QSV video encoding
    "obs-filters",          // Required for audio filters
    "linux-pipewire-audio"  // Required for per-window audio capture: https://github.com/dimtpap/obs-pipewire-audio-capture
  };
#else
    #error "Unsupported platform"
#endif

  for (const auto& module : modules) {
#ifdef _WIN32
    std::string modulePath = pluginPath + module + ".dll";
#elif defined(__linux__)
    std::string modulePath = pluginPath + module + ".so";
#else
    #error "Unsupported platform"
#endif
    std::string moduleDataPath = pluginDataPath + module;

    // NVENC fails if there is no NVENC hardware support.
    // QSV11 is hardlinked to libvpl on linux, so will almost always fail on pure AMD hardware
#ifdef __linux__
    bool allowFail = module == "obs-nvenc" || module == "obs-qsv11";
#else
    bool allowFail = module == "obs-nvenc";
#endif
    load_module(modulePath.c_str(), moduleDataPath.c_str(), allowFail);
  }
  
  obs_post_load_modules();
#ifdef _WIN32 // X11/Linux does not require window class registration
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
    obs_data_set_int(settings, "max_time_sec", 60);
    obs_data_set_int(settings, "max_size_mb", 1024);
    obs_data_set_string(settings, "directory", recording_path.c_str());
    obs_data_set_string(settings, "format", "%CCYY-%MM-%DD %hh-%mm-%ss");
    obs_data_set_string(settings, "extension", file_extension.c_str());
  } else {
    blog(LOG_INFO, "Set ffmpeg_muxer settings");
    // Need to specify the exact path for ffmpeg_muxer. We will write this again at start recording.
    std::filesystem::path filepath = std::filesystem::path(recording_path) / (get_current_date_time() + "." + file_extension);
    std::string filename = filepath.string();
    obs_data_set_string(settings, "path", filename.c_str());
    unbuffered_output_filename = filename;
  }

  if (fragmented && file_extension == "mp4") {
    blog(LOG_INFO, "Fragmentation enabled");
    std::string mux_frag = "movflags=frag_keyframe+empty_moov+delay_moov";
    obs_data_set_string(settings, "muxer_settings", mux_frag.c_str());
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

  audio_encoder = obs_audio_encoder_create(
    "ffmpeg_aac", 
    "aac_file",
    NULL, 
    0, 
    NULL
  );

  if (!audio_encoder) {
    blog(LOG_ERROR, "Failed to create audio encoder!");
    throw std::runtime_error("Failed to create audio encoder!");
  }

  blog(LOG_INFO, "Set audio encoder settings");
  obs_data_t *aenc_settings = obs_data_create();
  obs_data_set_int(aenc_settings, "bitrate", 128);
  obs_encoder_update(audio_encoder, aenc_settings);
  obs_data_release(aenc_settings);

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

  if (self->is_shutting_down())
    return;

  if (!self->volmeter_enabled) {
    return;
  }

  SignalData* sd = new SignalData{ "volmeter", ctx->id.c_str(), 0, obs_db_to_mul(peak[0]) };
  self->jscb.NonBlockingCall(sd, call_jscb);
}

std::string ObsInterface::createSource(std::string name, std::string type, obs_data_t* settings) {
  blog(LOG_INFO, "Create source: %s of type %s", name.c_str(), type.c_str());

  obs_source_t *source = obs_source_create(
    type.c_str(), // Type of source, e.g. "wasapi_input_capture"
    name.c_str(), // Name of the source, e.g. "My Audio Input"
    settings,
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

void ObsInterface::output_signal_handler(void *data, calldata_t *cd) {
  SignalContext* ctx = static_cast<SignalContext*>(data);
  ObsInterface* self = ctx->self;

  if (self->is_shutting_down())
    return;

  SignalData* sd;

  blog(LOG_INFO, "Handling %s signal from libobs", ctx->id.c_str());

  if (ctx->id == "converted") {
    const char *path = calldata_string(cd, "file");

    sd = new SignalData{ 
      "output", 
      "converted", 
      0, // Never actually get a code for a converted signal, so just set it to 0.
      std::nullopt, // No value, that's only used for volmeters.
      std::nullopt, // Never expect errors here.
      std::string(path),
    };
  } else {
    long long code = calldata_int(cd, "code");
    const char *err = calldata_string(cd, "last_error");
    std::optional<std::string> error;

    if (err) {
      error = std::string(err);
    }

    sd = new SignalData{ 
      "output", 
      ctx->id.c_str(), 
      code, 
      std::nullopt, // No value, that's only used for volmeters.
      error,
    };
  }

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
  signal_handler_connect(sh, "converted", output_signal_handler, converted_ctx);
}

void ObsInterface::disconnect_signal_handlers(obs_output_t *output) {
  signal_handler_t *sh = obs_output_get_signal_handler(output);
  signal_handler_disconnect(sh, "starting", output_signal_handler,  starting_ctx);
  signal_handler_disconnect(sh, "start", output_signal_handler,  start_ctx);
  signal_handler_disconnect(sh, "stopping", output_signal_handler,  stopping_ctx);
  signal_handler_disconnect(sh, "stop", output_signal_handler,  stop_ctx);
  signal_handler_disconnect(sh, "activate", output_signal_handler, activate_ctx);
  signal_handler_disconnect(sh, "deactivate", output_signal_handler, deactivate_ctx);
  signal_handler_disconnect(sh, "converted", output_signal_handler, converted_ctx);
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
  auto* obsInterface = static_cast<ObsInterface*>(data);

  if (obsInterface->is_shutting_down())
    return;

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
  if (source) {
    obs_source_video_render(source);
  }

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

void ObsInterface::initPreview(uintptr_t parent_handle) {

  blog(LOG_INFO, "ObsInterface::initPreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    HWND parent = (HWND)parent_handle;

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

#elif defined(__linux__)
  Window parent = (Window)parent_handle;

  if (!preview_window) {
    x11_display = XOpenDisplay(nullptr);

    preview_window = XCreateSimpleWindow(
      x11_display,            // Display ID - use default
      parent,                 // Window ID from electron electron
      0, 0,                   // Initial position (x, y)
      1, 1,                   // Initial size (width, height) -- 0 is BadValue in X11
      0,                      // border width
      0,                      // border pixel
      0                       // background pixel
    );

    if (!preview_window) {
      blog(LOG_ERROR, "Failed to create preview child window");
      return;
    }

    make_window_click_through(x11_display, preview_window);
  }
#endif

  if (!display) {
    blog(LOG_INFO, "Create OBS display in child window");

    gs_init_data gs_data = {};
    gs_data.adapter = 0;
    gs_data.cx = 1920; // Gets overwritten when we call configurePreview().
    gs_data.cy = 1080; // Gets overwritten when we call configurePreview().
    gs_data.format = GS_BGRA;
    gs_data.zsformat = GS_ZS_NONE;
    gs_data.num_backbuffers = 1;
#ifdef _WIN32
    gs_data.window.hwnd = preview_hwnd;
#else
    gs_data.window.id = preview_window;
    gs_data.window.display = x11_display;
#endif

    display = obs_display_create(&gs_data, 0x0);

    if (!display) {
      blog(LOG_ERROR, "Failed to create OBS display");
      return;
    }

    obs_display_add_draw_callback(display, draw_callback, this);
  }

  obs_display_set_enabled(display, false);
}

void ObsInterface::configurePreview(int x, int y, int width, int height) {
  blog(LOG_INFO, "ObsInterface::configurePreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }
#elif defined(__linux__)
  if (!preview_window) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }
#endif

  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

  blog(LOG_INFO, "Moving preview child window to (%d, %d) with size (%d x %d)", x, y, width, height);

  bool success;

#ifdef _WIN32
  // Resize and move the existing child window.
  success = SetWindowPos(
    preview_hwnd,                  // Handle to the child window
    NULL,                          // No Z-order change
    x, y,                          // New position (x, y)
    width, height,                 // New size (width, height)
    SWP_NOACTIVATE                 // Flags
  );
#elif defined(__linux__)
  XMoveResizeWindow(x11_display, preview_window, x, y, width, height);
  XMapRaised(x11_display, preview_window);
  XFlush(x11_display);
  success = true; // X11 is not straightforward about this, but this is unlikely to fail
#endif

  if (!success) {
    blog(LOG_ERROR, "Failed to resize preview window to (%d x %d)", width, height);
    return;
  }

  obs_display_resize(display, width, height);
  obs_display_set_enabled(display, true);
}

void ObsInterface::showPreview() {
  blog(LOG_INFO, "ObsInterface::showPreview");

#ifdef _WIN32
  if (!preview_hwnd) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }
#elif defined(__linux__)
  if (!preview_window) {
    blog(LOG_ERROR, "Preview window not initialized");
    return;
  }
#endif

  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

#ifdef _WIN32

  ShowWindow(preview_hwnd, SW_SHOW);
  blog(LOG_INFO, "Preview child window shown");

#elif defined(__linux__)

  XMapWindow(x11_display, preview_window);
  XFlush(x11_display);
  blog(LOG_INFO, "Preview child window shown");
  
#endif
  obs_display_set_enabled(display, true);
}

void ObsInterface::hidePreview() {
  blog(LOG_INFO, "ObsInterface::hidePreview");
  
#ifdef _WIN32

  if (preview_hwnd) {
    ShowWindow(preview_hwnd, SW_HIDE);
    blog(LOG_INFO, "Preview child window hidden");
  }

#elif defined(__linux__)

  if (preview_window) {
    XUnmapWindow(x11_display, preview_window);
    XFlush(x11_display);
    blog(LOG_INFO, "Preview child window hidden");
  }

#endif
}

void ObsInterface::disablePreview() {
  blog(LOG_INFO, "ObsInterface::disablePreview");

  if (!display) {
    blog(LOG_ERROR, "Preview display not initialized");
    return;
  }

  hidePreview();
  obs_display_set_enabled(display, false);
}

PreviewInfo ObsInterface::getPreviewInfo() {
  if (!display) {
    blog(LOG_WARNING, "Display not initialized when calling getPreviewInfo");
    return { 1920, 1080, 1920, 1080 }; // Default values
  }

  obs_video_info ovi;
  obs_get_video_info(&ovi);

  uint32_t width, height;
	obs_display_size(display, &width, &height);

  PreviewInfo info = {
    ovi.base_width,
    ovi.base_height,
    width,
    height,
  };

  return info;
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
  converted_ctx = new SignalContext{this, "converted"};

  // Create the resources we rely on.
  create_scene();
  create_output();
  create_video_encoders();
  create_audio_encoders();
}

ObsInterface::~ObsInterface() {
  blog(LOG_DEBUG, "Shutting down");

  shutting_down.store(true);

  // clean up the display and draw callback
  if (display) {
    obs_display_set_enabled(display, false);
    obs_display_remove_draw_callback(display, draw_callback, this);
    obs_display_destroy(display);
    display = nullptr;
  }

#ifdef __linux__
  if (x11_display) {
    XCloseDisplay(x11_display);
    x11_display = nullptr;
  }
#endif

  // cleanup volmeters
  for (auto& [name, volmeter] : volmeters) {
    auto it = volmeter_cb_ctx.find(name);
    SignalContext* ctx = (it != volmeter_cb_ctx.end()) ? it->second : nullptr;
    if (ctx) {
      obs_volmeter_remove_callback(volmeter, volmeter_callback, ctx);
    }
    obs_volmeter_detach_source(volmeter);
    obs_volmeter_destroy(volmeter);
    if (ctx) {
      delete ctx;
    }
  }
  volmeters.clear();
  volmeter_cb_ctx.clear();

  // set clear the output source
  obs_set_output_source(0, nullptr);

  // disconnect signal handlers before deleting their contexts
  if (output) {
    disconnect_signal_handlers(output);

    // try to cleanly stop the output for 2s
    if (obs_output_active(output)) {
      blog(LOG_DEBUG, "Stopping output");
      obs_output_stop(output);

      for (int i = 0; i < 200 && obs_output_active(output); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      // well, we tried ¯\_(ツ)_/¯
      if (obs_output_active(output)) {
        blog(LOG_DEBUG, "Output still active, force stopping");
        obs_output_force_stop(output);
      }
    }

    blog(LOG_DEBUG, "Releasing output");
    obs_output_release(output);
    output = nullptr;
  }

  // cleanup encoders
  if (video_encoder) {
    blog(LOG_DEBUG, "Releasing video encoder");
    obs_encoder_release(video_encoder);
    video_encoder = nullptr;
  }
  if (audio_encoder) {
    blog(LOG_DEBUG, "Releasing audio encoder");
    obs_encoder_release(audio_encoder);
    audio_encoder = nullptr;
  }

  // cleanup sources
  for (auto& [name, source] : sources) {

    // cleanup from the scene
    removeSourceFromScene(name);

    // cleanup source filters
    if (auto it = filters.find(name); it != filters.end()) {
      auto& [k, filter] = *it;
      blog(LOG_INFO, "Beginning filter removal for source: %s on shutdown", name.c_str());

      obs_source_filter_remove(source, filter);
      obs_source_release(filter);
      filters.erase(it);
      blog(LOG_INFO, "Filter removed for source: %s on shutdown", name.c_str());
    }

    blog(LOG_DEBUG, "Releasing source: %s", name.c_str());
    obs_source_remove(source);
    obs_source_release(source);
  }

  sources.clear();
  sizes.clear();

  if (scene) {
    obs_scene_release(scene);
    scene = nullptr;
  }

  // delete the signal handlers
  delete starting_ctx;
  delete start_ctx;
  delete stopping_ctx;
  delete stop_ctx;
  delete activate_ctx;
  delete deactivate_ctx;
  delete converted_ctx;

  // finally, shut down OBS
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

void ObsInterface::setFragmentation(bool value) {
  if (obs_output_active(output)) {
    blog(LOG_ERROR, "Cannot change fragmentation state while output is active");
    throw new std::runtime_error("Cannot change fragmentation state while output is active");
  }

  fragmented = value;
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

  bool success = obs_output_start(output);

  if (!success) {
    blog(LOG_ERROR, "Failed to start buffering!");
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

    blog(LOG_INFO, "calling save proc handler");
    calldata cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "offset_seconds", offset);
    proc_handler_t *ph = obs_output_get_proc_handler(output);
    bool success = proc_handler_call(ph, "convert", &cd);
    calldata_free(&cd);

    if (!success) {
      blog(LOG_ERROR, "Failed to call convert procedure handler");
      throw std::runtime_error("Failed to call convert procedure handler");
    }
  } else {
    obs_data_t *ffmpeg_settings = obs_data_create();
    std::filesystem::path filepath = std::filesystem::path(recording_path) / (get_current_date_time() + "." + file_extension);
    std::string filename = filepath.string();
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

void ObsInterface::setSceneItemOrder(std::string name, obs_order_movement movement) {
  blog(LOG_INFO, "ObsInterface::setSceneItemOrder called for source: %s with movement: %d", name.c_str(), movement);

  obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());
  
  if (!item) {
    blog(LOG_WARNING, "Did not find scene item for source: %s", name.c_str());
    return;
  }

  obs_sceneitem_set_order(item, movement);
  
  blog(LOG_INFO, "ObsInterface::setSceneItemOrder exited");
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
