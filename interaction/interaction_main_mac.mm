#include "interaction_main_mac.h"
#include "browser-client.hpp"
//#include "interaction_hdc.hpp"
#include "interaction_window_helper.h"
#include <util/util.hpp>
#include <util/platform.h>
#include <obs-module.h>

// Need DPI scale
#define INTERACTION_DEFAULT_CX 664
#define INTERACTION_DEFAULT_CY 562
#define INTERACTION_SIZE_IGNORE 400
#define INTERACTION_MIN_CX 332
#define INTERACTION_MIN_CY 280
#define INTERACTION_MAX_CX 7680
#define INTERACTION_MAX_CY 4320
#define CLOSE_BUTTON_CX 22
#define CLOSE_BUTTON_CY 22
#define CLOSE_BUTTON_MARGIN_TOP 10
#define CLOSE_BUTTON_MARGIN_RIGHT 10
#define TOP_REGION_HEIGHT 40
#define BORDER_SIZE 1
#define VIEW_MARGIN 10
#define RESIZE_REGION_SIZE 5
#define TITLE_OFFSET_X 17
#define TITLE_OFFSET_Y 11
#define TITLE_FONT_SIZE 12

#define NORMAL_BK_COLOR 0X272727
#define BORDER_COLOR 0X111111
#define TITLE_TEXT_COLOR 0XFFFFFF
#define RESIZING_DELAY_KEEP 300 // ms
#define RESIZING_DELAY_END 100  // ms

#define PRISM_ICON "images/PRISMLiveStudio.ico"
#define CLOSE_BTN_NORMAL "btn-close-normal.png"
#define CLOSE_BTN_HOVER "btn-close-over.png"
#define CLOSE_BTN_CLICK "btn-close-click.png"
#define CLOSE_BTN_DISABLE "close-btn-disable.png"

//-------------------------------------------------------
std::vector<std::string> BrowserInteractionMainMac::font_list = {
	"Segoe UI", "MalgunGothic", "Malgun Gothic", "Dotum", "Gulin"};
WindowHandle BrowserInteractionMainMac::prism_main = NULL;

void BrowserInteractionMainMac::SetGlobalParam(obs_data_t *private_data)
{
	int cx = obs_data_get_int(private_data, "interaction_cx");
	int cy = obs_data_get_int(private_data, "interaction_cy");
	if (cx >= INTERACTION_MIN_CX && cx <= INTERACTION_MAX_CX &&
	    cy >= INTERACTION_MIN_CY && cy <= INTERACTION_MAX_CY) {
		WindowHelper::SetWindowSize(
			{static_cast<long>(cx), static_cast<long>(cy)});
	}

	WindowHandle hWnd = (WindowHandle)obs_data_get_int(private_data, "prism_hwnd");
    //TODO
//	if (::IsWindow(hWnd)) {
//		BrowserInteractionMainMac::prism_main = hWnd;
//	} else {
//		blog(LOG_WARNING, "Invalid prism HWND for interaction");
//		assert(false);
//	}
}

//------------------------------------------------------------------------
BrowserInteractionMainMac::BrowserInteractionMainMac(SOURCE_HANDLE src)
	: BrowserInteractionMain(src), view_wnd(*this)
{
	view_wnd.ResetInteraction();
}

BrowserInteractionMainMac::~BrowserInteractionMainMac()
{
//	assert(!::IsWindow(hWnd));
//	if (::IsWindow(hWnd)) {
//		should_close = true;
//		::PostMessage(
//			hWnd, WM_CLOSE, NULL,
//			NULL); // Here we'd better use PostMessage instead of SendMessage to avoid deadlock.
//
//		Sleep(200); // For waiting PostMessage be done
//
//		blog(LOG_ERROR, "Interaction is not destroied ! title:%s",
//		     m_utf8_title.c_str());
//		assert(false);
//	}
}

void BrowserInteractionMainMac::CreateInteractionUI()
{
	DCHECK(CefCurrentlyOn(TID_UI));

//	assert(false == should_close);
//	if (::IsWindow(hWnd)) {
//		return;
//	}
//
//	hWnd = ::CreateWindowEx(0, INTERACTION_CLASS_MAIN, L"",
//				WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0,
//				0, 0, 0, NULL, NULL, GetModuleHandle(NULL),
//				NULL);
//
//	if (!::IsWindow(hWnd)) {
//		blog(LOG_ERROR,
//		     "Failed to create interaction main window. error:%u",
//		     GetLastError());
//
//		assert(false);
//		return;
//	}
//
//	blog(LOG_INFO, "Interaction is created. title:%s",
//	     m_utf8_title.c_str());
//
//	::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
//
//	btn_close.Create(hWnd, UI_IDC_BUTTON_CLOSE);
//	btn_close.SetBkColor(NORMAL_BK_COLOR, NORMAL_BK_COLOR, NORMAL_BK_COLOR,
//			     NORMAL_BK_COLOR);
//
//	view_wnd.Create(hWnd);
//
//	float dpi = GetWindowScaleFactor(hWnd);
//	this->SetWindowSize(0, 0, dpi);
//	UpdateImage(dpi);
//
//	::ShowWindow(hWnd, SW_HIDE);

	window_created = true;
}

void BrowserInteractionMainMac::WaitDestroyInteractionUI()
{
	DCHECK(CefCurrentlyOn(TID_UI));

	should_close = true;
//	if (::IsWindow(hWnd)) {
//		::SendMessage(hWnd, WM_CLOSE, NULL, NULL);
//	}
}

void BrowserInteractionMainMac::ShowInteractionUI(bool show)
{
	DCHECK(CefCurrentlyOn(TID_UI));

//	if (!::IsWindow(hWnd)) {
//		return;
//	}
//
//	view_wnd.ResetInteraction();
//	if (show) {
//		ShowUI();
//	} else {
//		::ShowWindow(hWnd, SW_HIDE);
//	}
}

void BrowserInteractionMainMac::BringUIToTop()
{
	DCHECK(CefCurrentlyOn(TID_UI));

//	if (!::IsWindow(hWnd)) {
//		return;
//	}
//
//	::ShowWindow(hWnd, SW_HIDE);
//
//	if (::IsWindow(prism_main)) {
//		MovePosition();
//	}
//
//	// Firstly bring to top, then show window
//	BringWndToTop(hWnd);
//
//	::ShowWindow(hWnd, SW_SHOW);
}

void BrowserInteractionMainMac::SetInteractionInfo(
	int source_width, int source_height, CefRefPtr<CefBrowser> cefBrowser)
{
	DCHECK(CefCurrentlyOn(TID_UI));

	view_wnd.source_cx_ = source_width;
	view_wnd.source_cy_ = source_height;
	view_wnd.browser_ = cefBrowser;
}

void BrowserInteractionMainMac::GetInteractionInfo(
	int &source_width, int &source_height,
	CefRefPtr<CefBrowser> &cefBrowser)
{
	DCHECK(CefCurrentlyOn(TID_UI));

	source_width = view_wnd.source_cx_;
	source_height = view_wnd.source_cy_;
	cefBrowser = view_wnd.browser_;
}

void BrowserInteractionMainMac::OnImeCompositionRangeChanged(
                                                             CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
                                                             const CefRenderHandler::RectList &character_bounds)
{
	DCHECK(CefCurrentlyOn(TID_UI));
	view_wnd.OnImeCompositionRangeChanged(browser, selection_range,
					      character_bounds);
}

void BrowserInteractionMainMac::PostWindowTitle(const char *str)
{
//	if (str && ::IsWindow(hWnd)) {
//		// Note : remember to free memory in handler
//		::PostMessage(hWnd, WM_INTERACTION_SET_TITLE,
//			      (WPARAM)bstrdup(str), 0);
//	}
}

void BrowserInteractionMainMac::PostDestroyInteractionUI()
{
	should_close = true;
//	if (::IsWindow(hWnd)) {
//		::PostMessage(hWnd, WM_CLOSE, NULL, NULL);
//	}
}

WindowHandle BrowserInteractionMainMac::GetInteractionView()
{
	if (window_created) {
		return view_wnd.hwnd_;
	} else {
		return NULL;
	}
}

WindowHandle BrowserInteractionMainMac::GetInteractionMain()
{
	if (window_created) {
		return interaction_wnd;
	} else {
		return NULL;
	}
}

bool BrowserInteractionMainMac::IsResizing()
{
//	DWORD current = timeGetTime();
//	DWORD previous = previous_resize_time;
//
//	if (current <= previous) {
//		return true;
//	}
//
//	DWORD space = current - previous;
//	if (space > RESIZING_DELAY_KEEP) {
//		return false;
//	} else {
//		if (::GetAsyncKeyState(VK_LBUTTON)) {
//			return true;
//		} else {
//			if (space <= RESIZING_DELAY_END) {
//				return true;
//			} else {
//				return false;
//			}
//		}
//	}
    return false;
}

void BrowserInteractionMainMac::OnDestroy()
{
    interaction_wnd = NULL;
	window_created = false;

	blog(LOG_INFO, "Interaction is destroied. title:%s",
	     m_utf8_title.c_str());
}

void BrowserInteractionMainMac::OnPaint()
{
//	PAINTSTRUCT ps;
//	BeginPaint(hWnd, &ps);
//
//	{
//		float dpi = GetWindowScaleFactor(hWnd);
//
//		RECT rc;
//		GetClientRect(hWnd, &rc);
//
//		RECT top_line = {
//			0, LogicalToDevice(TOP_REGION_HEIGHT, dpi), rc.right,
//			LogicalToDevice(TOP_REGION_HEIGHT + BORDER_SIZE, dpi)};
//
//		DoubleBufferHDC memdc(ps.hdc, rc);
//
//		FillSolidRect(memdc.canvas_hdc, &rc, NORMAL_BK_COLOR);
//		FillSolidRect(memdc.canvas_hdc, &top_line, BORDER_COLOR);
//		DrawBorder(memdc.canvas_hdc, rc,
//			   LogicalToDevice(BORDER_SIZE, dpi), BORDER_COLOR);
//
//		DrawTitle(memdc.canvas_hdc, rc, dpi);
//	}
//
//	EndPaint(hWnd, &ps);
}

void BrowserInteractionMainMac::OnSizing()
{
//	RECT rc;
//	GetClientRect(hWnd, &rc);
//
//	int new_cx = RectWidth(rc);
//	int new_cy = RectHeight(rc);
//
//	if (new_cx != previous_size.cx || new_cy != previous_size.cy) {
//		bool old_resizing = IsResizing();
//
//		previous_size = {new_cx, new_cy};
//		previous_resize_time = timeGetTime();
//
//		if (!old_resizing) {
//			// Because of UI flicker whle resizing and d3d render, we set resizing state and handle it in video render thread.
//			// Here we block current thread for a while to make sure video render thread can handle the resizing state.
//			// By this way, we can reduce the chance much about UI flicker.
//			Sleep(60);
//			previous_resize_time = timeGetTime();
//		}
//	}
}

void BrowserInteractionMainMac::OnSize()
{
	SaveUserSize();
	UpdateLayout();
}

void BrowserInteractionMainMac::OnDpiChanged(float dpi)
{
//	UpdateImage(GetWindowScaleFactor(hWnd));
//
//	RECT *rect = reinterpret_cast<RECT *>(lParam);
//	SetWindowPos(hWnd, NULL, rect->left, rect->top, RectWidth(*rect),
//		     RectHeight(*rect), SWP_NOZORDER);
}

void BrowserInteractionMainMac::OnCloseButton()
{
	// Handle this request in BrowserSource::Tick()
	InteractionManager::Instance()->RequestHideInteraction(m_source);
}

bool BrowserInteractionMainMac::ShouldClose()
{
	return should_close;
}

void BrowserInteractionMainMac::SaveUserSize()
{
//	if (!IsFocusInWnd(hWnd)) {
//		return;
//	}
//
//	float dpi = GetWindowScaleFactor(hWnd);
//	if (dpi < 1.0) {
//		return;
//	}
//
//	RECT rc;
//	GetClientRect(hWnd, &rc);
//
//	int cx = (float)RectWidth(rc) / dpi;
//	int cy = (float)RectHeight(rc) / dpi;
//
//	if (cx < INTERACTION_MIN_CX) {
//		cx = INTERACTION_MIN_CX;
//	}
//
//	if (cy < INTERACTION_MIN_CY) {
//		cy = INTERACTION_MIN_CY;
//	}
//
//	WindowHelper::SetWindowSize({cx, cy});
}

void BrowserInteractionMainMac::UpdateLayout()
{
//	if (!::IsWindow(view_wnd.hwnd_)) {
//		return;
//	}
//
//	float dpi = GetWindowScaleFactor(hWnd);
//
//	RECT rc;
//	GetClientRect(hWnd, &rc);
//
//	int close_top = LogicalToDevice(CLOSE_BUTTON_MARGIN_TOP, dpi);
//	int close_left =
//		rc.right -
//		LogicalToDevice(CLOSE_BUTTON_CX + CLOSE_BUTTON_MARGIN_RIGHT,
//				dpi);
//
//	int view_left = LogicalToDevice((BORDER_SIZE + VIEW_MARGIN), dpi);
//	int view_top = LogicalToDevice((TOP_REGION_HEIGHT + VIEW_MARGIN), dpi);
//	int view_right = rc.right - LogicalToDevice(VIEW_MARGIN, dpi);
//	int view_bottom = rc.bottom - LogicalToDevice(VIEW_MARGIN, dpi);
//
//	HDWP hdwp = BeginDeferWindowPos(2);
//	hdwp = DeferWindowPos(hdwp, btn_close.hWnd, NULL, close_left, close_top,
//			      LogicalToDevice(CLOSE_BUTTON_CX, dpi),
//			      LogicalToDevice(CLOSE_BUTTON_CY, dpi),
//			      SWP_NOACTIVATE | SWP_NOZORDER);
//	hdwp = DeferWindowPos(hdwp, view_wnd.hwnd_, NULL, view_left, view_top,
//			      view_right - view_left, view_bottom - view_top,
//			      SWP_NOACTIVATE | SWP_NOZORDER);
//	EndDeferWindowPos(hdwp);
}

void BrowserInteractionMainMac::UpdateImage(float dpi)
{
	blog(LOG_INFO, "Interaction update image by dpi(%f) title:%s", dpi,
	     m_utf8_title.c_str());

	std::string dir;
	if (dpi > 2.4) {
		dir = "images/x3/";
	} else if (dpi > 1.4) {
		dir = "images/x2/";
	} else {
		dir = "images/x1/";
	}

	std::string close_normal = dir + std::string(CLOSE_BTN_NORMAL);
	std::string close_hover = dir + std::string(CLOSE_BTN_HOVER);
	std::string close_click = dir + std::string(CLOSE_BTN_CLICK);
	std::string close_disable = dir + std::string(CLOSE_BTN_DISABLE);

//	btn_close.SetImage(
//		ImageManager::Instance()->LoadImageFile(
//			GetModuleFile(close_normal.c_str()).c_str()),
//		ImageManager::Instance()->LoadImageFile(
//			GetModuleFile(close_hover.c_str()).c_str()),
//		ImageManager::Instance()->LoadImageFile(
//			GetModuleFile(close_click.c_str()).c_str()),
//		ImageManager::Instance()->LoadImageFile(
//			GetModuleFile(close_disable.c_str()).c_str()));
}

void BrowserInteractionMainMac::ShowUI()
{
	// from hide to show, set user's size
//	if (!::IsWindowVisible(hWnd)) {
//		float dpi = GetWindowScaleFactor(hWnd);
//
//		RECT wrc;
//		GetWindowRect(hWnd, &wrc);
//
//		SetWindowSize(wrc.left, wrc.top, dpi);
//	}
//
//	// update position with main dialog
//	assert(::IsWindow(prism_main));
//	if (::IsWindow(prism_main)) {
//		MovePosition();
//	}
//
//	::RedrawWindow(view_wnd.hwnd_, NULL, NULL, RDW_INTERNALPAINT);
//
//	// Firstly bring to top, then show window
//	BringWndToTop(hWnd);
//
//	::ShowWindow(hWnd, SW_SHOW);
}

void BrowserInteractionMainMac::MovePosition()
{
//	RECT prc;
//	GetWindowRect(prism_main, &prc);
//	float pdpi = GetWindowScaleFactor(prism_main);
//
//	RECT rc;
//	GetClientRect(hWnd, &rc);
//	float dpi = GetWindowScaleFactor(hWnd);
//
//	/*
//	NOTE:
//	Here when to calculate new position of interaction aligement with PRISM's main dialog, we should consider their dpi values are different.
//	So, we should transform interaction's resolution to match PRISM's while getting its new lef/top.
//	But we should use interaction's current resolution while moving its position, because it can receive WM_DPICHANGED later for resizing.
//	*/
//
//	int best_cx;
//	int best_cy;
//	if (pdpi > 0.f && dpi > 0.f) {
//		best_cx = (float)RectWidth(rc) * (pdpi / dpi);
//		best_cy = (float)RectHeight(rc) * (pdpi / dpi);
//	} else {
//		best_cx = RectWidth(rc);
//		best_cy = RectHeight(rc);
//	}
//
//	int left = prc.left + (RectWidth(prc) - best_cx) / 2;
//	int top = prc.top + (RectHeight(prc) - best_cy) / 2;
//
//	bool top_checked = false;
//	HMONITOR monitor = MonitorFromWindow(prism_main, MONITOR_DEFAULTTONULL);
//	if (monitor) {
//		MONITORINFOEX mi = {};
//		mi.cbSize = sizeof(mi);
//		if (GetMonitorInfo(monitor, &mi)) {
//			top_checked = true;
//			if (top < mi.rcMonitor.top) {
//				top = mi.rcMonitor.top;
//			}
//		}
//	}
//
//	if (!top_checked) {
//		if (top < prc.top) {
//			top = prc.top;
//		}
//	}
//
//	::MoveWindow(hWnd, left, top, RectWidth(rc), RectHeight(rc), TRUE);
}

void BrowserInteractionMainMac::DrawTitle()
{
//	std::wstring title = TransUtf8ToUnicode(m_utf8_title.c_str());
//	if (title.empty()) {
//		return;
//	}
//
//	const int font_height = LogicalToDevice(TITLE_FONT_SIZE, dpi);
//	HFONT font = CreateCustomFont(font_height, font_list);
//	HGDIOBJ old_font = ::SelectObject(hdc, font);
//
//	RECT text_region;
//	text_region.left = LogicalToDevice(TITLE_OFFSET_X, dpi);
//	text_region.top = LogicalToDevice(TITLE_OFFSET_Y, dpi);
//	text_region.bottom = LogicalToDevice(TOP_REGION_HEIGHT, dpi);
//	text_region.right =
//		rc.right - LogicalToDevice(CLOSE_BUTTON_CX * 2, dpi);
//
//	::SetTextColor(hdc, TITLE_TEXT_COLOR);
//	::SetBkMode(hdc, TRANSPARENT);
//	::DrawTextW(hdc, title.c_str(), static_cast<int>(title.length()),
//		    &text_region, DT_LEFT | DT_TOP | DT_END_ELLIPSIS);
//
//	::SelectObject(hdc, old_font);
//	if (font) {
//		::DeleteObject(font);
//	}
}

void BrowserInteractionMainMac::SetWindowSize(int left, int top, float dpi)
{
//	if (WindowHelper::GetWindowSize().width > INTERACTION_SIZE_IGNORE &&
//	    WindowHelper::GetWindowSize().height > INTERACTION_SIZE_IGNORE) {
//		::MoveWindow(hWnd, left, top,
//			     LogicalToDevice(
//				     WindowHelper::GetWindowSize().width, dpi),
//			     LogicalToDevice(
//				     WindowHelper::GetWindowSize().height, dpi),
//			     TRUE);
//	} else {
//		::MoveWindow(hWnd, left, top,
//			     LogicalToDevice(INTERACTION_DEFAULT_CX, dpi),
//			     LogicalToDevice(INTERACTION_DEFAULT_CY, dpi),
//			     TRUE);
//	}
}

bool BrowserInteractionMainMac::GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX,
                                               int viewY, int &screenX, int &screenY)
{
    
}

bool
BrowserInteractionMainMac::OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor,
               cef_cursor_type_t type,
                                          const CefCursorInfo &custom_cursor_info)
{
    
}
