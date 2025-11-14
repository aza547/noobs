#include <napi.h>
#include <windows.h>
#include <obs.h>
#include "obs_interface.h"
#include "utils.h"

ObsInterface* obs = nullptr;

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

Napi::Value ObsInit(const Napi::CallbackInfo& info) {
  bool valid = info.Length() == 3 &&
   info[0].IsString() &&   // Dist path
   info[1].IsString() &&   // Log path
   info[2].IsFunction();   // JavaScript callback

  if (!valid) {
    Napi::Error::New(info.Env(), "Invalid arguments passed to ObsInit").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string distPath = info[0].As<Napi::String>().Utf8Value();
  std::string logPath = info[1].As<Napi::String>().Utf8Value();
  Napi::Function fn = info[2].As<Napi::Function>();

  Napi::ThreadSafeFunction jscb =
    Napi::ThreadSafeFunction::New(info.Env(), fn, "JavaScript callback", 0, 1);

  obs = new ObsInterface(distPath, logPath, jscb);
  return info.Env().Undefined();
}

Napi::Value ObsShutdown(const Napi::CallbackInfo& info) {
  delete obs;
  obs = nullptr;
  return info.Env().Undefined();
}

Napi::Value ObsSetRecordingCfg(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetRecordingCfg called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 && info[0].IsString() && info[1].IsNumber();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetRecordingCfg").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string recordingPath = info[0].As<Napi::String>().Utf8Value();
  int fileExt = info[1].As<Napi::Number>().Int32Value();
  std::string fileExtension;

  if (fileExt == 0) {
    fileExtension = "mp4";
  } else if (fileExt == 1) {
    fileExtension = "mkv";
  } else {
    Napi::TypeError::New(info.Env(), "Invalid file extension value").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  
  obs->setRecordingCfg(recordingPath, fileExtension);
  return info.Env().Undefined();
}

Napi::Value ObsResetVideoContext(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsResetVideoContext called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 3 && info[0].IsNumber() && info[1].IsNumber() && info[2].IsNumber();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsResetVideo").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  int fps = info[0].As<Napi::Number>().Int32Value();
  int width = info[1].As<Napi::Number>().Int32Value();
  int height = info[2].As<Napi::Number>().Int32Value();

  obs->setVideoContext(fps, width, height);
  return info.Env().Undefined();
}

Napi::Value ObsListVideoEncoders(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsListVideoEncoders called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 0;

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsListVideoEncoders").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  auto encoders = obs->listAvailableVideoEncoders();
  Napi::Array result = Napi::Array::New(info.Env(), encoders.size());

  for (size_t i = 0; i < encoders.size(); ++i) {
    result[i] = Napi::String::New(info.Env(), encoders[i]);
  }

  return result;
}

Napi::Value ObsSetVideoEncoder(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetVideoEncoder called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 &&
    info[0].IsString() && // Encoder ID
    info[1].IsObject(); // Settings object

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetVideoEncoder").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string id = info[0].As<Napi::String>().Utf8Value();
  Napi::Object obj = info[1].As<Napi::Object>();

  obs_data_t* settings = napi_to_data(obj);
  obs->setVideoEncoder(id, settings);

  return info.Env().Undefined();
}

Napi::Value ObsSetBuffering(const Napi::CallbackInfo& info) {
  blog(LOG_INFO, "ObsSetBuffering called");

  if (!obs) {
    blog(LOG_ERROR, "ObsSetBuffering called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBoolean();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetBuffering").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool buffering = info[0].As<Napi::Boolean>().Value();
  obs->setBuffering(buffering);

  return info.Env().Undefined();
}

Napi::Value ObsStartBuffer(const Napi::CallbackInfo& info) {
  blog(LOG_INFO, "ObsStartBuffer called");

  if (!obs) {
    blog(LOG_ERROR, "ObsStartBuffer called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->startBuffering();
  return info.Env().Undefined();
}

Napi::Value ObsStartRecording(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsStartRecording called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  int offset = 0;

  if (info.Length() == 1 && info[0].IsNumber()) {
    offset = info[0].As<Napi::Number>().Int32Value();
  }

  obs->startRecording(offset);
  return info.Env().Undefined();
}

Napi::Value ObsStopRecording(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsStopRecording called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->stopRecording();
  return info.Env().Undefined();
}

Napi::Value ObsForceStopRecording(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsForceStopRecording called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->forceStopRecording();
  return info.Env().Undefined();
}

Napi::Value ObsGetLastRecording(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsGetLastRecording called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string lastRecording = obs->getLastRecording();
  return Napi::String::New(info.Env(), lastRecording);
}

Napi::Value ObsInitPreview(const Napi::CallbackInfo& info) {
  blog(LOG_INFO, "ObsInitPreview called");

  if (!obs) {
    blog(LOG_ERROR, "ObsInitPreview called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBuffer();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsInitPreview").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();

  if (buffer.Length() < sizeof(HWND)) {
    Napi::TypeError::New(info.Env(), "Buffer too small for HWND").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  HWND hwnd = *reinterpret_cast<HWND*>(buffer.Data());
  obs->initPreview(hwnd);
  return info.Env().Undefined();
}

Napi::Value ObsConfigurePreview(const Napi::CallbackInfo& info) {
  blog(LOG_INFO, "ObsConfigurePreview called");

  if (!obs) {
    blog(LOG_ERROR, "ObsConfigurePreview called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 4 &&
    info[0].IsNumber() && // X
    info[1].IsNumber() && // Y
    info[2].IsNumber() && // Width
    info[3].IsNumber(); // Height

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsConfigurePreview").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  int x = info[0].As<Napi::Number>().Int32Value();
  int y = info[1].As<Napi::Number>().Int32Value();
  int width = info[2].As<Napi::Number>().Int32Value();
  int height = info[3].As<Napi::Number>().Int32Value();

  obs->configurePreview(x, y, width, height);
  return info.Env().Undefined();
}

Napi::Value ObsShowPreview(const Napi::CallbackInfo& info) {
  blog(LOG_INFO, "ObsShowPreview called");

  if (!obs) {
    blog(LOG_ERROR, "ObsShowPreview called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->showPreview();
  return info.Env().Undefined();
}

Napi::Value ObsHidePreview(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsHidePreview called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->hidePreview();
  return info.Env().Undefined();
}

Napi::Value ObsDisablePreview(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsDisablePreview called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  obs->disablePreview();
  return info.Env().Undefined();
}

Napi::Value ObsGetPreviewInfo(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsGetPreviewInfo called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  PreviewInfo previewInfo = obs->getPreviewInfo();

  Napi::Object result = Napi::Object::New(info.Env());
  result.Set("canvasWidth", Napi::Number::New(info.Env(), previewInfo.canvasWidth));
  result.Set("canvasHeight", Napi::Number::New(info.Env(), previewInfo.canvasHeight));
  result.Set("previewWidth", Napi::Number::New(info.Env(), previewInfo.displayWidth));
  result.Set("previewHeight", Napi::Number::New(info.Env(), previewInfo.displayHeight));

  return result;
}

Napi::Value ObsCreateSource(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsCreateSource called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 &&
   info[0].IsString() && // Source name
   info[1].IsString();   // Source type

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsCreateSource").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  std::string type = info[1].As<Napi::String>().Utf8Value();

  std::string real_name = obs->createSource(name, type);
  return Napi::String::New(info.Env(), real_name);
}

Napi::Value ObsDeleteSource(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsDeleteSource called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsDeleteSource").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  obs->deleteSource(name);
  return info.Env().Undefined();
}

Napi::Value ObsGetSourceSettings(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsGetSourceSettings called but obs is not initialized");
    Napi::Env env = info.Env();
    Napi::Error::New(env, "Obs not initialized").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsGetSourceSettings").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  obs_data_t* settings = obs->getSourceSettings(name);
  Napi::Object result = data_to_napi(info.Env(), settings);
  obs_data_release(settings);

  return result;
}

Napi::Value ObsSetSourceSettings(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetSourceSettings called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 && info[0].IsString() && info[1].IsObject();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetSourceSettings").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  Napi::Object obj = info[1].As<Napi::Object>();
  obs_data_t* settings = napi_to_data(obj);
  obs->setSourceSettings(name, settings);
  obs_data_release(settings);

  return info.Env().Undefined();
}

Napi::Value ObsGetSourceProperties(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsGetSourceProperties called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsGetSourceProperties").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  obs_properties_t* properties = obs->getSourceProperties(name);
  Napi::Object result = properties_to_napi(info.Env(), properties);
  obs_properties_destroy(properties);

  return result;
}


Napi::Value ObsSetMuteAudioInputs(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetMuteAudioInputs called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBoolean();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetMuteAudioInputs").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  bool mute = info[0].As<Napi::Boolean>().Value();
  obs->setMuteAudioInputs(mute);
  return info.Env().Undefined();
}

Napi::Value ObsSetSourceVolume(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetSourceVolume called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 && info[0].IsString() && info[1].IsNumber();
  std::string name = info[0].As<Napi::String>().Utf8Value();
  float volume = info[1].As<Napi::Number>().FloatValue();

  if (!valid || (volume < 0.0f || volume > 1.0f)) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetOutputVolume").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  obs->setSourceVolume(name,volume);
  return info.Env().Undefined();
}

Napi::Value ObsSetVolmeterEnabled(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetVolmeterEnabled called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBoolean();


  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetVolmeterEnabled").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  bool enabled = info[0].As<Napi::Boolean>().Value();
  obs->setVolmeterEnabled(enabled);
  return info.Env().Undefined();
}

Napi::Value ObsSetAudioSuppression(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetAudioSuppression called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBoolean();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetAudioSuppression").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool enabled = info[0].As<Napi::Boolean>().Value();
  obs->setAudioSuppression(enabled);
  return info.Env().Undefined();
}

Napi::Value ObsSetForceMono(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetForceMono called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsBoolean();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetForceMono").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool enabled = info[0].As<Napi::Boolean>().Value();
  obs->setForceMono(enabled);
  return info.Env().Undefined();
}

Napi::Value ObsAddSourceToScene(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsAddSourceToScene called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsAddSourceToScene").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  obs->addSourceToScene(name);
  return info.Env().Undefined();
}

Napi::Value ObsRemoveSourceFromScene(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsRemoveSourceFromScene called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsRemoveSourceFromScene").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  obs->removeSourceFromScene(name);
  return info.Env().Undefined();
}

Napi::Value ObsGetSourcePos(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsGetSourcePos called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 1 && info[0].IsString();

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsGetSourcePos").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  vec2 pos; vec2 size; vec2 scale; obs_sceneitem_crop crop;
  obs->getSourcePos(name, &pos, &size, &scale, &crop);

  Napi::Object result = Napi::Object::New(info.Env());

  result.Set("x", Napi::Number::New(info.Env(), pos.x));
  result.Set("y", Napi::Number::New(info.Env(), pos.y));

  result.Set("width", Napi::Number::New(info.Env(), size.x));
  result.Set("height", Napi::Number::New(info.Env(), size.y));
  result.Set("scaleX", Napi::Number::New(info.Env(), scale.x));
  result.Set("scaleY", Napi::Number::New(info.Env(), scale.y));

  result.Set("cropLeft", Napi::Number::New(info.Env(), crop.left));
  result.Set("cropRight", Napi::Number::New(info.Env(), crop.right));
  result.Set("cropTop", Napi::Number::New(info.Env(), crop.top));
  result.Set("cropBottom", Napi::Number::New(info.Env(), crop.bottom));

  return result;
}

Napi::Value ObsSetSourcePos(const Napi::CallbackInfo& info) {
  if (!obs) {
    blog(LOG_ERROR, "ObsSetSourcePos called but obs is not initialized");
    Napi::Error::New(info.Env(), "Obs not initialized").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  bool valid = info.Length() == 2 &&
    info[0].IsString() && // Source name
    info[1].IsObject();   // Position definition.

  if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetSourcePos").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();

  Napi::Object position = info[1].As<Napi::Object>();
  float x = position.Get("x").As<Napi::Number>().FloatValue();
  float y = position.Get("y").As<Napi::Number>().FloatValue();
  vec2 pos = { x, y };

  float scaleX = position.Get("scaleX").As<Napi::Number>().FloatValue();
  float scaleY = position.Get("scaleY").As<Napi::Number>().FloatValue();
  vec2 scale = { scaleX, scaleY };

  int cropLeft = position.Get("cropLeft").As<Napi::Number>().Int32Value();
  int cropRight = position.Get("cropRight").As<Napi::Number>().Int32Value();
  int cropTop = position.Get("cropTop").As<Napi::Number>().Int32Value();
  int cropBottom = position.Get("cropBottom").As<Napi::Number>().Int32Value();
  obs_sceneitem_crop crop = { cropLeft, cropTop, cropRight, cropBottom }; // Careful with ordering.

  obs->setSourcePos(name, &pos, &scale, &crop);
  return info.Env().Undefined();
}

Napi::Value ObsSetDrawSourceOutline(const Napi::CallbackInfo& info) {
  bool valid =  info.Length() == 1 && info[0].IsBoolean();
    if (!valid) {
    Napi::TypeError::New(info.Env(), "Invalid arguments passed to ObsSetDrawSourceOutline").ThrowAsJavaScriptException();
    return info.Env().Undefined();  
  }
  bool enabled = info[0].As<Napi::Boolean>();
  blog(LOG_DEBUG, "ObsSetDrawSourceOutline set to %d", enabled);
  obs->setDrawSourceOutline(enabled);
  return info.Env().Undefined();
}

Napi::Value ObsGetDrawSourceOutlineEnabled(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), obs->getDrawSourceOutlineEnabled());
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("Init", Napi::Function::New(env, ObsInit));
  exports.Set("Shutdown", Napi::Function::New(env, ObsShutdown));
  exports.Set("SetRecordingCfg", Napi::Function::New(env, ObsSetRecordingCfg));
  exports.Set("ResetVideoContext", Napi::Function::New(env, ObsResetVideoContext));
  exports.Set("ListVideoEncoders", Napi::Function::New(env, ObsListVideoEncoders));
  exports.Set("SetVideoEncoder", Napi::Function::New(env, ObsSetVideoEncoder));

  exports.Set("SetBuffering", Napi::Function::New(env, ObsSetBuffering));
  exports.Set("StartBuffer", Napi::Function::New(env, ObsStartBuffer));
  exports.Set("StartRecording", Napi::Function::New(env, ObsStartRecording));
  exports.Set("StopRecording", Napi::Function::New(env, ObsStopRecording));
  exports.Set("ForceStopRecording", Napi::Function::New(env, ObsForceStopRecording));
  exports.Set("GetLastRecording", Napi::Function::New(env, ObsGetLastRecording));

  exports.Set("CreateSource", Napi::Function::New(env, ObsCreateSource));
  exports.Set("DeleteSource", Napi::Function::New(env, ObsDeleteSource));
  exports.Set("GetSourceSettings", Napi::Function::New(env, ObsGetSourceSettings));
  exports.Set("SetSourceSettings", Napi::Function::New(env, ObsSetSourceSettings));
  exports.Set("GetSourceProperties", Napi::Function::New(env, ObsGetSourceProperties));
  exports.Set("SetMuteAudioInputs", Napi::Function::New(env, ObsSetMuteAudioInputs));
  exports.Set("SetSourceVolume", Napi::Function::New(env, ObsSetSourceVolume));
  exports.Set("SetVolmeterEnabled", Napi::Function::New(env, ObsSetVolmeterEnabled));
  exports.Set("SetAudioSuppression", Napi::Function::New(env, ObsSetAudioSuppression));
  exports.Set("SetForceMono", Napi::Function::New(env, ObsSetForceMono));

  exports.Set("AddSourceToScene", Napi::Function::New(env, ObsAddSourceToScene));
  exports.Set("RemoveSourceFromScene", Napi::Function::New(env, ObsRemoveSourceFromScene));
  exports.Set("GetSourcePos", Napi::Function::New(env, ObsGetSourcePos));
  exports.Set("SetSourcePos", Napi::Function::New(env, ObsSetSourcePos));

  exports.Set("InitPreview", Napi::Function::New(env, ObsInitPreview));
  exports.Set("ConfigurePreview", Napi::Function::New(env, ObsConfigurePreview));
  exports.Set("ShowPreview", Napi::Function::New(env, ObsShowPreview));
  exports.Set("HidePreview", Napi::Function::New(env, ObsHidePreview));
  exports.Set("DisablePreview", Napi::Function::New(env, ObsDisablePreview));
  exports.Set("GetPreviewInfo", Napi::Function::New(env, ObsGetPreviewInfo));
  exports.Set("GetDrawSourceOutlineEnabled", Napi::Function::New(env, ObsGetDrawSourceOutlineEnabled));
  exports.Set("SetDrawSourceOutline", Napi::Function::New(env, ObsSetDrawSourceOutline));

  return exports;
}

NODE_API_MODULE(addon, Init)