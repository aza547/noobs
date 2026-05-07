// macOS-specific preview implementation. ObjC++ to deal with Cocoa.
//
// Architecture (Path B, the nwr-style child-window pattern):
//   - JS hands us a parent NSView pointer (BrowserWindow's contentView,
//     fetched via Electron's getNativeWindowHandle).
//   - We grab its parent NSWindow and create a borderless,
//     mouse-transparent child NSWindow that we addChildWindow:above
//     onto the parent. The child window tracks the parent's screen
//     position automatically.
//   - The child window's contentView holds the OBS canvas; libobs
//     renders into it via obs_display_create. Because the child is a
//     separate NSWindow, libobs's NSOpenGLContext binds cleanly without
//     fighting layer-backing rules.
//   - Mouse events: child NSWindow has setIgnoresMouseEvents:YES, so
//     clicks fall through to the parent's WebContents view, where the
//     renderer's transparent <div> forwards them to EditorService over
//     IPC.
//
// Win uses node-window-rendering for the same effect; this is the Mac
// equivalent.

#import <Cocoa/Cocoa.h>
#include <obs.h>
#include "obs_interface.h"
#include "utils.h"

extern void draw_callback(void* data, uint32_t cx, uint32_t cy);

namespace {
NSView *to_view(uintptr_t handle) {
  return (__bridge NSView *)reinterpret_cast<void *>(handle);
}

NSWindow *to_window(uintptr_t handle) {
  return (__bridge NSWindow *)reinterpret_cast<void *>(handle);
}
}

void ObsInterface::initPreview(uintptr_t parentHandle) {
  blog(LOG_INFO, "ObsInterface::initPreview (mac) parent=%p",
       reinterpret_cast<void *>(parentHandle));

  NSView *parentView = to_view(parentHandle);
  if (!parentView) {
    blog(LOG_ERROR, "initPreview: null parent handle");
    return;
  }

  __block NSWindow *childWindow = nil;
  __block NSView *canvasView = nil;
  dispatch_block_t setup = ^{
    NSWindow *parentWindow = [parentView window];
    if (!parentWindow) {
      blog(LOG_ERROR, "initPreview: parent view has no window yet");
      return;
    }

    // Borderless transparent child window. Sized 1x1 here; configurePreview
    // resizes/positions it.
    childWindow = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 1, 1)
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [childWindow setOpaque:NO];
    [childWindow setBackgroundColor:[NSColor blackColor]];
    [childWindow setHasShadow:NO];
    // Mouse-transparent so clicks fall through to the parent's
    // WebContents → renderer's editor overlay div → EditorService IPC.
    [childWindow setIgnoresMouseEvents:YES];
    // Don't bring focus when shown.
    [childWindow setHidesOnDeactivate:NO];

    canvasView = [[NSView alloc] initWithFrame:[[childWindow contentView] bounds]];
    [canvasView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [childWindow setContentView:canvasView];

    // addChildWindow happens lazily on first configurePreview because
    // the parent BrowserWindow may not be on screen yet at init time
    // (Electron fires 'ready-to-show' after our initPreview runs);
    // attaching a child to a not-yet-on-screen parent has been
    // observed to corrupt later position calls.
  };
  if ([NSThread isMainThread]) {
    setup();
  } else {
    dispatch_sync(dispatch_get_main_queue(), setup);
  }

  if (!canvasView || !childWindow) {
    blog(LOG_ERROR, "initPreview: failed to create child window");
    return;
  }

  // CFBridgingRetain transfers ownership to CF (i.e. retains under
  // ARC) so the void* we stash in the C++ side keeps the window
  // alive after this method returns. addChildWindow also retains,
  // but relying solely on Cocoa's internal retain has bitten us
  // before — explicit ownership keeps the lifetime tied to the
  // ObsInterface instance.
  preview_handle = reinterpret_cast<uintptr_t>(CFBridgingRetain(canvasView));
  preview_child_window =
      reinterpret_cast<uintptr_t>(CFBridgingRetain(childWindow));

  if (!display) {
    gs_init_data gs_data = {};
    gs_data.cx = 1920;
    gs_data.cy = 1080;
    gs_data.format = GS_BGRA;
    gs_data.zsformat = GS_ZS_NONE;
    gs_data.num_backbuffers = 1;
    gs_data.window.view = (__bridge id)(__bridge void *)canvasView;

    display = obs_display_create(&gs_data, 0x0);
    if (!display) {
      blog(LOG_ERROR, "Failed to create OBS display on mac");
      return;
    }
    obs_display_add_draw_callback(display, draw_callback, this);
  }

  obs_display_set_enabled(display, false);
}

void ObsInterface::configurePreview(int x, int y, int width, int height) {
  blog(LOG_INFO,
       "ObsInterface::configurePreview (mac) x=%d y=%d w=%d h=%d",
       x, y, width, height);

  NSWindow *childWindow = to_window(preview_child_window);
  if (!childWindow) {
    blog(LOG_ERROR, "configurePreview: child window not initialized");
    return;
  }
  if (!display) {
    blog(LOG_ERROR, "configurePreview: display not initialized");
    return;
  }

  // Need the parent BrowserWindow's NSWindow. The renderer-side
  // initPreview handed us its contentView pointer; recover the
  // window from it. Cached as preview_handle's superview chain root
  // through canvasView's window once attached.
  NSView *canvasView = (__bridge NSView *)reinterpret_cast<void *>(preview_handle);
  if (!canvasView) {
    blog(LOG_ERROR, "configurePreview: canvas view missing");
    return;
  }

  __block CGFloat backingScale = 1.0;
  dispatch_block_t apply = ^{
    // Find the parent NSWindow. addChildWindow hasn't necessarily run
    // yet — fall back to walking the host process's keyWindow /
    // mainWindow when we don't have a parent linkage.
    NSWindow *parentWindow = [childWindow parentWindow];
    if (!parentWindow) {
      parentWindow = [NSApp mainWindow];
      if (!parentWindow) parentWindow = [NSApp keyWindow];
      if (parentWindow) {
        blog(LOG_INFO, "configurePreview: attaching child to %p (mainWindow)",
             (__bridge void *)parentWindow);
        [parentWindow addChildWindow:childWindow ordered:NSWindowAbove];
      } else {
        blog(LOG_ERROR, "configurePreview: no parent window available yet");
        return;
      }
    }

    NSView *contentView = [parentWindow contentView];
    if (!contentView) return;

    // (x, y, w, h) come in CSS px (points), top-left origin, relative
    // to the parent contentView. Convert to screen coords (Cocoa
    // bottom-left).
    NSRect viewRect = [contentView bounds];
    CGFloat flippedY = viewRect.size.height - y - height;
    NSRect rectInWindow = NSMakeRect(x, flippedY, width, height);
    NSRect rectInScreen = [parentWindow convertRectToScreen:rectInWindow];

    [childWindow setFrame:rectInScreen display:YES];
    [childWindow orderFront:nil];

    backingScale = [parentWindow backingScaleFactor];
  };
  if ([NSThread isMainThread]) {
    apply();
  } else {
    dispatch_sync(dispatch_get_main_queue(), apply);
  }

  // obs_display_resize takes BACKING pixels.
  preview_backing_scale = backingScale;
  obs_display_resize(display, width * backingScale, height * backingScale);
  obs_display_set_enabled(display, true);
}

void ObsInterface::showPreview() {
  blog(LOG_INFO, "ObsInterface::showPreview (mac)");

  NSWindow *childWindow = to_window(preview_child_window);
  if (!childWindow) {
    blog(LOG_ERROR, "showPreview: child window not initialized");
    return;
  }
  if (!display) {
    blog(LOG_ERROR, "showPreview: display not initialized");
    return;
  }

  dispatch_block_t apply = ^{ [childWindow orderFront:nil]; };
  if ([NSThread isMainThread]) {
    apply();
  } else {
    dispatch_sync(dispatch_get_main_queue(), apply);
  }
  obs_display_set_enabled(display, true);
}

void ObsInterface::hidePreview() {
  blog(LOG_INFO, "ObsInterface::hidePreview (mac)");

  NSWindow *childWindow = to_window(preview_child_window);
  if (!childWindow) return;

  // Take child window off-screen for the "preview paused" case
  // (dropdowns). orderOut hides without destroying; orderFront in
  // showPreview brings it back.
  dispatch_block_t apply = ^{ [childWindow orderOut:nil]; };
  if ([NSThread isMainThread]) {
    apply();
  } else {
    dispatch_sync(dispatch_get_main_queue(), apply);
  }
  if (display) {
    obs_display_set_enabled(display, false);
  }
}

void ObsInterface::disablePreview() {
  blog(LOG_INFO, "ObsInterface::disablePreview (mac)");
  hidePreview();
}
