// macOS preview implementation using a transparent child NSWindow.

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

  if (display || preview_child_window || preview_handle) {
    blog(LOG_WARNING, "initPreview: preview already initialized");
    return;
  }

  NSView *parentView = to_view(parentHandle);
  if (!parentView) {
    blog(LOG_ERROR, "initPreview: null parent handle");
    return;
  }
  preview_parent_view = parentHandle;

  __block NSWindow *childWindow = nil;
  __block NSView *canvasView = nil;
  dispatch_block_t setup = ^{
    childWindow = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 1, 1)
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [childWindow setOpaque:NO];
    [childWindow setBackgroundColor:[NSColor blackColor]];
    [childWindow setHasShadow:NO];
    // Let the parent WebContents receive input.
    [childWindow setIgnoresMouseEvents:YES];
    // Don't bring focus when shown.
    [childWindow setHidesOnDeactivate:NO];

    canvasView = [[NSView alloc] initWithFrame:[[childWindow contentView] bounds]];
    [canvasView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [childWindow setContentView:canvasView];

    // Attach lazily once the parent view has a window.
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

  // Retain across the C++ opaque handle lifetime.
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
      destroyPreview();
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

  NSView *parentView = to_view(preview_parent_view);
  if (!parentView) {
    blog(LOG_ERROR, "configurePreview: parent view missing");
    return;
  }

  __block CGFloat backingScale = 1.0;
  __block bool configured = false;
  dispatch_block_t apply = ^{
    NSWindow *parentWindow = [parentView window];
    if (!parentWindow) {
      blog(LOG_ERROR, "configurePreview: parent view has no window yet");
      return;
    }

    NSWindow *currentParent = [childWindow parentWindow];
    if (currentParent != parentWindow) {
      if (currentParent) {
        [currentParent removeChildWindow:childWindow];
      }
      blog(LOG_INFO, "configurePreview: attaching child to %p",
           (__bridge void *)parentWindow);
      [parentWindow addChildWindow:childWindow ordered:NSWindowAbove];
    }

    NSView *contentView = [parentWindow contentView];
    if (!contentView) return;

    // Convert top-left CSS points to Cocoa screen coordinates.
    NSRect viewRect = [contentView bounds];
    CGFloat flippedY = viewRect.size.height - y - height;
    NSRect rectInWindow = NSMakeRect(x, flippedY, width, height);
    NSRect rectInScreen = [parentWindow convertRectToScreen:rectInWindow];

    [childWindow setFrame:rectInScreen display:YES];
    [childWindow orderFront:nil];

    backingScale = [parentWindow backingScaleFactor];
    configured = true;
  };
  if ([NSThread isMainThread]) {
    apply();
  } else {
    dispatch_sync(dispatch_get_main_queue(), apply);
  }

  if (!configured) {
    return;
  }

  // obs_display_resize takes backing pixels.
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

  // Hide without destroying the preview resources.
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

void ObsInterface::destroyPreview() {
  blog(LOG_INFO, "ObsInterface::destroyPreview (mac)");

  if (display) {
    obs_display_remove_draw_callback(display, draw_callback, this);
    obs_display_destroy(display);
    display = nullptr;
  }

  NSWindow *childWindow = to_window(preview_child_window);
  dispatch_block_t cleanup = ^{
    if (!childWindow) return;

    NSWindow *parentWindow = [childWindow parentWindow];
    if (parentWindow) {
      [parentWindow removeChildWindow:childWindow];
    }
    [childWindow orderOut:nil];
  };
  if ([NSThread isMainThread]) {
    cleanup();
  } else {
    dispatch_sync(dispatch_get_main_queue(), cleanup);
  }

  if (preview_handle) {
    id retainedCanvasView = CFBridgingRelease(
        reinterpret_cast<void *>(preview_handle));
    (void)retainedCanvasView;
    preview_handle = 0;
  }

  if (preview_child_window) {
    id retainedChildWindow = CFBridgingRelease(
        reinterpret_cast<void *>(preview_child_window));
    (void)retainedChildWindow;
    preview_child_window = 0;
  }

  preview_parent_view = 0;
  preview_backing_scale = 1.0;
}
