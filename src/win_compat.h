#pragma once
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  // // TODO: STUBBING OUT EVERYTHING
  // #ifndef HWND
  //   struct HWND__ { int unused; };
  //   using HWND = HWND__*;
  // #endif
  // typedef intptr_t LONG_PTR;

  // #ifndef TEXT
  //   #define TEXT(x) x
  // #endif

  // static constexpr unsigned int WS_POPUP          = 0x80000000u;
  // static constexpr unsigned int WS_CHILD          = 0x40000000u;
  // static constexpr unsigned int WS_EX_TRANSPARENT = 0x00000020u;

  // static constexpr int GWL_STYLE   = -16;
  // static constexpr int GWL_EXSTYLE = -20;

  // static constexpr unsigned int SWP_NOACTIVATE = 0x0010u;

  // static constexpr int SW_SHOW = 5;
  // static constexpr int SW_HIDE = 0;

  // // Symbols only (signatures are intentionally minimal)
  // inline void* GetModuleHandle(void*) { return nullptr; }

  // inline HWND CreateWindowEx(
  //     unsigned int, const char*, const char*, unsigned int,
  //     int, int, int, int,
  //     HWND, void*, void*, void*)
  // {
  //   return nullptr;
  // }

  // inline HWND SetParent(HWND, HWND) { return nullptr; }

  // inline LONG_PTR GetWindowLongPtr(HWND, int) { return 0; }
  // inline LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR) { return 0; }

  // inline int SetWindowPos(HWND, HWND, int, int, int, int, unsigned int) { return 0; }

  // inline int ShowWindow(HWND, int) { return 0; }
  // // TODO: END STUBBING
#endif
