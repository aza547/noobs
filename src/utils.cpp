#include <iostream>
#include <obs.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include "utils.h"
#include <windows.h>

void log_handler(int lvl, const char *msg, va_list args, void *p) {
  static std::mutex logMutex;
  static std::stringstream logFileName;
  static bool logInitialized = false;

  // make logging thread-safe
  std::lock_guard<std::mutex> lock(logMutex);
  
  if (!logInitialized) {
    // Build the log filename.
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);

    std::string logDir = static_cast<const char*>(p);

    if (!logDir.empty() && logDir.back() != '\\' && logDir.back() != '/') {
      logDir += '\\';
    }
      
    logFileName << logDir << "OBS-" << std::put_time(std::localtime(&t), "%Y-%m-%d") << ".log";
    logInitialized = true;
  }

  // Timestamp
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

  std::stringstream timestamp;
  timestamp << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

  // Log level
  const char* level_str = "UNKNOWN";
  switch (lvl) {
      case LOG_ERROR:   level_str = "ERROR"; break;
      case LOG_WARNING: level_str = "WARN";  break;
      case LOG_INFO:    level_str = "INFO";  break;
      case LOG_DEBUG:   level_str = "DEBUG"; break;
  }
  timestamp << "[" << level_str << "]  ";

  // Format message
  char buffer[4096];
  vsnprintf(buffer, sizeof(buffer), msg, args);

  // Prepend timestamp and flush per line
  std::istringstream iss(buffer);
  std::string line;
  std::ofstream logFile(logFileName.str(), std::ios::app);

  while (logFile.is_open() && std::getline(iss, line)) {
    logFile << timestamp.str() << line << std::endl;  // std::endl flushes
  }
}

Napi::Object data_to_napi(Napi::Env env, obs_data_t* data) {
  Napi::Object obj = Napi::Object::New(env);
  
  if (!data) {
    return obj;
  }
  
  obs_data_item_t *item = obs_data_first(data);
  
  for (; item != NULL; obs_data_item_next(&item)) {
    const char* name = obs_data_item_get_name(item);
    enum obs_data_type type = obs_data_item_gettype(item);
    
    switch (type) {
      case OBS_DATA_STRING: {
        const char* str_val = obs_data_item_get_string(item);
        obj.Set(name, Napi::String::New(env, str_val ? str_val : ""));
        break;
      }
      
      case OBS_DATA_NUMBER: {
        double num_val = obs_data_item_get_double(item);
        obj.Set(name, Napi::Number::New(env, num_val));
        break;
      }
      
      case OBS_DATA_BOOLEAN: {
        bool bool_val = obs_data_item_get_bool(item);
        obj.Set(name, Napi::Boolean::New(env, bool_val));
        break;
      }
      
      case OBS_DATA_OBJECT: {
        obs_data_t *child_data = obs_data_item_get_obj(item);
        Napi::Object child_obj = data_to_napi(env, child_data);
        obj.Set(name, child_obj);
        obs_data_release(child_data);
        break;
      }
      
      case OBS_DATA_ARRAY: {
        obs_data_array_t *array = obs_data_item_get_array(item);
        size_t count = obs_data_array_count(array);
        Napi::Array js_array = Napi::Array::New(env, count);
        
        for (size_t i = 0; i < count; i++) {
          obs_data_t *array_item = obs_data_array_item(array, i);
          Napi::Object array_obj = data_to_napi(env, array_item);
          js_array.Set(i, array_obj);
          obs_data_release(array_item);
        }
        
        obj.Set(name, js_array);
        obs_data_array_release(array);
        break;
      }
      
      default:
        // For unknown types or NULL, set as null
        obj.Set(name, env.Null());
        break;
    }
  }
  
  return obj;
}

obs_data_t* napi_to_data(Napi::Object obj) {
  obs_data_t* data = obs_data_create();
  
  if (!data) {
    return nullptr;
  }
  
  Napi::Array prop_names = obj.GetPropertyNames();
  
  for (uint32_t i = 0; i < prop_names.Length(); i++) {
    Napi::Value key = prop_names.Get(i);
    std::string name = key.As<Napi::String>().Utf8Value();
    Napi::Value value = obj.Get(key);
    
    if (value.IsNull() || value.IsUndefined()) {
      // OBS doesn't have a specific "set null" function, just skip
      continue;
    }
    else if (value.IsString()) {
      std::string str_val = value.As<Napi::String>().Utf8Value();
      obs_data_set_string(data, name.c_str(), str_val.c_str());
    }
    else if (value.IsNumber()) {
      double num_val = value.As<Napi::Number>().DoubleValue();
      obs_data_set_double(data, name.c_str(), num_val);
    }
    else if (value.IsBoolean()) {
      bool bool_val = value.As<Napi::Boolean>().Value();
      obs_data_set_bool(data, name.c_str(), bool_val);
    }
    else if (value.IsObject() && !value.IsArray()) {
      Napi::Object child_obj = value.As<Napi::Object>();
      obs_data_t* child_data = napi_to_data(child_obj);
      obs_data_set_obj(data, name.c_str(), child_data);
      obs_data_release(child_data);
    }
    else if (value.IsArray()) {
      Napi::Array js_array = value.As<Napi::Array>();
      obs_data_array_t* array = obs_data_array_create();
      
      for (uint32_t j = 0; j < js_array.Length(); j++) {
        Napi::Value array_item = js_array.Get(j);
        if (array_item.IsObject()) {
          Napi::Object array_obj = array_item.As<Napi::Object>();
          obs_data_t* array_data = napi_to_data(array_obj);
          obs_data_array_push_back(array, array_data);
          obs_data_release(array_data);
        }
      }
      
      obs_data_set_array(data, name.c_str(), array);
      obs_data_array_release(array);
    }
  }
  
  return data;
}

Napi::Array properties_to_napi(Napi::Env env, obs_properties_t* properties) {
  Napi::Array propsArray = Napi::Array::New(env);
  
  if (!properties) {
    blog(LOG_WARNING, "properties_to_napi called with null properties");
    return propsArray;
  }
  
  obs_property_t* property = obs_properties_first(properties);
  uint32_t index = 0;
  
  while (property) {
    Napi::Object propObj = property_to_napi(env, property);
    propsArray.Set(index++, propObj);
    obs_property_next(&property);
  }
  
  blog(LOG_INFO, "Converted %d properties to napi", index);
  return propsArray;
}

Napi::Object property_to_napi(Napi::Env env, obs_property_t* property) {
  Napi::Object obj = Napi::Object::New(env);
  
  if (!property) {
    return obj;
  }
  
  // Basic property information
  const char* name = obs_property_name(property);
  const char* description = obs_property_description(property);
  enum obs_property_type type = obs_property_get_type(property);
  bool enabled = obs_property_enabled(property);
  bool visible = obs_property_visible(property);
  
  obj.Set("name", Napi::String::New(env, name ? name : ""));
  obj.Set("description", Napi::String::New(env, description ? description : ""));
  obj.Set("enabled", Napi::Boolean::New(env, enabled));
  obj.Set("visible", Napi::Boolean::New(env, visible));
  
  // Property type
  std::string type_str;
  switch (type) {
    case OBS_PROPERTY_INVALID: type_str = "invalid"; break;
    case OBS_PROPERTY_BOOL: type_str = "bool"; break;
    case OBS_PROPERTY_INT: type_str = "int"; break;
    case OBS_PROPERTY_FLOAT: type_str = "float"; break;
    case OBS_PROPERTY_TEXT: type_str = "text"; break;
    case OBS_PROPERTY_PATH: type_str = "path"; break;
    case OBS_PROPERTY_LIST: type_str = "list"; break;
    case OBS_PROPERTY_COLOR: type_str = "color"; break;
    case OBS_PROPERTY_BUTTON: type_str = "button"; break;
    case OBS_PROPERTY_FONT: type_str = "font"; break;
    case OBS_PROPERTY_EDITABLE_LIST: type_str = "editable_list"; break;
    case OBS_PROPERTY_FRAME_RATE: type_str = "frame_rate"; break;
    case OBS_PROPERTY_GROUP: type_str = "group"; break;
    case OBS_PROPERTY_COLOR_ALPHA: type_str = "color_alpha"; break;
    default: type_str = "unknown"; break;
  }
  obj.Set("type", Napi::String::New(env, type_str));
  
  // Type-specific properties
  switch (type) {
    case OBS_PROPERTY_INT: {
      int min_val = obs_property_int_min(property);
      int max_val = obs_property_int_max(property);
      int step_val = obs_property_int_step(property);
      enum obs_number_type num_type = obs_property_int_type(property);
      
      obj.Set("min", Napi::Number::New(env, min_val));
      obj.Set("max", Napi::Number::New(env, max_val));
      obj.Set("step", Napi::Number::New(env, step_val));
      
      std::string num_type_str = (num_type == OBS_NUMBER_SCROLLER) ? "scroller" : "slider";
      obj.Set("number_type", Napi::String::New(env, num_type_str));
      break;
    }
    
    case OBS_PROPERTY_FLOAT: {
      double min_val = obs_property_float_min(property);
      double max_val = obs_property_float_max(property);
      double step_val = obs_property_float_step(property);
      enum obs_number_type num_type = obs_property_float_type(property);
      
      obj.Set("min", Napi::Number::New(env, min_val));
      obj.Set("max", Napi::Number::New(env, max_val));
      obj.Set("step", Napi::Number::New(env, step_val));
      
      std::string num_type_str = (num_type == OBS_NUMBER_SCROLLER) ? "scroller" : "slider";
      obj.Set("number_type", Napi::String::New(env, num_type_str));
      break;
    }
    
    case OBS_PROPERTY_TEXT: {
      enum obs_text_type text_type = obs_property_text_type(property);
      std::string text_type_str;
      
      switch (text_type) {
        case OBS_TEXT_DEFAULT: text_type_str = "default"; break;
        case OBS_TEXT_PASSWORD: text_type_str = "password"; break;
        case OBS_TEXT_MULTILINE: text_type_str = "multiline"; break;
        case OBS_TEXT_INFO: text_type_str = "info"; break;
        default: text_type_str = "default"; break;
      }
      
      obj.Set("text_type", Napi::String::New(env, text_type_str));
      break;
    }
    
    case OBS_PROPERTY_PATH: {
      enum obs_path_type path_type = obs_property_path_type(property);
      const char* filter = obs_property_path_filter(property);
      const char* default_path = obs_property_path_default_path(property);
      
      std::string path_type_str;
      switch (path_type) {
        case OBS_PATH_FILE: path_type_str = "file"; break;
        case OBS_PATH_FILE_SAVE: path_type_str = "file_save"; break;
        case OBS_PATH_DIRECTORY: path_type_str = "directory"; break;
        default: path_type_str = "file"; break;
      }
      
      obj.Set("path_type", Napi::String::New(env, path_type_str));
      obj.Set("filter", Napi::String::New(env, filter ? filter : ""));
      obj.Set("default_path", Napi::String::New(env, default_path ? default_path : ""));
      break;
    }
    
    case OBS_PROPERTY_LIST: {
      enum obs_combo_type combo_type = obs_property_list_type(property);
      enum obs_combo_format combo_format = obs_property_list_format(property);
      size_t count = obs_property_list_item_count(property);
      
      std::string combo_type_str;
      switch (combo_type) {
        case OBS_COMBO_TYPE_INVALID: combo_type_str = "invalid"; break;
        case OBS_COMBO_TYPE_EDITABLE: combo_type_str = "editable"; break;
        case OBS_COMBO_TYPE_LIST: combo_type_str = "list"; break;
        case OBS_COMBO_TYPE_RADIO: combo_type_str = "radio"; break;
        default: combo_type_str = "list"; break;
      }
      
      std::string combo_format_str;
      switch (combo_format) {
        case OBS_COMBO_FORMAT_INVALID: combo_format_str = "invalid"; break;
        case OBS_COMBO_FORMAT_INT: combo_format_str = "int"; break;
        case OBS_COMBO_FORMAT_FLOAT: combo_format_str = "float"; break;
        case OBS_COMBO_FORMAT_STRING: combo_format_str = "string"; break;
        default: combo_format_str = "string"; break;
      }
      
      obj.Set("combo_type", Napi::String::New(env, combo_type_str));
      obj.Set("combo_format", Napi::String::New(env, combo_format_str));
      
      // Add list items
      Napi::Array items = Napi::Array::New(env, count);
      for (size_t i = 0; i < count; i++) {
        Napi::Object item = Napi::Object::New(env);
        const char* item_name = obs_property_list_item_name(property, i);
        
        item.Set("name", Napi::String::New(env, item_name ? item_name : ""));
        
        if (combo_format == OBS_COMBO_FORMAT_INT) {
          long long item_int = obs_property_list_item_int(property, i);
          item.Set("value", Napi::Number::New(env, static_cast<double>(item_int)));
        } else if (combo_format == OBS_COMBO_FORMAT_FLOAT) {
          double item_float = obs_property_list_item_float(property, i);
          item.Set("value", Napi::Number::New(env, item_float));
        } else {
          const char* item_string = obs_property_list_item_string(property, i);
          item.Set("value", Napi::String::New(env, item_string ? item_string : ""));
        }
        
        bool disabled = obs_property_list_item_disabled(property, i);
        item.Set("disabled", Napi::Boolean::New(env, disabled));
        
        items.Set(i, item);
      }
      obj.Set("items", items);
      break;
    }
    
    default:
      // For other types, we've already added the basic information
      break;
  }
  
  return obj;
}

std::string get_current_date_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H-%M-%S");
    return ss.str();
}

LRESULT CALLBACK DisplayWndProc(
  _In_ HWND hwnd, 
  _In_ UINT uMsg, 
  _In_ WPARAM wParam, 
  _In_ LPARAM lParam)
{
	switch (uMsg) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void register_preview_window_class() {
  WNDCLASSEX klass;
  
  klass.cbSize = sizeof(WNDCLASSEX);
	klass.style = CS_NOCLOSE | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	klass.lpfnWndProc = DisplayWndProc;
	klass.cbClsExtra = 0;
	klass.cbWndExtra = 0;
	klass.hInstance = GetModuleHandle(NULL);
	klass.hIcon = NULL;
	klass.hCursor = NULL;
	klass.hbrBackground = NULL;
	klass.lpszMenuName = NULL;
	klass.lpszClassName = TEXT("PreviewWindowClass");
	klass.hIconSm = NULL;

	if (RegisterClassEx(&klass) == NULL) {
		blog(LOG_ERROR, "Failed to register window class");
    throw std::runtime_error("Failed to register window class");
	}

  blog(LOG_INFO, "Registered preview window class");
}
