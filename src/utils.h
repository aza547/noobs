#pragma once

#include <napi.h>
#include <obs.h>

#ifdef _WIN32
void register_preview_window_class();
#endif
void log_handler(int lvl, const char *msg, va_list args, void *p);

Napi::Object data_to_napi(Napi::Env env,obs_data_t* data);
obs_data_t* napi_to_data(Napi::Object obj);

Napi::Object property_to_napi(Napi::Env env, obs_property_t* property);
Napi::Array properties_to_napi(Napi::Env env, obs_properties_t* properties);
std::string get_current_date_time();