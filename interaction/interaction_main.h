#pragma once
#include "interaction_def.h"
#include <obs-data.h>
#include <include/internal/cef_ptr.h>
#include <include/internal/cef_types_wrappers.h>
#include <include/cef_render_handler.h>
#include <include/cef_browser.h>
#include <memory>

class BrowserClient;
class BrowserInteractionMain {
public:
	BrowserInteractionMain(SOURCE_HANDLE src) : m_source(src) {}
	virtual ~BrowserInteractionMain() {}

	// Warning : Should be invoked in CEF's main thread
	virtual void CreateInteractionUI() = 0;
	virtual void WaitDestroyInteractionUI() = 0;
	virtual void ShowInteractionUI(bool show) = 0;
	virtual void BringUIToTop() = 0;
	virtual void SetInteractionInfo(int source_width, int source_height,
					CefRefPtr<CefBrowser> cefBrowser) = 0;
	virtual void GetInteractionInfo(int &source_width, int &source_height,
					CefRefPtr<CefBrowser> &cefBrowser) = 0;
	virtual void OnImeCompositionRangeChanged(
		CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
		const CefRenderHandler::RectList &character_bounds) = 0;
	virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX,
				    int viewY, int &screenX, int &screenY) = 0;

	virtual bool
	OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor,
		       cef_cursor_type_t type,
		       const CefCursorInfo &custom_cursor_info) = 0;

	// Can be invoked out of CEF's main thread
	virtual void PostWindowTitle(const char *str) = 0;
	virtual void PostDestroyInteractionUI() = 0;
	virtual WindowHandle GetInteractionView() = 0;
	virtual WindowHandle GetInteractionMain() = 0;
	virtual bool IsResizing() = 0;

protected:
	std::string m_utf8_title = "";

	SOURCE_HANDLE m_source = nullptr;
};

using INTERACTION_PTR = std::shared_ptr<BrowserInteractionMain>;
using INTERACTION_WEAK_PTR = std::weak_ptr<BrowserInteractionMain>;
