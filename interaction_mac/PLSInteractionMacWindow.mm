//
//  PLSTestInteractionWindow.m
//  obs-browser
//
//  Created by jimbo on 2023/4/26.
//

#import "PLSInteractionMacWindow.h"
#include "geometry_util.h"
#include "obs-frontend-api.h"

NSString *const kCEFDragDummyPboardType = @"org.CEF.drag-dummy-type";
NSString *const kNSURLTitlePboardType = @"public.url-name";
#define NSColorFromRGB(rgbValue)                                              \
	[NSColor colorWithSRGBRed:((float)((rgbValue & 0xFF0000) >> 16)) /    \
				  255.0                                       \
			    green:((float)((rgbValue & 0xFF00) >> 8)) / 255.0 \
			     blue:((float)(rgbValue & 0xFF)) / 255.0          \
			    alpha:1.0]

@interface PLSInteractionMacWindow () <NSWindowDelegate> {
	BrowserSource *_browser_source;
	BrowserOpenGLView *native_browser_view_;
	InteractionDelegate *_srcDelegate;
}
@end

static void onPrismAppQuit(enum obs_frontend_event event, void *context)
{
	if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		PLSInteractionMacWindow *view =
			(__bridge PLSInteractionMacWindow *)context;
		if (view) {
			[view close];
		}
	}
}
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

@implementation PLSInteractionMacWindow

- (instancetype)init
{
	self = [super init];

	NSRect re = NSMakeRect(0, 0, 600, 600);
	NSWindow *wi = [[NSWindow alloc]
		initWithContentRect:re
			  styleMask:NSWindowStyleMaskTitled |
				    NSWindowStyleMaskResizable |
				    NSWindowStyleMaskClosable |
				    NSWindowStyleMaskMiniaturizable |
				    NSWindowStyleMaskFullSizeContentView
			    backing:NSBackingStoreBuffered
			      defer:false];

	self.window = wi;
	self.window.delegate = self;
	self.window.titlebarAppearsTransparent = YES;
	self.window.contentView.wantsLayer = YES;
	self.window.contentView.layer.backgroundColor =
		NSColorFromRGB(0X272727).CGColor;
	NSAppearance *appearance =
		[NSAppearance appearanceNamed:NSAppearanceNameVibrantDark];
	[self.window setAppearance:appearance];
	self.window.minSize = NSMakeSize(100, 100);
	_browser_source = nullptr;
	obs_frontend_add_event_callback(onPrismAppQuit, (__bridge void *)self);

	return self;
}

- (void)setupData:(BrowserSource *)bs
	srcDelegate:(InteractionDelegate *)srcDelegate
{
	_srcDelegate = srcDelegate;
	_browser_source = bs;
	self.window.movableByWindowBackground = YES;

	NSRect re = self.window.contentView.bounds;
	int offset = 11;
	re.origin.x = offset;
	re.origin.y = offset;
	re.size.width = re.size.width - 2 * offset;
	re.size.height = re.size.height - 4 * offset;

	native_browser_view_ = [[BrowserOpenGLView alloc] initWithFrame:re
						       andBrowserWindow:bs];
	native_browser_view_.autoresizingMask =
		(NSViewWidthSizable | NSViewHeightSizable);
	native_browser_view_.autoresizesSubviews = YES;
	[self.window.contentView addSubview:native_browser_view_];

	// Determine the default scale factor.
	[native_browser_view_ resetDeviceScaleFactor];
}

- (NSView *)getOpenGLView
{
	return native_browser_view_;
}
- (NSSize)getPixelSize
{
	return [native_browser_view_
		convertSizeToBacking:native_browser_view_.frame.size];
}

- (bool)isSameSource:(BrowserSource *)bs_
{
	return bs_ == _browser_source;
}

- (void)windowWillClose:(NSNotification *)notification
{
	obs_frontend_remove_event_callback(onPrismAppQuit,
					   (__bridge void *)self);
	self.window.delegate = nil;
	if (_srcDelegate) {
		_srcDelegate->interactionWindowClosed();
	}
}

- (void)windowDidLoad
{
	[super windowDidLoad];
}

- (void)dealloc
{
	blog(LOG_INFO, "PLSInteractionMacWindow dealloced");
}

- (void)windowDidResize:(NSNotification *)notification
{
	if (native_browser_view_) {
		[native_browser_view_ resetDeviceScaleFactor];
	}
	if (_srcDelegate) {
		_srcDelegate->interactionWindowResized();
	}
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
	[self windowDidResize:notification];
}

@end

@implementation BrowserOpenGLView

- (id)initWithFrame:(NSRect)frame
	andBrowserWindow:(BrowserSource *)browser_window
{
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc]
		initWithAttributes:(NSOpenGLPixelFormatAttribute[]){
					   NSOpenGLPFADoubleBuffer,
					   NSOpenGLPFADepthSize, 32, 0}];

	if (self = [super initWithFrame:frame pixelFormat:pixelFormat]) {
		browser_window_ = browser_window;
		rotating_ = false;
		endWheelMonitor_ = nil;
		device_scale_factor_ = 1.0f;

		tracking_area_ = [[NSTrackingArea alloc]
			initWithRect:frame
			     options:NSTrackingMouseMoved |
				     NSTrackingActiveInActiveApp |
				     NSTrackingInVisibleRect
			       owner:self
			    userInfo:nil];
		[self addTrackingArea:tracking_area_];

		// enable HiDPI buffer
		[self setWantsBestResolutionOpenGLSurface:YES];

		[self resetDragDrop];
		NSArray *types =
			[NSArray arrayWithObjects:kCEFDragDummyPboardType,
						  NSPasteboardTypeString,
						  NSFilenamesPboardType,
						  NSPasteboardTypeString, nil];
		[self registerForDraggedTypes:types];
	}

	return self;
}

- (void)dealloc
{
}

- (void)detach
{
	browser_window_ = nullptr;
	if (text_input_client_)
		[text_input_client_ detach];
}

- (CefRefPtr<CefBrowser>)getBrowser
{
	if (browser_window_)
		return browser_window_->GetBrowser();
	return nullptr;
}

- (void)setFrame:(NSRect)frameRect
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	[super setFrame:frameRect];
	browser->GetHost()->WasResized();
}

- (void)sendMouseClick:(NSEvent *)event
		button:(CefBrowserHost::MouseButtonType)type
		  isUp:(bool)isUp
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	CefMouseEvent mouseEvent;
	[self getMouseEvent:mouseEvent forEvent:event];

	// |point| is in OS X view coordinates.
	NSPoint point = [self getClickPointForEvent:event];

	// Convert to device coordinates.
	point = [self convertPointToBackingInternal:point];
	browser->GetHost()->SendMouseClickEvent(mouseEvent, type, isUp,
						(int)[event clickCount]);
}

- (void)mouseDown:(NSEvent *)event
{
	[self sendMouseClick:event button:MBT_LEFT isUp:false];
}

- (void)rightMouseDown:(NSEvent *)event
{
	if ([event modifierFlags] & NSEventModifierFlagShift) {
		// Start rotation effect.
		last_mouse_pos_ = cur_mouse_pos_ =
			[self getClickPointForEvent:event];
		rotating_ = true;
		return;
	}

	[self sendMouseClick:event button:MBT_RIGHT isUp:false];
}

- (void)otherMouseDown:(NSEvent *)event
{
	[self sendMouseClick:event button:MBT_MIDDLE isUp:false];
}

- (void)mouseUp:(NSEvent *)event
{
	[self sendMouseClick:event button:MBT_LEFT isUp:true];
}

- (void)rightMouseUp:(NSEvent *)event
{
	if (rotating_) {
		rotating_ = false;
		[self setNeedsDisplay:YES];
		return;
	}
	[self sendMouseClick:event button:MBT_RIGHT isUp:true];
}

- (void)otherMouseUp:(NSEvent *)event
{
	[self sendMouseClick:event button:MBT_MIDDLE isUp:true];
}

- (bool)GetSourceRelativeXY:(int)mouseX
		     mouseY:(int)mouseY
		       relX:(int &)relX
		       relY:(int &)relY
{
	int mouseXscaled = (int)roundf(mouseX);
	int mouseYscaled = (int)roundf(mouseY);

	NSSize size = [self convertSizeToBacking:(self.frame.size)];

	uint32_t sourceCX =
		fmax(obs_source_get_width(browser_window_->source), 1u);
	uint32_t sourceCY =
		fmax(obs_source_get_height(browser_window_->source), 1u);

	int x, y;
	float scale;

	client::GetScaleAndCenterPos(sourceCX, sourceCY, size.width,
				     size.height, x, y, scale);

	if (x > 0) {
		relX = int(float(mouseXscaled - x) / scale);
		relY = int(float(mouseYscaled / scale));
	} else {
		relX = int(float(mouseXscaled / scale));
		relY = int(float(mouseYscaled - y) / scale);
	}

	// Confirm mouse is inside the source
	if (relX < 0 || relX > int(sourceCX))
		return false;
	if (relY < 0 || relY > int(sourceCY))
		return false;

	return true;
}

- (bool)getMouseEvent:(CefMouseEvent &)mouseEvent forEvent:(NSEvent *)event
{

	// |point| is in OS X view coordinates.
	NSPoint point = [self getClickPointForEvent:event];
	// Convert to device coordinates.
	point = [self convertPointToBackingInternal:point];
	bool mouseLeave = ![self GetSourceRelativeXY:point.x
					      mouseY:point.y
						relX:mouseEvent.x
						relY:mouseEvent.y];
	mouseEvent.modifiers = [self getModifiersForEvent:event];
	return mouseLeave;
}

- (void)mouseMoved:(NSEvent *)event
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	if (rotating_) {
		// Apply rotation effect.
		cur_mouse_pos_ = [self getClickPointForEvent:event];

		last_mouse_pos_ = cur_mouse_pos_;
		[self setNeedsDisplay:YES];
		return;
	}

	CefMouseEvent mouseEvent;
	bool isLeave = [self getMouseEvent:mouseEvent forEvent:event];
	browser->GetHost()->SendMouseMoveEvent(mouseEvent, isLeave);
}

- (void)mouseDragged:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)mouseEntered:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)mouseExited:(NSEvent *)event
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	CefMouseEvent mouseEvent;
	bool isLeave = [self getMouseEvent:mouseEvent forEvent:event];

	browser->GetHost()->SendMouseMoveEvent(mouseEvent, isLeave);
}

- (void)keyDown:(NSEvent *)event
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get() || !text_input_context_osr_mac_)
		return;

	if ([event type] != NSEventTypeFlagsChanged) {
		if (text_input_client_) {
			[text_input_client_
				HandleKeyEventBeforeTextInputClient:event];

			// The return value of this method seems to always be set to YES, thus we
			// ignore it and ask the host view whether IME is active or not.
			[text_input_context_osr_mac_ handleEvent:event];

			CefKeyEvent keyEvent;
			[self getKeyEvent:keyEvent forEvent:event];

			[text_input_client_
				HandleKeyEventAfterTextInputClient:keyEvent];
		}
	}
}

// OSX does not have touch screens, so we emulate it by mapping multitouch
// events on TrackPad to Touch Events on Screen. To ensure it does not
// interfere with other Trackpad events, this mapping is only enabled if
// touch-events=enabled commandline is passed and caps lock key is on.
- (void)toggleTouchEmulation:(NSEvent *)event
{
	if ([event type] == NSEventTypeFlagsChanged &&
	    [event keyCode] == 0x39) {
		NSUInteger flags = [event modifierFlags];
		BOOL touch_enabled = flags & NSEventModifierFlagCapsLock ? YES
									 : NO;
		[self setAcceptsTouchEvents:touch_enabled];
	}
}

- (void)keyUp:(NSEvent *)event
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	CefKeyEvent keyEvent;
	[self getKeyEvent:keyEvent forEvent:event];

	keyEvent.type = KEYEVENT_KEYUP;
	browser->GetHost()->SendKeyEvent(keyEvent);
}

- (void)flagsChanged:(NSEvent *)event
{
	if ([self isKeyUpEvent:event])
		[self keyUp:event];
	else
		[self keyDown:event];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent *)event
{
	if ([event phase] != NSEventPhaseEnded &&
	    [event phase] != NSEventPhaseCancelled)
		return;

	[self sendScrollWheelEvet:event];

	if (endWheelMonitor_) {
		[NSEvent removeMonitor:endWheelMonitor_];
		endWheelMonitor_ = nil;
	}
}

- (void)scrollWheel:(NSEvent *)event
{
	// Use an NSEvent monitor to listen for the wheel-end end. This ensures that
	// the event is received even when the mouse cursor is no longer over the
	// view when the scrolling ends. Also it avoids sending duplicate scroll
	// events to the renderer.
	if ([event phase] == NSEventPhaseBegan && !endWheelMonitor_) {
		endWheelMonitor_ = [NSEvent
			addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
						     handler:^(
							     NSEvent *blockEvent) {
							     [self shortCircuitScrollWheelEvent:
									     blockEvent];
							     return blockEvent;
						     }];
	}

	[self sendScrollWheelEvet:event];
}

- (void)sendScrollWheelEvet:(NSEvent *)event
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (!browser.get())
		return;

	CGEventRef cgEvent = [event CGEvent];
	DCHECK(cgEvent);

	int deltaX = (int)CGEventGetIntegerValueField(
		cgEvent, kCGScrollWheelEventPointDeltaAxis2);
	int deltaY = (int)CGEventGetIntegerValueField(
		cgEvent, kCGScrollWheelEventPointDeltaAxis1);

	CefMouseEvent mouseEvent;
	[self getMouseEvent:mouseEvent forEvent:event];

	browser->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX, deltaY);
}

- (BOOL)canBecomeKeyView
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	return (browser.get() != nullptr);
}

- (BOOL)acceptsFirstResponder
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	return (browser.get() != nullptr);
}

- (BOOL)becomeFirstResponder
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get()) {
		browser->GetHost()->SetFocus(true);
		return [super becomeFirstResponder];
	}

	return NO;
}

- (BOOL)resignFirstResponder
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get()) {
		browser->GetHost()->SetFocus(false);
		return [super resignFirstResponder];
	}

	return NO;
}

- (void)undo:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Undo();
}

- (void)redo:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Redo();
}

- (void)cut:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Cut();
}

- (void)copy:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Copy();
}

- (void)paste:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Paste();
}

- (void)delete:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->Delete();
}

- (void)selectAll:(id)sender
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser.get())
		browser->GetFocusedFrame()->SelectAll();
}

- (NSPoint)getClickPointForEvent:(NSEvent *)event
{
	NSPoint windowLocal = [event locationInWindow];
	NSPoint contentLocal = [self convertPoint:windowLocal fromView:nil];

	NSPoint point;
	point.x = contentLocal.x;
	point.y = [self frame].size.height - contentLocal.y; // Flip y.
	return point;
}

- (void)getKeyEvent:(CefKeyEvent &)keyEvent forEvent:(NSEvent *)event
{
	if ([event type] == NSEventTypeKeyDown ||
	    [event type] == NSEventTypeKeyUp) {
		NSString *s = [event characters];
		if ([s length] > 0)
			keyEvent.character = [s characterAtIndex:0];

		s = [event charactersIgnoringModifiers];
	}

	if ([event type] == NSEventTypeFlagsChanged) {
		keyEvent.character = 0;
		keyEvent.unmodified_character = 0;
	}

	keyEvent.native_key_code = [event keyCode];

	keyEvent.modifiers = [self getModifiersForEvent:event];
}

- (NSTextInputContext *)inputContext
{
	if (!text_input_context_osr_mac_) {
		text_input_client_ = [[CefTextInputClientOSRMac alloc]
			initWithBrowser:[self getBrowser]
			      superView:self];
		text_input_context_osr_mac_ = [[NSTextInputContext alloc]
			initWithClient:text_input_client_];
	}

	return text_input_context_osr_mac_;
}

- (void)getMouseEvent:(CefMouseEvent &)mouseEvent
	  forDragInfo:(id<NSDraggingInfo>)info
{
	const float device_scale_factor = [self getDeviceScaleFactor];

	// |point| is in OS X view coordinates.
	NSPoint windowPoint = [info draggingLocation];
	NSPoint point = [self flipWindowPointToView:windowPoint];

	// Convert to device coordinates.
	point = [self convertPointToBackingInternal:point];

	// Convert to browser view coordinates.
	mouseEvent.x = client::DeviceToLogical(point.x, device_scale_factor);
	mouseEvent.y = client::DeviceToLogical(point.y, device_scale_factor);

	mouseEvent.modifiers = (uint32)[NSEvent modifierFlags];
}

- (int)getModifiersForEvent:(NSEvent *)event
{
	int modifiers = 0;

	if ([event modifierFlags] & NSEventModifierFlagControl)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if ([event modifierFlags] & NSEventModifierFlagShift)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if ([event modifierFlags] & NSEventModifierFlagOption)
		modifiers |= EVENTFLAG_ALT_DOWN;
	if ([event modifierFlags] & NSEventModifierFlagCommand)
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	if ([event modifierFlags] & NSEventModifierFlagCapsLock)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	if ([event type] == NSEventTypeKeyUp ||
	    [event type] == NSEventTypeKeyDown ||
	    [event type] == NSEventTypeFlagsChanged) {
		// Only perform this check for key events
		if ([self isKeyPadEvent:event])
			modifiers |= EVENTFLAG_IS_KEY_PAD;
	}

	// OS X does not have a modifier for NumLock, so I'm not entirely sure how to
	// set EVENTFLAG_NUM_LOCK_ON;
	//
	// There is no EVENTFLAG for the function key either.

	// Mouse buttons
	switch ([event type]) {
	case NSEventTypeLeftMouseDragged:
	case NSEventTypeLeftMouseDown:
	case NSEventTypeLeftMouseUp:
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		break;
	case NSEventTypeRightMouseDragged:
	case NSEventTypeRightMouseDown:
	case NSEventTypeRightMouseUp:
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
		break;
	case NSEventTypeOtherMouseDragged:
	case NSEventTypeOtherMouseDown:
	case NSEventTypeOtherMouseUp:
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
		break;
	default:
		break;
	}

	return modifiers;
}

- (BOOL)isKeyUpEvent:(NSEvent *)event
{
	if ([event type] != NSEventTypeFlagsChanged)
		return [event type] == NSEventTypeKeyUp;

	// FIXME: This logic fails if the user presses both Shift keys at once, for
	// example: we treat releasing one of them as keyDown.
	switch ([event keyCode]) {
	case 54: // Right Command
	case 55: // Left Command
		return ([event modifierFlags] & NSEventModifierFlagCommand) ==
		       0;

	case 57: // Capslock
		return ([event modifierFlags] & NSEventModifierFlagCapsLock) ==
		       0;

	case 56: // Left Shift
	case 60: // Right Shift
		return ([event modifierFlags] & NSEventModifierFlagShift) == 0;

	case 58: // Left Alt
	case 61: // Right Alt
		return ([event modifierFlags] & NSEventModifierFlagOption) == 0;

	case 59: // Left Ctrl
	case 62: // Right Ctrl
		return ([event modifierFlags] & NSEventModifierFlagControl) ==
		       0;

	case 63: // Function
		return ([event modifierFlags] & NSEventModifierFlagFunction) ==
		       0;
	}
	return false;
}

- (BOOL)isKeyPadEvent:(NSEvent *)event
{
	if ([event modifierFlags] & NSEventModifierFlagNumericPad)
		return true;

	switch ([event keyCode]) {
	case 71: // Clear
	case 81: // =
	case 75: // /
	case 67: // *
	case 78: // -
	case 69: // +
	case 76: // Enter
	case 65: // .
	case 82: // 0
	case 83: // 1
	case 84: // 2
	case 85: // 3
	case 86: // 4
	case 87: // 5
	case 88: // 6
	case 89: // 7
	case 91: // 8
	case 92: // 9
		return true;
	}

	return false;
}

- (void)drawRect:(NSRect)dirtyRect
{
	CefRefPtr<CefBrowser> browser = [self getBrowser];

	// The Invalidate below fixes flicker when resizing.
	if ([self inLiveResize] && browser.get())
		browser->GetHost()->Invalidate(PET_VIEW);
}

// Drag and drop

- (BOOL)startDragging:(CefRefPtr<CefDragData>)drag_data
	   allowedOps:(NSDragOperation)ops
		point:(NSPoint)position
{
	DCHECK(!pasteboard_);
	DCHECK(!fileUTI_);
	DCHECK(!current_drag_data_.get());

	[self resetDragDrop];

	current_allowed_ops_ = ops;
	current_drag_data_ = drag_data;

	[self fillPasteboard];

	NSEvent *currentEvent =
		[[NSApplication sharedApplication] currentEvent];
	NSWindow *window = [self window];
	NSTimeInterval eventTime = [currentEvent timestamp];

	NSEvent *dragEvent = [NSEvent
		mouseEventWithType:NSEventTypeLeftMouseDragged
			  location:position
		     modifierFlags:NSEventMaskLeftMouseDragged
			 timestamp:eventTime
		      windowNumber:[window windowNumber]
			   context:nil
		       eventNumber:0
			clickCount:1
			  pressure:1.0];

	// TODO(cef): Pass a non-nil value to dragImage (see issue #1715). For now
	// work around the "callee requires a non-null argument" error that occurs
	// when building with the 10.11 SDK.
	id nilArg = nil;
	[window dragImage:nilArg
			at:position
		    offset:NSZeroSize
		     event:dragEvent
		pasteboard:pasteboard_
		    source:self
		 slideBack:YES];
	return YES;
}

- (void)setCurrentDragOp:(NSDragOperation)op
{
	current_drag_op_ = op;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info
{
	return NO;
}

// NSPasteboardOwner Protocol

- (void)pasteboard:(NSPasteboard *)pboard provideDataForType:(NSString *)type
{
	if (!current_drag_data_) {
		return;
	}

	// URL.
	if ([type isEqualToString:NSURLPboardType]) {
		DCHECK(current_drag_data_->IsLink());
		NSString *strUrl = [NSString
			stringWithUTF8String:current_drag_data_->GetLinkURL()
						     .ToString()
						     .c_str()];
		NSURL *url = [NSURL URLWithString:strUrl];
		[url writeToPasteboard:pboard];
		// URL title.
	} else if ([type isEqualToString:kNSURLTitlePboardType]) {
		NSString *strTitle = [NSString
			stringWithUTF8String:current_drag_data_->GetLinkTitle()
						     .ToString()
						     .c_str()];
		[pboard setString:strTitle forType:kNSURLTitlePboardType];

		// File contents.
	} else if ([type isEqualToString:fileUTI_]) {

	} else if ([type isEqualToString:NSStringPboardType]) {
		NSString *strTitle = [NSString
			stringWithUTF8String:current_drag_data_
						     ->GetFragmentText()
						     .ToString()
						     .c_str()];
		[pboard setString:strTitle forType:NSStringPboardType];

	} else if ([type isEqualToString:kCEFDragDummyPboardType]) {
		// The dummy type _was_ promised and someone decided to call the bluff.
		[pboard setData:[NSData data] forType:kCEFDragDummyPboardType];
	}
}

// Utility methods.
- (void)resetDragDrop
{
	current_drag_op_ = NSDragOperationNone;
	current_allowed_ops_ = NSDragOperationNone;
	current_drag_data_ = nullptr;
	if (fileUTI_) {
#if !__has_feature(objc_arc)
		[fileUTI_ release];
#endif // !__has_feature(objc_arc)
		fileUTI_ = nil;
	}
	if (pasteboard_) {
#if !__has_feature(objc_arc)
		[pasteboard_ release];
#endif // !__has_feature(objc_arc)
		pasteboard_ = nil;
	}
}

- (void)fillPasteboard
{
	DCHECK(!pasteboard_);
	pasteboard_ = [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
#if !__has_feature(objc_arc)
	[pasteboard_ retain];
#endif // !__has_feature(objc_arc)

	[pasteboard_ declareTypes:@[kCEFDragDummyPboardType] owner:self];

	// URL (and title).
	if (current_drag_data_->IsLink()) {
		[pasteboard_
			addTypes:@[NSPasteboardTypeURL, kNSURLTitlePboardType]
			   owner:self];
	}

	// MIME type.
	CefString mimeType;
	size_t contents_size = current_drag_data_->GetFileContents(nullptr);
	CefString download_metadata = current_drag_data_->GetLinkMetadata();

	// File.
	if (contents_size > 0) {
		std::string file_name =
			current_drag_data_->GetFileName().ToString();
		size_t sep = file_name.find_last_of(".");
		CefString extension = file_name.substr(sep + 1);

		mimeType = CefGetMimeType(extension);

		if (!mimeType.empty()) {
			CFStringRef mimeTypeCF;
			mimeTypeCF = CFStringCreateWithCString(
				kCFAllocatorDefault,
				mimeType.ToString().c_str(),
				kCFStringEncodingUTF8);
			fileUTI_ = (__bridge NSString *)
				UTTypeCreatePreferredIdentifierForTag(
					kUTTagClassMIMEType, mimeTypeCF,
					nullptr);
			CFRelease(mimeTypeCF);
			// File (HFS) promise.
			NSArray *fileUTIList = @[fileUTI_];
			[pasteboard_ addTypes:@[NSFilesPromisePboardType]
					owner:self];
			[pasteboard_ setPropertyList:fileUTIList
					     forType:NSFilesPromisePboardType];

			[pasteboard_ addTypes:fileUTIList owner:self];
		}
	}

	// Plain text.
	if (!current_drag_data_->GetFragmentText().empty()) {
		[pasteboard_ addTypes:@[NSStringPboardType] owner:self];
	}
}

- (void)populateDropData:(CefRefPtr<CefDragData>)data
	  fromPasteboard:(NSPasteboard *)pboard
{
	DCHECK(data);
	DCHECK(pboard);
	DCHECK(data && !data->IsReadOnly());
	NSArray *types = [pboard types];

	// Get plain text.
	if ([types containsObject:NSStringPboardType]) {
		data->SetFragmentText(
			[[pboard stringForType:NSStringPboardType] UTF8String]);
	}

	// Get files.
	if ([types containsObject:NSFilenamesPboardType]) {
		NSArray *files =
			[pboard propertyListForType:NSFilenamesPboardType];
		if ([files isKindOfClass:[NSArray class]] && [files count]) {
			for (NSUInteger i = 0; i < [files count]; i++) {
				NSString *filename = [files objectAtIndex:i];
				BOOL exists = [[NSFileManager defaultManager]
					fileExistsAtPath:filename];
				if (exists) {
					data->AddFile([filename UTF8String],
						      CefString());
				}
			}
		}
	}
}

- (NSPoint)flipWindowPointToView:(const NSPoint &)windowPoint
{
	NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
	NSRect viewFrame = [self frame];
	viewPoint.y = viewFrame.size.height - viewPoint.y;
	return viewPoint;
}

- (void)resetDeviceScaleFactor
{
	float device_scale_factor = 1.0f;
	NSWindow *window = [self window];
	if (window)
		device_scale_factor = [window backingScaleFactor];
	[self setDeviceScaleFactor:device_scale_factor];
}

- (void)setDeviceScaleFactor:(float)device_scale_factor
{
	if (device_scale_factor == device_scale_factor_)
		return;

	// Apply some sanity checks.
	if (device_scale_factor < 1.0f || device_scale_factor > 4.0f)
		return;

	device_scale_factor_ = device_scale_factor;

	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser) {
		browser->GetHost()->NotifyScreenInfoChanged();
		browser->GetHost()->WasResized();
	}
}

- (float)getDeviceScaleFactor
{
	return device_scale_factor_;
}

- (void)viewDidChangeBackingProperties
{
	const CGFloat device_scale_factor = [self getDeviceScaleFactor];

	if (device_scale_factor == device_scale_factor_)
		return;

	CefRefPtr<CefBrowser> browser = [self getBrowser];
	if (browser) {
		browser->GetHost()->NotifyScreenInfoChanged();
		browser->GetHost()->WasResized();
	}
}

// Convert from scaled coordinates to view coordinates.
- (NSPoint)convertPointFromBackingInternal:(NSPoint)aPoint
{
	return [self convertPointFromBacking:aPoint];
}

// Convert from view coordinates to scaled coordinates.
- (NSPoint)convertPointToBackingInternal:(NSPoint)aPoint
{
	return [self convertPointToBacking:aPoint];
}

// Convert from scaled coordinates to view coordinates.
- (NSRect)convertRectFromBackingInternal:(NSRect)aRect
{
	return [self convertRectFromBacking:aRect];
}

// Convert from view coordinates to scaled coordinates.
- (NSRect)convertRectToBackingInternal:(NSRect)aRect
{
	return [self convertRectToBacking:aRect];
}

- (void)ChangeCompositionRange:(CefRange)range
	      character_bounds:(const CefRenderHandler::RectList &)bounds
{
	if (text_input_client_)
		[text_input_client_ ChangeCompositionRange:range
					  character_bounds:bounds];
}

@end
#pragma GCC diagnostic pop

//PRISM/Renjinbo/20231027/#2894/void * view is not nsview , is _NSViewLayoutAux, which can't constain removeFromSuperview method
void cefViewRemoveFromSuperView(void *view)
{
	if (!*((bool *)view)) {
		blog(LOG_ERROR,
		     "cef view Failed to remove from superview. instance is not valid");

		return;
	}

	NSView *appleView = (__bridge NSView *)view;
	if (![appleView isKindOfClass:[NSView class]]) {
		blog(LOG_ERROR,
		     "cef view Failed to remove from superview. instanceClass:%s",
		     [appleView className].UTF8String);
		return;
	}
	[appleView removeFromSuperview];
}
