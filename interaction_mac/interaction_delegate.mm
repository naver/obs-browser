#include <util/threading.h>
#include "interaction_delegate.h"
#include "../obs-browser-source.hpp"
#include "pls/pls-source.h"

#ifdef ENABLE_BROWSER_QT_LOOP
#include <qthread.h>
#include <qapplication.h>
#endif

#include "PLSInteractionMacWindow.h"
#include "geometry_util.h"
#include "include/wrapper/cef_helpers.h"

#define VIEW_BK_RENDER_COLOR 0X151515
#define VIEW_BK_RESIZE_COLOR 0X000000

static PLSInteractionMacWindow *gInteractionWindow;

InteractionDelegate::InteractionDelegate(BrowserSource *bs) : browser_source(bs)
{
	if (bs) {

		source = bs->source;
		signal_handler_connect_ref(
			obs_source_get_signal_handler(source), "rename",
			InteractionDelegate::SourceRenamed, this);
	}
}

void InteractionDelegate::showInteractionInMainThread(bool show)
{
	CEF_REQUIRE_UI_THREAD();
	is_interaction_showing = show;

	if (gInteractionWindow) {
		[gInteractionWindow close];
		gInteractionWindow = nil;
	}

	if (show) {
		gInteractionWindow = [[PLSInteractionMacWindow alloc] init];
		[gInteractionWindow setupData:browser_source srcDelegate:this];
		PostInteractionTitle();
		[gInteractionWindow.window center];
		[gInteractionWindow.window orderFront:nil];
		OnInteractionShow(nullptr);
	}
}

//PRISM/jimboRen/20250317/#None/This object may be accessed asynchronously after being released
void InteractionDelegate::ShowInteraction(bool show)
{
	if ([NSThread isMainThread]) {
		showInteractionInMainThread(show);
		return;
	}
	dispatch_sync(dispatch_get_main_queue(), ^{
		showInteractionInMainThread(show);
	});
}

void InteractionDelegate::DestroyInteraction()
{
	signal_handler_disconnect(obs_source_get_signal_handler(source),
				  "rename", InteractionDelegate::SourceRenamed,
				  this);

	blog(LOG_INFO, "To run DestroyInteraction for '%s'",
	     obs_source_get_name(source));
	closeWindowWithSource(browser_source);
}
void InteractionDelegate::closeWindowWithSource(BrowserSource *browser_source_)
{
	if (!gInteractionWindow ||
	    ![gInteractionWindow isSameSource:browser_source_]) {
		return;
	}
	if ([NSThread isMainThread]) {
		[gInteractionWindow close];
		gInteractionWindow = nil;
	} else {
		dispatch_sync(dispatch_get_main_queue(), ^{
			[gInteractionWindow close];
			gInteractionWindow = nil;
		});
	}
}

bool InteractionDelegate::CreateDisplay(WindowHandle hWnd, int cx, int cy)
{
	assert(NULL == interaction_display);
	assert(NULL == reference_source);

	reference_source = obs_get_source_by_name(obs_source_get_name(source));
	if (!reference_source) {
		assert(false);
		return false;
	}

	gs_init_data info = {};
	info.cx = cx;
	info.cy = cy;
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
#if defined(_WIN32)
	info.window.hwnd = hWnd;
#elif defined(__APPLE__)
	info.window.view = (__bridge __unsafe_unretained id)hWnd;
#endif

	interaction_display = obs_display_create(&info, VIEW_BK_RENDER_COLOR);
	assert(interaction_display);
	if (!interaction_display) {
		obs_source_release(reference_source);
		reference_source = NULL;
		return false;
	}

	obs_display_add_draw_callback(interaction_display,
				      InteractionDelegate::DrawPreview, this);

	blog(LOG_INFO,
	     "Added display for interaction for '%s', BrowserSource:%p",
	     obs_source_get_name(source), this);

	return true;
}

void InteractionDelegate::ClearDisplay()
{
	if (interaction_display) {
		obs_display_remove_draw_callback(
			interaction_display, InteractionDelegate::DrawPreview,
			this);

		obs_display_destroy(interaction_display);

		interaction_display = NULL;

		blog(LOG_INFO,
		     "Removed display for interaction for '%s', BrowserSource:%p",
		     obs_source_get_name(source), this);
	}

	if (reference_source) {
		obs_source_release(reference_source);
		reference_source = NULL;
	}
}

void InteractionDelegate::OnInteractionShow(WindowHandle)
{

	NSView *view_hwnd = [gInteractionWindow getOpenGLView];

	if (!view_hwnd) {
		return;
	}
	NSSize openGlSize = [gInteractionWindow getPixelSize];
	int cx = openGlSize.width;
	int cy = openGlSize.height;

	if (!interaction_display) {
		auto tView = (__bridge void *)view_hwnd;
		CreateDisplay(tView, cx, cy);
	} else {
		obs_display_resize(interaction_display, cx, cy);
	}
}

void InteractionDelegate::PostInteractionTitle()
{
	dispatch_async(dispatch_get_main_queue(), ^{
		if (!gInteractionWindow) {
			return;
		}
		const char *name = obs_source_get_name(source);
		if (name && *name) {
			return;
		}
		const char *fmt = obs_module_text("Basic.InteractionWindow");
		if (fmt && *fmt) {
			return;
		}
		QString title = QString(fmt).arg(name);
		[gInteractionWindow.window
			setTitle:[NSString stringWithUTF8String:
						   title.toUtf8().constData()]];
	});
}

//移到其他地方进行展示
void InteractionDelegate::CheckInteractionShowing() {}

void InteractionDelegate::CreateInteractionUI() {}

void InteractionDelegate::SetInteractionInfo(int, int, CefRefPtr<CefBrowser>) {}
void InteractionDelegate::ExecuteOnInteraction(InteractionFunc, bool) {}

void InteractionDelegate::SourceRenamed(void *data, calldata_t *)
{
	InteractionDelegate *self = (InteractionDelegate *)data;
	self->PostInteractionTitle();
}

void InteractionDelegate::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	InteractionDelegate *interaction_delegate =
		static_cast<InteractionDelegate *>(data);

	if (!interaction_delegate->source)
		return;

	uint32_t sourceCX = std::max(
		obs_source_get_width(interaction_delegate->source), 1u);
	uint32_t sourceCY = std::max(
		obs_source_get_height(interaction_delegate->source), 1u);

	int x, y;
	float scale;
	client::GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	int newCX = int(scale * float(sourceCX));
	int newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(interaction_delegate->source);

	gs_projection_pop();
	gs_viewport_pop();
}

void InteractionDelegate::SetBrowserData(void *src, obs_data_t *data)
{
	if (!data || !src) {
		return;
	}
	const char *method = obs_data_get_string(data, "method");
	if (!method) {
		assert(false);
		return;
	}

	BrowserSource *bs = reinterpret_cast<BrowserSource *>(src);
	if (0 == strcmp(method, METHOD_SHOW_INTERACTION)) {
		bs->interaction_delegate->ShowInteraction(true);

	} else if (0 == strcmp(method, METHOD_HIDE_INTERACTION)) {
		bs->interaction_delegate->ShowInteraction(false);

	} else if (0 == strcmp(method, METHOD_REFRESH_BROWSER)) {
		bs->Refresh();
	}
}

void InteractionDelegate::GetBrowserData(void *, obs_data_t *) {}

void InteractionDelegate::interactionWindowClosed()
{
	blog(LOG_INFO, "interaction window closed");
	ClearDisplay();
	is_interaction_showing = false;
	gInteractionWindow = nullptr;
}
void InteractionDelegate::interactionWindowResized()
{
	OnInteractionShow(nullptr);
}
bool InteractionDelegate::OnCursorChange(CefRefPtr<CefBrowser>,
					 CefCursorHandle cursor,
					 cef_cursor_type_t,
					 const CefCursorInfo &)
{
	if (gInteractionWindow) {
		[CAST_CEF_CURSOR_HANDLE_TO_NSCURSOR(cursor) set];
		return true;
	}
	return false;
}

void InteractionDelegate::resizeCefViewSize(void *_view, int w, int h)
{
	NSView *cefView = (__bridge NSView *)_view;
	if (cefView) {
		[cefView setFrameSize:NSMakeSize(w, h)];
	}
}
