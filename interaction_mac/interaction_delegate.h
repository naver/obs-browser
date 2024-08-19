#pragma once
#include <obs-module.h>
#include <functional>
#include "include/cef_browser.h"

#if defined(_WIN32)
#include <windows.h>
using WindowHandle = HWND;
#elif defined(__APPLE__)
using WindowHandle = void *;
#endif

using SOURCE_HANDLE = void *;

class BrowserInteractionMain {
public:
	void SetInteractionInfo(int, int, CefRefPtr<CefBrowser>){};

	bool GetScreenPoint(CefRefPtr<CefBrowser>, int, int, int &, int &)
	{
		return false;
	}
};

using INTERACTION_PTR = std::shared_ptr<BrowserInteractionMain>;
using INTERACTION_WEAK_PTR = std::weak_ptr<BrowserInteractionMain>;
using InteractionFunc = std::function<void(INTERACTION_PTR)>;

struct BrowserSource;
class InteractionDelegate {
public:
	InteractionDelegate(BrowserSource *bs);
	virtual ~InteractionDelegate(){};

	obs_source_t *source = nullptr;
	BrowserSource *browser_source = nullptr;

	INTERACTION_PTR interaction_ui;
	volatile bool is_interaction_showing = false;
	volatile bool is_interaction_reshow = false;

	// reference_source is for display, to make sure source won't be released while display is added
	// reference_source and interaction_display are created/deleted in video render thread
	obs_source_t *reference_source = nullptr;
	obs_display_t *interaction_display = nullptr;

	void ShowInteraction(bool show);
	void DestroyInteraction();
	void PostInteractionTitle();
	void CheckInteractionShowing();
	void CreateInteractionUI();
	void SetInteractionInfo(int source_width, int source_height,
				CefRefPtr<CefBrowser> cefBrowser);
	void ExecuteOnInteraction(InteractionFunc func, bool async = false);
	static void SourceRenamed(void *data, calldata_t *pr);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void SetBrowserData(void *data, obs_data_t *private_data);
	static void GetBrowserData(void *data, obs_data_t *data_output);

	void interactionWindowClosed();
	void interactionWindowResized();

	static bool OnCursorChange(CefRefPtr<CefBrowser> browser,
				   CefCursorHandle cursor,
				   cef_cursor_type_t type,
				   const CefCursorInfo &custom_cursor_info);

	static void resizeCefViewSize(void *_view, int w, int h);

private:
	// Invoked from video render thread
	void OnInteractionShow(WindowHandle hWnd);
	bool CreateDisplay(WindowHandle hWnd, int cx, int cy);
	void ClearDisplay();
	void closeWindowWithSource(BrowserSource *browser_source);
};
