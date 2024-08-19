#include <util/threading.h>
#include "interaction_delegate.h"
#include "../obs-browser-source.hpp"
#if defined(OS_WIN)
#include "interaction_main_win.h"
#elif defined(OS_MAC)
#include "interaction_main_mac.h"
#endif
#include "interaction_window_helper.h"
#include "pls/pls-source.h"
#ifdef ENABLE_BROWSER_QT_LOOP
#include <qthread.h>
#include <qapplication.h>
#endif

InteractionDelegate::InteractionDelegate(BrowserSource *bs) : browser_source(bs)
{
	if (bs) {

		source = bs->source;
		signal_handler_connect_ref(
			obs_source_get_signal_handler(source), "rename",
			InteractionDelegate::SourceRenamed, this);

		interaction_ui =
#if defined(_WIN32)
			std::make_shared<BrowserInteractionMainWin>(
				browser_source);
#elif defined(__APPLE__)
			std::make_shared<BrowserInteractionMainMac>(
				browser_source);
#endif

		InteractionManager::Instance()->OnSourceCreated(this);
	}
}

void InteractionDelegate::ShowInteraction(bool show)
{
	// WARNING:
	// In this function, we'd better not add any lock !

	if (is_interaction_showing != show) {
		if (show) {
			blog(LOG_INFO,
			     "Request show interaction for '%s', BrowserSource:%p",
			     obs_source_get_name(source), this);
		} else {
			blog(LOG_INFO,
			     "Request hide interaction for '%s', BrowserSource:%p",
			     obs_source_get_name(source), this);
		}
	}

	if (is_interaction_showing && show) {
		is_interaction_reshow = true;
	} else {
		is_interaction_reshow = false;
	}

	is_interaction_showing = show;
}

void InteractionDelegate::DestroyInteraction()
{
	signal_handler_disconnect(obs_source_get_signal_handler(source),
				  "rename", InteractionDelegate::SourceRenamed,
				  this);
	InteractionManager::Instance()->OnSourceDeleted(this);

	if (interaction_display) {
		blog(LOG_ERROR, "Display is not cleared. BrowserSource:%p",
		     this);
		assert(NULL == interaction_display && "display exist !");
	}

	// Here we firstly post a message to make sure the window can be destroied soon.
	interaction_ui->PostDestroyInteractionUI();

	blog(LOG_INFO, "To run DestroyInteraction for '%s'",
	     obs_source_get_name(source));

	ExecuteOnInteraction(
		[](INTERACTION_PTR interaction) {
			blog(LOG_INFO,
			     "DestroyInteraction is invoked, address:%p",
			     interaction.get());

			interaction->SetInteractionInfo(0, 0, nullptr);
			interaction->WaitDestroyInteractionUI();
		},
		false); // Here we must wait interaction window to be destroied, because its instance will be freed soon.
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
	info.window.view = (id)hWnd;
#endif

	interaction_display = obs_display_create(&info, VIEW_BK_RENDER_COLOR);
	assert(interaction_display);
	if (!interaction_display) {
		obs_source_release(reference_source);
		reference_source = NULL;
		return false;
	}

	display_cx = cx;
	display_cy = cy;

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
		display_cx = 0;
		display_cy = 0;

		blog(LOG_INFO,
		     "Removed display for interaction for '%s', BrowserSource:%p",
		     obs_source_get_name(source), this);
	}

	if (reference_source) {
		obs_source_release(reference_source);
		reference_source = NULL;
	}
}

void InteractionDelegate::OnInteractionShow(WindowHandle top_hwnd)
{
#if defined(_WIN32)
	if (!::IsWindowVisible(top_hwnd)) {

		HWND view_hwnd = interaction_ui->GetInteractionView();
		if (view_hwnd) {
			RECT rc;
			GetClientRect(view_hwnd, &rc);

			int cx = RectWidth(rc);
			int cy = RectHeight(rc);

			if (!interaction_display) {
				CreateDisplay(view_hwnd, cx, cy);
			} else {
				if (cx != display_cx || cy != display_cy) {
					obs_display_resize(interaction_display,
							   cx, cy);
					display_cx = cx;
					display_cy = cy;
				}
			}
		}

		is_interaction_reshow = false;
		ExecuteOnInteraction(
			[](INTERACTION_PTR interaction) {
				interaction->ShowInteractionUI(true);
			},
			true);
	} else {
		if (is_interaction_reshow) {
			is_interaction_reshow = false;
			ExecuteOnInteraction(
				[](INTERACTION_PTR interaction) {
					interaction->BringUIToTop();
				},
				true);
		}

		if (interaction_ui->IsResizing()) {
			if (interaction_display &&
			    obs_display_enabled(interaction_display)) {
				obs_display_set_enabled(interaction_display,
							false);
			}
			// We won't render display while resizing interaction to avoid UI flicker
			return;
		} else {
			if (interaction_display &&
			    !obs_display_enabled(interaction_display)) {
				obs_display_set_enabled(interaction_display,
							true);
			}
		}

		HWND view_hwnd = interaction_ui->GetInteractionView();

		RECT rc;
		GetClientRect(view_hwnd, &rc);

		int cx = RectWidth(rc);
		int cy = RectHeight(rc);

		if (!interaction_display) {
			CreateDisplay(view_hwnd, cx, cy);
		} else {
			if (cx != display_cx || cy != display_cy) {
				obs_display_resize(interaction_display, cx, cy);
				display_cx = cx;
				display_cy = cy;
			}
		}
	}
#elif defined(_APPLE_)
	//TODO
#endif
}

void InteractionDelegate::OnInteractionHide(WindowHandle top_hwnd)
{
#if defined(_WIN32)
	if (::IsWindowVisible(top_hwnd)) {
		ExecuteOnInteraction(
			[](INTERACTION_PTR interaction) {
				interaction->ShowInteractionUI(false);
			},
			true);
	}
#elif defined(_APPLE_)
	//TODO
#endif

	ClearDisplay();
}

void InteractionDelegate::ExecuteOnInteraction(InteractionFunc func, bool async)
{
	extern bool QueueCEFTask(std::function<void()> task);
	if (!async) {
#ifdef ENABLE_BROWSER_QT_LOOP
		if (QThread::currentThread() == qApp->thread()) {
			if (!!interaction_ui)
				func(interaction_ui);
			return;
		}
#else

		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);

		bool success = QueueCEFTask([&]() {
			if (!!interaction_ui)
				func(interaction_ui);
			os_event_signal(finishedEvent);
		});

		if (success) {
			os_event_wait(finishedEvent);
		}
		os_event_destroy(finishedEvent);
#endif

	} else {
		INTERACTION_PTR ui = interaction_ui;
		if (!!ui) {
#ifdef ENABLE_BROWSER_QT_LOOP
            QueueBrowserTask(browser_source->cefBrowser, [=](CefRefPtr<CefBrowser> cefBrowser){
                UNUSED_PARAMETER(cefBrowser);
                if (!!ui)
                    func(ui);
            });
#else
			QueueCEFTask([=]() { func(ui); });
#endif
		}
	}
}

void InteractionDelegate::PostInteractionTitle()
{
	const char *name = obs_source_get_name(source);
	if (name && *name) {
		const char *fmt = obs_module_text("Basic.InteractionWindow");
		if (fmt && *fmt) {
			QString title = QString(fmt).arg(name);
			interaction_ui->PostWindowTitle(title.toUtf8().data());
		}
	}
}

void InteractionDelegate::CheckInteractionShowing()
{
	auto top_hwnd = interaction_ui->GetInteractionMain();
#if defined(_WIN32)
	if (!::IsWindow(top_hwnd)) {
		return;
	}
#elif defined(_APPLE_)
	//TODO
#endif

	if (is_interaction_showing) {
		OnInteractionShow(top_hwnd);
	} else {
		OnInteractionHide(top_hwnd);
	}
}

void InteractionDelegate::CreateInteractionUI()
{
	if (interaction_ui) {
		interaction_ui->CreateInteractionUI();
	}
}

void InteractionDelegate::SetInteractionInfo(int source_width,
					     int source_height,
					     CefRefPtr<CefBrowser> cefBrowser)
{
	if (interaction_ui) {
		interaction_ui->SetInteractionInfo(source_width, source_height,
						   cefBrowser);
	}
}

void InteractionDelegate::SourceRenamed(void *data, calldata_t *params)
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

	if (interaction_delegate->interaction_ui->IsResizing()) {
		return;
	}

	uint32_t sourceCX = std::max(
		obs_source_get_width(interaction_delegate->source), 1u);
	uint32_t sourceCY = std::max(
		obs_source_get_height(interaction_delegate->source), 1u);

	int x, y;
	float scale;
#if defined(_WIN32)
	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
#elif defined(__APPLE__)
    //TODO
#endif

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
	if (!data) {
		assert(false);
		return;
	}

	if (!src) {
		// set global params
#if defined(_WIN32)
		BrowserInteractionMainWin::SetGlobalParam(data);
#elif defined(_APPLE_)
		BrowserInteractionMainMac::SetGlobalParam(data);
#endif
		return;
	} else {
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
}

void InteractionDelegate::GetBrowserData(void *src, obs_data_t *data)
{
	if (!data) {
		return;
	}

	if (!src) {
		// get global params
		obs_data_set_int(data, "interaction_cx",
				 WindowHelper::GetWindowSize().width);
		obs_data_set_int(data, "interaction_cy",
				 WindowHelper::GetWindowSize().height);
	}
}
