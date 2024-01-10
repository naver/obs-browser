#pragma once
#include "cef-headers.hpp"
#include "interaction_main.h"
#include "interaction_view.h"

#define VIEW_BK_RENDER_COLOR 0X151515
#define VIEW_BK_RESIZE_COLOR 0X000000

/*
In class InteractionView, many code is integrated from CEF's sample and we never modify its code style.
You can get the sample from https://github.com/chromiumembedded/cef/tree/master/tests/cefclient/browser
*/

class BrowserInteractionMain;
class InteractionViewMac : public InteractionView {
public:
    InteractionViewMac(BrowserInteractionMain &interaction_main);
	virtual ~InteractionViewMac();

	// should be invoked in CEF's main thread
	virtual void Create(WindowHandle hp) override;
	virtual void ResetInteraction() override;

	virtual void OnImeCompositionRangeChanged(
		CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
		const CefRenderHandler::RectList &character_bounds);
};
