//
//  PLSTestInteractionWindow.h
//  obs-browser
//
//  Created by jimbo on 2023/4/26.
//

#pragma once
#define GL_SILENCE_DEPRECATION
#import <Cocoa/Cocoa.h>
#include "../obs-browser-source.hpp"
#include "text_input_client_osr_mac.h"
#include "interaction_delegate.h"

struct BrowserSource;

NS_ASSUME_NONNULL_BEGIN

@interface BrowserOpenGLView
	: NSOpenGLView <NSDraggingSource, NSDraggingDestination> {
@private
	NSTrackingArea *tracking_area_;
	BrowserSource *browser_window_;
	NSPoint last_mouse_pos_;
	NSPoint cur_mouse_pos_;
	bool rotating_;

	float device_scale_factor_;

	// Drag and drop.
	CefRefPtr<CefDragData> current_drag_data_;
	NSDragOperation current_drag_op_;
	NSDragOperation current_allowed_ops_;
	NSPasteboard *pasteboard_;
	NSString *fileUTI_;

	// For intreacting with IME.
	NSTextInputContext *text_input_context_osr_mac_;
	CefTextInputClientOSRMac *text_input_client_;

	// Event monitor for scroll wheel end event.
	id endWheelMonitor_;
}
- (id)initWithFrame:(NSRect)frame
	andBrowserWindow:(BrowserSource *)browser_window;
- (void)resetDeviceScaleFactor;
@end // @interface BrowserOpenGLView

@interface PLSInteractionMacWindow : NSWindowController

- (void)setupData:(BrowserSource *)bs
	srcDelegate:(InteractionDelegate *)srcDelegate;

- (NSView *)getOpenGLView;
- (NSSize)getPixelSize;
- (bool)isSameSource:(BrowserSource *)bs_;

@end

NS_ASSUME_NONNULL_END
