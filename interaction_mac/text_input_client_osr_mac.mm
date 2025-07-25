// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

// Implementation based on
// content/browser/renderer_host/render_widget_host_view_mac.mm from Chromium.

#include "text_input_client_osr_mac.h"
#include "include/cef_client.h"

#define ColorBLACK 0xFF000000 // Same as Blink SKColor.

namespace {

// TODO(suzhe): Upstream this function.
cef_color_t CefColorFromNSColor(NSColor *color)
{
	CGFloat r, g, b, a;
	[color getRed:&r green:&g blue:&b alpha:&a];

	return std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255))
		       << 24 |
	       std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255))
		       << 16 |
	       std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255))
		       << 8 |
	       std::max(0,
			std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(NSAttributedString *string,
		       std::vector<CefCompositionUnderline> *underlines)
{
	int length = (int)[[string string] length];
	int i = 0;
	while (i < length) {
		NSRange range;
		NSDictionary *attrs = [string
			    attributesAtIndex:i
			longestEffectiveRange:&range
				      inRange:NSMakeRange(i, length - i)];
		NSNumber *style =
			[attrs objectForKey:NSUnderlineStyleAttributeName];
		if (style) {
			cef_color_t color = ColorBLACK;
			if (NSColor *colorAttr = [attrs
				    objectForKey:NSUnderlineColorAttributeName]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
				color = CefColorFromNSColor([colorAttr
					colorUsingColorSpaceName:
						NSDeviceRGBColorSpace]);
#pragma clang diagnostic pop
			}
			cef_composition_underline_t line = {
				{static_cast<uint32_t>(range.location),
				 static_cast<uint32_t>(NSMaxRange(range))},
				color,
				0,
				[style intValue] > 1};
			underlines->push_back(line);
		}
		i = (int)range.location + (int)range.length;
	}
}

} // namespace

extern "C" {
extern NSString *NSTextInputReplacementRangeAttributeName;
}

@implementation CefTextInputClientOSRMac

@synthesize selectedRange = selectedRange_;
@synthesize handlingKeyDown = handlingKeyDown_;

- (id)initWithBrowser:(CefRefPtr<CefBrowser>)browser superView:(NSView *)view_
{
	self = [super init];
	browser_ = browser;
	self.superView = view_;
	return self;
}

- (void)detach
{
	browser_ = nullptr;
}

- (NSArray *)validAttributesForMarkedText
{
	if (!validAttributesForMarkedText_) {
		validAttributesForMarkedText_ = [[NSArray alloc]
			initWithObjects:NSUnderlineStyleAttributeName,
					NSUnderlineColorAttributeName,
					NSMarkedClauseSegmentAttributeName,
					NSTextInputReplacementRangeAttributeName,
					nil];
	}
	return validAttributesForMarkedText_;
}

- (NSRange)selectedRange
{
	if (selectedRange_.location == NSNotFound || selectedRange_.length == 0)
		return NSMakeRange(NSNotFound, 0);
	return selectedRange_;
}

- (NSRange)markedRange
{
	return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText
{
	return hasMarkedText_;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
	BOOL isAttributedString =
		[aString isKindOfClass:[NSAttributedString class]];
	NSString *im_text = isAttributedString ? [aString string] : aString;
	if (handlingKeyDown_) {
		textToBeInserted_.append([im_text UTF8String]);
	} else {
		cef_range_t range = {
			static_cast<uint32_t>(replacementRange.location),
			static_cast<uint32_t>(NSMaxRange(replacementRange))};
		browser_->GetHost()->ImeCommitText([im_text UTF8String], range,
						   0);
	}

	// Inserting text will delete all marked text automatically.
	hasMarkedText_ = NO;
}

- (void)doCommandBySelector:(SEL)aSelector
{
	// An input method calls this function to dispatch an editing command to be
	// handled by this view.
}

- (void)setMarkedText:(id)aString
	   selectedRange:(NSRange)newSelRange
	replacementRange:(NSRange)replacementRange
{
	// An input method has updated the composition string. We send the given text
	// and range to the browser so it can update the composition node of Blink.

	BOOL isAttributedString =
		[aString isKindOfClass:[NSAttributedString class]];
	NSString *im_text = isAttributedString ? [aString string] : aString;
	int length = (int)[im_text length];

	// |markedRange_| will get set in a callback from ImeSetComposition().
	selectedRange_ = newSelRange;
	markedText_ = [im_text UTF8String];
	hasMarkedText_ = (length > 0);
	underlines_.clear();

	if (isAttributedString) {
		ExtractUnderlines(aString, &underlines_);
	} else {
		// Use a thin black underline by default.
		cef_composition_underline_t line = {
			{0, static_cast<uint32_t>(length)}, ColorBLACK, 0, false};
		underlines_.push_back(line);
	}

	// If we are handling a key down event then ImeSetComposition() will be
	// called from the keyEvent: method.
	// Input methods of Mac use setMarkedText calls with empty text to cancel an
	// ongoing composition. Our input method backend will automatically cancel an
	// ongoing composition when we send empty text.
	if (handlingKeyDown_) {
		setMarkedTextReplacementRange_ = {
			static_cast<uint32_t>(replacementRange.location),
			static_cast<uint32_t>(NSMaxRange(replacementRange))};
	} else if (!handlingKeyDown_) {
		CefRange replacement_range((int)replacementRange.location,
					   (int)NSMaxRange(replacementRange));
		CefRange selection_range((int)newSelRange.location,
					 (int)NSMaxRange(newSelRange));

		browser_->GetHost()->ImeSetComposition(markedText_, underlines_,
						       replacement_range,
						       selection_range);
	}
}

- (void)unmarkText
{
	// Delete the composition node of the browser and finish an ongoing
	// composition.
	// It seems that, instead of calling this method, an input method will call
	// the setMarkedText method with empty text to cancel ongoing composition.
	// Implement this method even though we don't expect it to be called.
	hasMarkedText_ = NO;
	markedText_.clear();
	underlines_.clear();

	// If we are handling a key down event then ImeFinishComposingText() will be
	// called from the keyEvent: method.
	if (!handlingKeyDown_)
		browser_->GetHost()->ImeFinishComposingText(false);
	else
		unmarkTextCalled_ = YES;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
						actualRange:(NSRangePointer)
								    actualRange
{
	// Modify the attributed string if required.
	// Not implemented here as we do not want to control the IME window view.
	return nil;
}

- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
			     actualRange:(NSRangePointer)actualRange
{
	NSRect rect;

	NSUInteger location = theRange.location;

	// If location is not specified fall back to the composition range start.
	if (location == NSNotFound)
		location = markedRange_.location;

	// Offset location by the composition range start if required.
	if (location >= markedRange_.location)
		location -= markedRange_.location;

	if (location < composition_bounds_.size()) {
		const CefRect &rc = composition_bounds_[location];
		rect = NSMakeRect(rc.x, rc.y, rc.width, rc.height);
	}

	if (actualRange)
		*actualRange = NSMakeRange(location, theRange.length);

	return rect;
}

- (NSRect)screenRectFromViewRect:(NSRect)rect
{
	NSRect screenRect = rect;
	if (_superView) {
		NSView *_sView = self.superView;
		NSWindow *win = [_sView window];
		NSRect locationInWindow = [_sView convertRect:_sView.bounds
						       toView:nil];
		NSPoint po = [win convertPointToScreen:locationInWindow.origin];
		po.y += win.frame.size.height - 40;

		screenRect.origin = po;
	}
	return screenRect;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
			 actualRange:(NSRangePointer)actualRange
{
	NSRect rect = [self firstViewRectForCharacterRange:theRange
					       actualRange:actualRange];

	rect = [self screenRectFromViewRect:rect];

	if (rect.origin.y >= rect.size.height)
		rect.origin.y -= rect.size.height;
	else
		rect.origin.y = 0;
	return rect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
	return NSNotFound;
}

- (void)HandleKeyEventBeforeTextInputClient:(NSEvent *)keyEvent
{
	DCHECK([keyEvent type] == NSEventTypeKeyDown);
	// Don't call this method recursively.
	DCHECK(!handlingKeyDown_);

	oldHasMarkedText_ = hasMarkedText_;
	handlingKeyDown_ = YES;

	// These variables might be set when handling the keyboard event.
	// Clear them here so that we can know whether they have changed afterwards.
	textToBeInserted_.clear();
	markedText_.clear();
	underlines_.clear();
	setMarkedTextReplacementRange_ = CefRange(UINT32_MAX, UINT32_MAX);
	unmarkTextCalled_ = NO;
}

- (void)HandleKeyEventAfterTextInputClient:(CefKeyEvent)keyEvent
{
	handlingKeyDown_ = NO;

	// Send keypress and/or composition related events.
	// Note that |textToBeInserted_| is a UTF-16 string but it's fine to only
	// handle BMP characters here as we can always insert non-BMP characters as
	// text.

	// If the text to be inserted only contains 1 character then we can just send
	// a keypress event.
	if (!hasMarkedText_ && !oldHasMarkedText_ &&
	    textToBeInserted_.length() <= 1) {
		keyEvent.type = KEYEVENT_KEYDOWN;

		browser_->GetHost()->SendKeyEvent(keyEvent);

		// Don't send a CHAR event for non-char keys like arrows, function keys and
		// clear.
		if (keyEvent.modifiers & (EVENTFLAG_IS_KEY_PAD)) {
			if (keyEvent.native_key_code == 71)
				return;
		}

		keyEvent.type = KEYEVENT_CHAR;
		browser_->GetHost()->SendKeyEvent(keyEvent);
	}

	// If the text to be inserted contains multiple characters then send the text
	// to the browser using ImeCommitText().
	BOOL textInserted = NO;
	if (textToBeInserted_.length() >
	    ((hasMarkedText_ || oldHasMarkedText_) ? 0u : 1u)) {
		browser_->GetHost()->ImeCommitText(
			textToBeInserted_, CefRange(UINT32_MAX, UINT32_MAX), 0);
		textToBeInserted_.clear();
	}

	// Update or cancel the composition. If some text has been inserted then we
	// don't need to explicitly cancel the composition.
	if (hasMarkedText_ && markedText_.length()) {
		// Update the composition by sending marked text to the browser.
		// |selectedRange_| is the range being selected inside the marked text.
		browser_->GetHost()->ImeSetComposition(
			markedText_, underlines_,
			setMarkedTextReplacementRange_,
			CefRange((int)selectedRange_.location,
				 (int)NSMaxRange(selectedRange_)));
	} else if (oldHasMarkedText_ && !hasMarkedText_ && !textInserted) {
		// There was no marked text or inserted text. Complete or cancel the
		// composition.
		if (unmarkTextCalled_)
			browser_->GetHost()->ImeFinishComposingText(false);
		else
			browser_->GetHost()->ImeCancelComposition();
	}

	setMarkedTextReplacementRange_ = CefRange(UINT32_MAX, UINT32_MAX);
}

- (void)ChangeCompositionRange:(CefRange)range
	      character_bounds:(const CefRenderHandler::RectList &)bounds
{
	composition_range_ = range;
	markedRange_ = NSMakeRange(range.from, range.to - range.from);
	composition_bounds_ = bounds;
}

- (void)cancelComposition
{
	if (!hasMarkedText_)
		return;

// Cancel the ongoing composition. [NSInputManager markedTextAbandoned:]
// doesn't call any NSTextInput functions, such as setMarkedText or
// insertText.
// TODO(erikchen): NSInputManager is deprecated since OSX 10.6. Switch to
// NSTextInputContext. http://www.crbug.com/479010.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	NSInputManager *currentInputManager =
		[NSInputManager currentInputManager];
	[currentInputManager markedTextAbandoned:self];
#pragma clang diagnostic pop

	hasMarkedText_ = NO;
	// Should not call [self unmarkText] here because it'll send unnecessary
	// cancel composition messages to the browser.
}

@end
