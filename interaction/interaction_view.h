#pragma once
#include "cef-headers.hpp"
#if defined(_WIN32)
#include "osr_ime_handler_win.h"
#endif
#include "interaction_main.h"

#define VIEW_BK_RENDER_COLOR 0X151515
#define VIEW_BK_RESIZE_COLOR 0X000000

/*
In class InteractionView, many code is integrated from CEF's sample and we never modify its code style.
You can get the sample from https://github.com/chromiumembedded/cef/tree/master/tests/cefclient/browser
*/

class BrowserInteractionMain;
class InteractionView {
public:

	InteractionView(BrowserInteractionMain &interaction_main)
		: main_ui_(interaction_main)
	{
	}
	virtual ~InteractionView(){};

	// should be invoked in CEF's main thread
	virtual void Create(WindowHandle hp) = 0;
	virtual void ResetInteraction() = 0;

	virtual void OnImeCompositionRangeChanged(
		CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
		const CefRenderHandler::RectList &character_bounds) = 0;

public:
	BrowserInteractionMain &main_ui_;

	int source_cx_ = 0;
	int source_cy_ = 0;

	WindowHandle hwnd_ = nullptr;
	CefRefPtr<CefBrowser> browser_;
};
