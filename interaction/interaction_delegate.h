#pragma once
#include "interaction_def.h"
#include "interaction_main.h"
#if defined(_WIN32)
#include "interaction_main_win.h"
#elif defined(__APPLE__)
#include "interaction_main_mac.h"
#endif
#include <obs-module.h>
#include <functional>

class BrowserInteractionMain;

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
	int display_cx = 0;
	int display_cy = 0;

	void ShowInteraction(bool show);
	void DestroyInteraction();
	void ExecuteOnInteraction(InteractionFunc func, bool async = false);
	void PostInteractionTitle();
	void CheckInteractionShowing();
	void CreateInteractionUI();
	void SetInteractionInfo(int source_width, int source_height,
				CefRefPtr<CefBrowser> cefBrowser);

	// Invoked from video render thread
	void OnInteractionShow(WindowHandle hWnd);
	void OnInteractionHide(WindowHandle hWnd);
	bool CreateDisplay(WindowHandle hWnd, int cx, int cy);
	void ClearDisplay();

	static void SourceRenamed(void *data, calldata_t *pr);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void SetBrowserData(void *data, obs_data_t *private_data);
	static void GetBrowserData(void *data, obs_data_t *data_output);
};
