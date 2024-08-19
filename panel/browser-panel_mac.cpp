#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"
#include "PLSBrowserPanel.h"

#include <QWindow>
#include <QApplication>

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#endif

#include <obs-module.h>
#include <util/threading.h>
#include <util/base.h>
#include <thread>
#include <cmath>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif

#include <mutex>
#include <vector>
#include <set>

//PRISM/Renjinbo/20240111/stop queue create cef when obs exiting
#include "pls/pls-obs-api.h"

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

static int zoomLvls[] = {25,  33,  50,  67,  75,  80,  90,  100,
			 110, 125, 150, 175, 200, 250, 300, 400};

//PRISM/Zhangdewen/20230117/#/libbrowser
static std::recursive_mutex cef_widgets_mutex;
static std::set<QCefWidgetInternal *> cef_widgets;

//PRISM/Zhangdewen/20230117/#/libbrowser
static void cef_widgets_add(QCefWidgetInternal *impl, bool locked)
{
	if (!locked) {
		std::lock_guard guard(cef_widgets_mutex);
		cef_widgets.insert(impl);
	} else {
		cef_widgets.insert(impl);
	}
}
//PRISM/Zhangdewen/20230117/#/libbrowser
static void cef_widgets_remove(QCefWidgetInternal *impl)
{
	std::lock_guard guard(cef_widgets_mutex);
	cef_widgets.erase(impl);
}
//PRISM/Zhangdewen/20230117/#/libbrowser
template<typename Callback> static void cef_widgets_foreach(Callback callback)
{
	std::lock_guard guard(cef_widgets_mutex);
	for (QCefWidgetInternal *impl : cef_widgets) {
		callback(impl);
	}
}
//PRISM/Zhangdewen/20230117/#/libbrowser
bool cef_widgets_is_valid(QCefWidgetInternal *impl)
{
	if (!impl) {
		return false;
	}

	std::lock_guard guard(cef_widgets_mutex);
	if (cef_widgets.find(impl) != cef_widgets.end()) {
		return true;
	}
	return false;
}
//PRISM/Zhangdewen/20230117/#/libbrowser
bool cef_widgets_sync_call(QCefWidgetInternal *impl,
			   std::function<void(QCefWidgetInternal *)> call)
{
	if (!impl) {
		return false;
	}

	std::lock_guard guard(cef_widgets_mutex);
	if (cef_widgets.find(impl) != cef_widgets.end()) {
		call(impl);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------- */

class CookieCheck : public CefCookieVisitor {
public:
	QCefCookieManager::cookie_exists_cb callback;
	std::string target;
	bool cookie_found = false;

	inline CookieCheck(QCefCookieManager::cookie_exists_cb callback_,
			   const std::string target_)
		: callback(callback_), target(target_)
	{
	}

	virtual ~CookieCheck() { callback(cookie_found); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		CefString cef_name = cookie.name.str;
		std::string name = cef_name;

		if (name == target) {
			cookie_found = true;
			return false;
		}
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieCheck);
};

//PRISM/Zhangdewen/20230308/#/libbrowser
class CookieRead : public CefCookieVisitor {
public:
	std::function<void(const std::list<PLSQCefCookieManager::Cookie> &)>
		m_cookies_cb;
	std::list<PLSQCefCookieManager::Cookie> m_cookies;

	inline CookieRead(const std::function<
			  void(const std::list<PLSQCefCookieManager::Cookie> &)>
				  &cookies_cb)
		: m_cookies_cb(cookies_cb)
	{
	}

	virtual ~CookieRead() { m_cookies_cb(m_cookies); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		m_cookies.push_back(PLSQCefCookieManager::Cookie{
			CefString(cookie.name.str).ToString(),
			CefString(cookie.value.str).ToString(),
			CefString(cookie.domain.str).ToString(),
			CefString(cookie.path.str).ToString(),
			!!cookie.httponly});
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieRead);
};

struct QCefCookieManagerInternal : PLSQCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
	CefRefPtr<CefRequestContext> rc;

	QCefCookieManagerInternal(const std::string &storage_path,
				  bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		if (os_mkdirs(rpath.Get()) == MKDIR_ERROR)
			throw "Failed to create cookie directory";

		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

		CefRequestContextSettings settings;
		settings.persist_user_preferences = 1;
		CefString(&settings.cache_path) = path.Get();
		rc = CefRequestContext::CreateContext(
			settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
	}

	virtual bool DeleteCookies(const std::string &url,
				   const std::string &name) override
	{
		return !!cm ? cm->DeleteCookies(url, name, nullptr) : false;
	}

	virtual bool SetStoragePath(const std::string &storage_path,
				    bool persist_session_cookies) override
	{
		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

		CefRequestContextSettings settings;
		settings.persist_user_preferences = 1;
		CefString(&settings.cache_path) = storage_path;
		rc = CefRequestContext::CreateContext(
			settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
		return true;
	}

	virtual bool FlushStore() override
	{
		return !!cm ? cm->FlushStore(nullptr) : false;
	}

	virtual void CheckForCookie(const std::string &site,
				    const std::string &cookie,
				    cookie_exists_cb callback) override
	{
		if (!cm)
			return;

		CefRefPtr<CookieCheck> c = new CookieCheck(callback, cookie);
		cm->VisitUrlCookies(site, false, c);
	}

	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual void ReadCookies(
		const std::string &site,
		const std::function<void(const std::list<Cookie> &)> &cookies,
		bool isOnlyHttp) override
	{
		if (!cm) {
			cookies({});
			return;
		}

		CefRefPtr<CookieRead> c = new CookieRead(cookies);
		cm->VisitUrlCookies(site, isOnlyHttp, c);
	}
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual void ReadAllCookies(
		const std::function<void(const std::list<Cookie> &)> &cookies)
		override
	{
		if (!cm) {
			cookies({});
			return;
		}

		CefRefPtr<CookieRead> c = new CookieRead(cookies);
		cm->VisitAllCookies(c);
	}
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual bool SetCookie(const std::string &url, const std::string &name,
			       const std::string &value,
			       const std::string &domain,
			       const std::string &path,
			       bool isOnlyHttp) override
	{
		if (!cm)
			return false;

		CefCookie cookie;
		cef_string_utf8_to_utf16(name.c_str(), name.size(),
					 &cookie.name);
		cef_string_utf8_to_utf16(value.c_str(), value.size(),
					 &cookie.value);
		cef_string_utf8_to_utf16(domain.c_str(), domain.size(),
					 &cookie.domain);
		cef_string_utf8_to_utf16(path.c_str(), path.size(),
					 &cookie.path);

		if (isOnlyHttp) {
			cookie.httponly = true;
		} else {
			cookie.secure = true;
			cookie.same_site = CEF_COOKIE_SAME_SITE_NO_RESTRICTION;
		}

		return cm->SetCookie(url, cookie, nullptr);
	}
};

/* ------------------------------------------------------------------------- */

QCefWidgetInternal::QCefWidgetInternal(
	QWidget *parent, const std::string &url_,
	CefRefPtr<CefRequestContext> rqc_,
	//PRISM/Zhangdewen/20230308/#/libbrowser
	const std::string &script_,
	const std::map<std::string, std::string> &headers_, bool allowPopups,
	const QColor &bkgColor_, const std::string &css_)
	: PLSQCefWidget(parent),
	  url(url_),
	  rqc(rqc_),
	  //PRISM/Zhangdewen/20230308/#/libbrowser
	  allowAllPopups_(allowPopups),
	  headers(headers_),
	  bkgColor(bkgColor_),
	  css(css_),
	  script(script_)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);

	cef_widgets_add(this, false);

#ifndef __APPLE__
	window = new QWindow();
	window->setFlags(Qt::FramelessWindowHint);
#endif
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	closeBrowser();
}

extern void cefViewRemoveFromSuperView(void *view);

void QCefWidgetInternal::closeBrowser()
{
	if (!cef_widgets_is_valid(this)) {
		return;
	}
	cef_widgets_remove(this);
	CefRefPtr<CefBrowser> browser = cefBrowser;
	if (!!browser) {
		auto destroyBrowser = [](CefRefPtr<CefBrowser> cefBrowser) {
			CefRefPtr<CefClient> client =
				cefBrowser->GetHost()->GetClient();
			QCefBrowserClient *bc =
				reinterpret_cast<QCefBrowserClient *>(
					client.get());

			if (bc) {
				bc->widget = nullptr;
			}

			cefBrowser->GetHost()->CloseBrowser(true);
		};

		/* So you're probably wondering what's going on here.  If you
		 * call CefBrowserHost::CloseBrowser, and it fails to unload
		 * the web page *before* WM_NCDESTROY is called on the browser
		 * HWND, it will call an internal CEF function
		 * CefBrowserPlatformDelegateNativeWin::CloseHostWindow, which
		 * will attempt to close the browser's main window itself.
		 * Problem is, this closes the root window containing the
		 * browser's HWND rather than the browser's specific HWND for
		 * whatever mysterious reason.  If the browser is in a dock
		 * widget, then the window it closes is, unfortunately, the
		 * main program's window, causing the entire program to shut
		 * down.
		 *
		 * So, instead, before closing the browser, we need to decouple
		 * the browser from the widget.  To do this, we hide it, then
		 * remove its parent. */
#ifdef _WIN32
		HWND hwnd = (HWND)cefBrowser->GetHost()->GetWindowHandle();
		if (hwnd) {
			ShowWindow(hwnd, SW_HIDE);
			SetParent(hwnd, nullptr);
		}
#elif __APPLE__
		// felt hacky, might delete later
		void *view = (id)cefBrowser->GetHost()->GetWindowHandle();
		cefViewRemoveFromSuperView(view);
#endif

		destroyBrowser(browser);
		browser = nullptr;
		cefBrowser = nullptr;
		browserClosed();
	}
}

#ifdef __linux__
static bool XWindowHasAtom(Display *display, Window w, Atom a)
{
	Atom type;
	int format;
	unsigned long nItems;
	unsigned long bytesAfter;
	unsigned char *data = NULL;

	if (XGetWindowProperty(display, w, a, 0, LONG_MAX, False,
			       AnyPropertyType, &type, &format, &nItems,
			       &bytesAfter, &data) != Success)
		return false;

	if (data)
		XFree(data);

	return type != None;
}

/* On Linux / X11, CEF sets the XdndProxy of the toplevel window
 * it's attached to, so that it can read drag events. When this
 * toplevel happens to be OBS Studio's main window (e.g. when a
 * browser panel is docked into to the main window), setting the
 * XdndProxy atom ends up breaking DnD of sources and scenes. Thus,
 * we have to manually unset this atom.
 */
void QCefWidgetInternal::unsetToplevelXdndProxy()
{
	if (!cefBrowser)
		return;

	CefWindowHandle browserHandle =
		cefBrowser->GetHost()->GetWindowHandle();
	Display *xDisplay = cef_get_xdisplay();
	Window toplevel, root, parent, *children;
	unsigned int nChildren;
	bool found = false;

	toplevel = browserHandle;

	// Find the toplevel
	Atom netWmPidAtom = XInternAtom(xDisplay, "_NET_WM_PID", False);
	do {
		if (XQueryTree(xDisplay, toplevel, &root, &parent, &children,
			       &nChildren) == 0)
			return;

		if (children)
			XFree(children);

		if (root == parent ||
		    !XWindowHasAtom(xDisplay, parent, netWmPidAtom)) {
			found = true;
			break;
		}
		toplevel = parent;
	} while (true);

	if (!found)
		return;

	// Check if the XdndProxy property is set
	Atom xDndProxyAtom = XInternAtom(xDisplay, "XdndProxy", False);
	if (needsDeleteXdndProxy &&
	    !XWindowHasAtom(xDisplay, toplevel, xDndProxyAtom)) {
		QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
		return;
	}

	XDeleteProperty(xDisplay, toplevel, xDndProxyAtom);
	needsDeleteXdndProxy = false;
}
#endif

void QCefWidgetInternal::Init()
{
#ifndef __APPLE__
	WId handle = window->winId();
	QSize size = this->size();
	size *= devicePixelRatioF();
	bool success =
		QueueCEFTask([this, handle, size,
#else
	WId handle = winId();
	bool success =
		QueueCEFTask([this, handle,
#endif
			      //PRISM/Zhangdewen/20230308/#/libbrowser
			      bkgColor = CefColorSetARGB(0xff, bkgColor.red(),
							 bkgColor.green(),
							 bkgColor.blue())]() {
			//PRISM/Renjinbo/20240715/#5848/when closed browser, not init
			if (!cef_widgets_is_valid(this)) {
				return;
			}
			//PRISM/Renjinbo/20240111/stop queue create cef when obs exiting
			if (pls_get_obs_exiting())
				return;

			CefWindowInfo windowInfo;

			/* Make sure Init isn't called more than once. */
			if (cefBrowser)
				return;

#ifdef __APPLE__
			QSize size = this->size();
#endif

#if CHROME_VERSION_BUILD < 4430
#ifdef __APPLE__
			windowInfo.SetAsChild((CefWindowHandle)handle, 0, 0,
					      size.width(), size.height());
#else
#ifdef _WIN32
			RECT rc = {0, 0, size.width(), size.height()};
#else
			CefRect rc = {0, 0, size.width(), size.height()};
#endif
			windowInfo.SetAsChild((CefWindowHandle)handle, rc);
#endif
#else
			windowInfo.SetAsChild((CefWindowHandle)handle,
					      CefRect(0, 0, size.width(),
						      size.height()));
#endif

			CefRefPtr<QCefBrowserClient> browserClient =
				new QCefBrowserClient(
					this, script, allowAllPopups_,
					//PRISM/Zhangdewen/20230308/#/libbrowser
					headers, css);

			CefBrowserSettings cefBrowserSettings;
			//PRISM/Zhangdewen/20230308/#/libbrowser
			cefBrowserSettings.background_color = bkgColor;
			cefBrowser = CefBrowserHost::CreateBrowserSync(
				windowInfo, browserClient, url,
				cefBrowserSettings,
				CefRefPtr<CefDictionaryValue>(), rqc);

#ifdef __linux__
			QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
#endif
		});

	if (success) {
		timer.stop();
#ifndef __APPLE__
		if (!container) {
			container =
				QWidget::createWindowContainer(window, this);
			container->show();
		}

		Resize();
#endif
	}
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
#ifndef __APPLE__
	Resize();
}

void QCefWidgetInternal::Resize()
{
	QSize size = this->size() * devicePixelRatioF();

	bool success = QueueCEFTask([this, size]() {
		if (!cefBrowser)
			return;

		CefWindowHandle handle =
			cefBrowser->GetHost()->GetWindowHandle();

		if (!handle)
			return;

#ifdef _WIN32
		SetWindowPos((HWND)handle, nullptr, 0, 0, size.width(),
			     size.height(),
			     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		SendMessage((HWND)handle, WM_SIZE, 0,
			    MAKELPARAM(size.width(), size.height()));
#else
		Display *xDisplay = cef_get_xdisplay();

		if (!xDisplay)
			return;

		XWindowChanges changes = {0};
		changes.x = 0;
		changes.y = 0;
		changes.width = size.width();
		changes.height = size.height();
		XConfigureWindow(xDisplay, (Window)handle,
				 CWX | CWY | CWHeight | CWWidth, &changes);
#if CHROME_VERSION_BUILD >= 4638
		XSync(xDisplay, false);
#endif
#endif
	});

	if (success && container)
		container->resize(size.width(), size.height());
#endif
}

void QCefWidgetInternal::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	if (!cefBrowser && cef_widgets_is_valid(this)) {
		obs_browser_initialize();
		connect(&timer, &QTimer::timeout, this,
			&QCefWidgetInternal::Init);
		timer.start(500);
		Init();
	}
}

QPaintEngine *QCefWidgetInternal::paintEngine() const
{
	return nullptr;
}

void QCefWidgetInternal::setURL(const std::string &url_)
{
	url = url_;
	if (cefBrowser) {
		cefBrowser->GetMainFrame()->LoadURL(url);
	}
}

void QCefWidgetInternal::reloadPage()
{
	if (cefBrowser)
		cefBrowser->ReloadIgnoreCache();
}

//PRISM/Zhangdewen/20230308/#/libbrowser
void QCefWidgetInternal::sendMsg(const std::wstring &type,
				 const std::wstring &msg)
{
	if (!cefBrowser) {
		return;
	}

	QueueCEFTask([browser = cefBrowser, type = std::wstring(type.c_str()),
		      msg = std::wstring(msg.c_str())]() {
		CefRefPtr<CefProcessMessage> message =
			CefProcessMessage::Create("sendToBrowser");
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		args->SetString(0, type);
		args->SetString(1, msg);
		browser->GetMainFrame()->SendProcessMessage(PID_RENDERER,
							    message);
	});
}

void QCefWidgetInternal::setStartupScript(const std::string &script_)
{
	script = script_;
}

void QCefWidgetInternal::executeJavaScript(const std::string &script_)
{
	if (!cefBrowser)
		return;

	CefRefPtr<CefFrame> frame = cefBrowser->GetMainFrame();
	std::string url = frame->GetURL();
	frame->ExecuteJavaScript(script_, url, 0);
}

void QCefWidgetInternal::allowAllPopups(bool allow)
{
	allowAllPopups_ = allow;
}

bool QCefWidgetInternal::zoomPage(int direction)
{
	if (!cefBrowser || direction < -1 || direction > 1)
		return false;

	CefRefPtr<CefBrowserHost> host = cefBrowser->GetHost();
	if (direction == 0) {
		// Reset zoom
		host->SetZoomLevel(0);
		return true;
	}

	int currentZoomPercent = round(pow(1.2, host->GetZoomLevel()) * 100.0);
	int zoomCount = sizeof(zoomLvls) / sizeof(zoomLvls[0]);
	int zoomIdx = 0;

	while (zoomIdx < zoomCount) {
		if (zoomLvls[zoomIdx] == currentZoomPercent) {
			break;
		}
		zoomIdx++;
	}
	if (zoomIdx == zoomCount)
		return false;

	int newZoomIdx = zoomIdx;
	if (direction == -1 && zoomIdx > 0) {
		// Zoom out
		newZoomIdx -= 1;
	} else if (direction == 1 && zoomIdx >= 0 && zoomIdx < zoomCount - 1) {
		// Zoom in
		newZoomIdx += 1;
	}

	if (newZoomIdx != zoomIdx) {
		int newZoomLvl = zoomLvls[newZoomIdx];
		// SetZoomLevel only accepts a zoomLevel, not a percentage
		host->SetZoomLevel(log(newZoomLvl / 100.0) / log(1.2));
		return true;
	}
	return false;
}

//PRISM/Renjinbo/20240308/#4627/make main thread to get is dockWidget
bool QCefWidgetInternal::event(QEvent *event)
{
	if (event->type() == QEvent::ShowToParent ||
	    event->type() == QEvent::ParentChange) {
		bool isDockWidget = false;
		QWidget *parent = this;
		while (parent) {
			if (parent->inherits("QDockWidget")) {
				isDockWidget = true;
				break;
			}
			parent = parent->parentWidget();
		}
		m_isDockWidget = isDockWidget;
	}
	return PLSQCefWidget::event(event);
}

/* ------------------------------------------------------------------------- */
//PRISM/ChengBing/20230425/#porting from PRISM 3.1.3
extern void DispatchJSEvent(std::string eventName, std::string jsonString);

//PRISM/ChengBing/20230425/#porting from PRISM 3.1.3
void DispatchPrismEvent(const char *eventName, const char *jsonString)
{
	std::string event = eventName;
	std::string json = jsonString ? jsonString : "{}";

	auto func = [event, json](CefRefPtr<CefBrowser> browser) {
		if (!browser.get()) {
			return;
		}
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		args->SetString(0, event);
		args->SetString(1, json);

		SendBrowserProcessMessage(browser, PID_RENDERER, msg);
	};

	cef_widgets_foreach([&](QCefWidgetInternal *item) {
		if (item->cefBrowser) {
			QueueCEFTask([=]() {
				//PRISM/Renjinbo/20230102/#3967/crash, in queue visit item is releasing
				if (cef_widgets_is_valid(item))
					func(item->cefBrowser);
			});
		}
	});

	//PRISM/Zhangdewen/20200921/#/fix event not notify web source
	DispatchJSEvent(event, json);
}

struct QCefInternal : PLSQCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      QCefCookieManager *cookie_manager) override;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      const std::string &script,
		      QCefCookieManager *cookie_manager,
		      const std::map<std::string, std::string> &headers =
			      std::map<std::string, std::string>(),
		      bool allowPopups = false,
		      const QColor &initBkgColor = Qt::white,
		      const std::string &css = std::string(),
		      bool showAtLoadEnded = false) override;

	virtual QCefCookieManager *
	create_cookie_manager(const std::string &storage_path,
			      bool persist_session_cookies) override;

	virtual BPtr<char>
	get_cookie_path(const std::string &storage_path) override;

	virtual void add_popup_whitelist_url(const std::string &url,
					     QObject *obj) override;
	virtual void add_force_popup_url(const std::string &url,
					 QObject *obj) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	obs_browser_initialize();
	return false;
}

bool QCefInternal::initialized(void)
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefInternal::wait_for_browser_init(void)
{
	return os_event_wait(cef_started_event) == 0;
}

QCefWidget *QCefInternal::create_widget(QWidget *parent, const std::string &url,
					QCefCookieManager *cm)
{
	QCefCookieManagerInternal *cmi =
		reinterpret_cast<QCefCookieManagerInternal *>(cm);

	return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr);
}

//PRISM/Zhangdewen/20230308/#/libbrowser
QCefWidget *
QCefInternal::create_widget(QWidget *parent, const std::string &url,
			    const std::string &script_, QCefCookieManager *cm,
			    const std::map<std::string, std::string> &headers,
			    bool allowPopups, const QColor &initBkgColor,
			    const std::string &css, bool)
{
	QCefCookieManagerInternal *cmi =
		reinterpret_cast<QCefCookieManagerInternal *>(cm);
	return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr,
				      script_, headers, allowPopups,
				      initBkgColor, css);
}

QCefCookieManager *
QCefInternal::create_cookie_manager(const std::string &storage_path,
				    bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(storage_path,
						     persist_session_cookies);
	} catch (const char *error) {
		blog(LOG_ERROR, "Failed to create cookie manager: %s", error);
		return nullptr;
	}
}

BPtr<char> QCefInternal::get_cookie_path(const std::string &storage_path)
{
	BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
	return os_get_abs_path_ptr(rpath.Get());
}

void QCefInternal::add_popup_whitelist_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	popup_whitelist.emplace_back(url, obj);
}

void QCefInternal::add_force_popup_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	forced_popups.emplace_back(url, obj);
}

extern "C" EXPORT QCef *obs_browser_create_qcef(void)
{
	return new QCefInternal();
}

#define BROWSER_PANEL_VERSION 3

extern "C" EXPORT int obs_browser_qcef_version_export(void)
{
	return BROWSER_PANEL_VERSION;
}
