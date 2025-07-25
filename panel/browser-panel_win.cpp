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

//PRISM/Zhangdewen/20230117/#/libbrowser
#include <set>
#include <qpainter.h>
#include <qevent.h>
#include <qboxlayout.h>
#include <qthread.h>
#include <qpointer.h>

#include <Windows.h>
//PRISM/Renjinbo/20240111/stop queue create cef when obs exiting
#include "pls/pls-obs-api.h"

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

static int zoomLvls[] = {25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400};

//PRISM/Zhangdewen/20230117/#/libbrowser
static std::recursive_mutex cef_widgets_mutex;
static std::set<QCefWidgetImpl *> cef_widgets;

//PRISM/Zhangdewen/20230117/#/libbrowser
static void cef_widgets_add(QCefWidgetImpl *impl, bool locked)
{
	if (!locked) {
		std::lock_guard guard(cef_widgets_mutex);
		cef_widgets.insert(impl);
	} else {
		cef_widgets.insert(impl);
	}
}
//PRISM/Zhangdewen/20230117/#/libbrowser
static void cef_widgets_remove(QCefWidgetImpl *impl)
{
	std::lock_guard guard(cef_widgets_mutex);
	cef_widgets.erase(impl);
	impl->m_popups.clear();
	impl->m_popup = nullptr;
}
//PRISM/Zhangdewen/20230117/#/libbrowser
template<typename Callback> static void cef_widgets_foreach(Callback callback)
{
	std::lock_guard guard(cef_widgets_mutex);
	for (QCefWidgetImpl *impl : cef_widgets) {
		callback(impl);
	}
}
//PRISM/Zhangdewen/20230117/#/libbrowser
bool cef_widgets_is_valid(QCefWidgetImpl *impl)
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
bool cef_widgets_sync_call(QCefWidgetImpl *impl, std::function<void(QCefWidgetImpl *)> call)
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

	inline CookieCheck(QCefCookieManager::cookie_exists_cb callback_, const std::string target_)
		: callback(callback_),
		  target(target_)
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

//PRISM/Zhangdewen/20230117/#/libbrowser
class CookieRead : public CefCookieVisitor {
public:
	std::function<void(const std::list<PLSQCefCookieManager::Cookie> &)> m_cookies_cb;
	std::list<PLSQCefCookieManager::Cookie> m_cookies;

	inline CookieRead(const std::function<void(const std::list<PLSQCefCookieManager::Cookie> &)> &cookies_cb)
		: m_cookies_cb(cookies_cb)
	{
	}

	virtual ~CookieRead() { m_cookies_cb(m_cookies); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		m_cookies.push_back(PLSQCefCookieManager::Cookie{
			CefString(cookie.name.str).ToString(), CefString(cookie.value.str).ToString(),
			CefString(cookie.domain.str).ToString(), CefString(cookie.path.str).ToString(),
			!!cookie.httponly});
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieRead);
};

struct QCefCookieManagerInternal : PLSQCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
	CefRefPtr<CefRequestContext> rc;

	QCefCookieManagerInternal(const std::string &storage_path, bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		if (os_mkdirs(rpath.Get()) == MKDIR_ERROR)
			throw "Failed to create cookie directory";

		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

		CefRequestContextSettings settings;
#if CHROME_VERSION_BUILD <= 6533
		settings.persist_user_preferences = 1;
#endif
		CefString(&settings.cache_path) = path.Get();
		rc = CefRequestContext::CreateContext(settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
	}

	virtual bool DeleteCookies(const std::string &url, const std::string &name) override
	{
		return !!cm ? cm->DeleteCookies(url, name, nullptr) : false;
	}

	virtual bool SetStoragePath(const std::string &storage_path, bool persist_session_cookies) override
	{
		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

		CefRequestContextSettings settings;
#if CHROME_VERSION_BUILD <= 6533
		settings.persist_user_preferences = 1;
#endif
		CefString(&settings.cache_path) = storage_path;
		rc = CefRequestContext::CreateContext(settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
		return true;
	}

	virtual bool FlushStore() override { return !!cm ? cm->FlushStore(nullptr) : false; }

	virtual void CheckForCookie(const std::string &site, const std::string &cookie,
				    cookie_exists_cb callback) override
	{
		if (!cm)
			return;

		CefRefPtr<CookieCheck> c = new CookieCheck(callback, cookie);
		cm->VisitUrlCookies(site, false, c);
	}

	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void ReadCookies(const std::string &site, const std::function<void(const std::list<Cookie> &)> &cookies,
				 bool isOnlyHttp) override
	{
		if (!cm) {
			cookies({});
			return;
		}

		CefRefPtr<CookieRead> c = new CookieRead(cookies);
		cm->VisitUrlCookies(site, isOnlyHttp, c);
	}
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void ReadAllCookies(const std::function<void(const std::list<Cookie> &)> &cookies) override
	{
		if (!cm) {
			cookies({});
			return;
		}

		CefRefPtr<CookieRead> c = new CookieRead(cookies);
		cm->VisitAllCookies(c);
	}
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual bool SetCookie(const std::string &url, const std::string &name, const std::string &value,
			       const std::string &domain, const std::string &path, bool isOnlyHttp) override
	{
		if (!cm)
			return false;

		CefCookie cookie;
		cef_string_utf8_to_utf16(name.c_str(), name.size(), &cookie.name);
		cef_string_utf8_to_utf16(value.c_str(), value.size(), &cookie.value);
		cef_string_utf8_to_utf16(domain.c_str(), domain.size(), &cookie.domain);
		cef_string_utf8_to_utf16(path.c_str(), path.size(), &cookie.path);

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

//PRISM/Zhangdewen/20230117/#/libbrowser
QCefWidgetImpl::QCefWidgetImpl(QWidget *parent, const std::string &url, const std::string &script, Rqc rqc,
			       const Headers &headers, bool allowPopups, QCefBrowserPopupDialog *popup,
			       const QColor &initBkgColor, const std::string &css, bool showAtLoadEnded)
	: QWidget(parent),
	  m_browser(),
	  m_url(url),
	  m_script(script),
	  m_rqc(rqc),
	  m_headers(headers),
	  m_allowPopups(allowPopups),
	  m_popups(),
	  m_popup(popup),
	  m_initBkgColor(initBkgColor),
	  m_css(css),
	  m_showAtLoadEnded(showAtLoadEnded)
{
	setAttribute(Qt::WA_NativeWindow);
	setFocusPolicy(Qt::ClickFocus);
	connect(this, SIGNAL(loadEnded()), this, SLOT(onLoadEnded()));
	//PRISM/ChengBing/20230510/#add browserCLose signal,close loginView cef exception
	connect(this, SIGNAL(destroyed()), parent, SIGNAL(browserClosed()));
	if (showAtLoadEnded) {
		hide();
	}

	//PRISM/jimboRen/20250604/#/add log
	blog(LOG_INFO, "cef create cef widget. internal:%p url:see kr log", this);
	blog(LOG_DEBUG, "cef create cef widget. internal:%p url:%s", this, m_url.c_str());
	if (pls_get_obs_exiting()) {
		blog(LOG_ERROR, "cef create cef widget in app exiting. internal:%p, url:see kr log", this);
		blog(LOG_DEBUG, "cef create cef widget in app exiting. internal:%p, url:%s", this, m_url.c_str());
		assert(false && "can't create widget in app exiting");
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
QCefWidgetImpl::~QCefWidgetImpl() {}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::init()
{
	executeOnCef([this,
#if defined(Q_OS_WIN)
		      size = size() * devicePixelRatioF(),
#elif defined(Q_OS_MACOS)
		      size = size(),
#endif
		      handle = (CefWindowHandle)winId(),
		      bkgColor = CefColorSetARGB(0xff, m_initBkgColor.red(), m_initBkgColor.green(),
						 m_initBkgColor.blue())]() {
		//PRISM/Renjinbo/20240111/stop queue create cef when obs exiting
		if (pls_get_obs_exiting())
			return;

		if (!cef_widgets_is_valid(this) || m_browser) {
			return;
		}

		CefWindowInfo windowInfo;
#if CHROME_VERSION_BUILD >= 6533
		windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
#endif
		windowInfo.SetAsChild(handle, CefRect(0, 0, size.width(), size.height()));

		CefRefPtr<QCefBrowserClient> browserClient = new QCefBrowserClient(this);

		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.background_color = bkgColor;
		//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
		blog(LOG_INFO, "[obs-browser]: call create browser sync method. internal:%p", this);
		m_browser = CefBrowserHost::CreateBrowserSync(windowInfo, browserClient, m_url, cefBrowserSettings,
							      CefRefPtr<CefDictionaryValue>(), m_rqc);
	});
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::destroy(bool restart)
{
	//PRISM/jimboRen/20250604/#/add log
	blog(LOG_INFO, "cef close browser with try: internal:%p browser:%p restart:%s url:see kr log", this,
	     (!!m_browser) ? m_browser.get() : 0x00, restart ? "true" : "false");
	blog(LOG_DEBUG, "cef close browser with try: internal:%p browser:%p restart:%s url:%s", this,
	     (!!m_browser) ? m_browser.get() : 0x00, restart ? "true" : "false", m_url.c_str());
	if (!restart) {
		hide();
		setParent(nullptr);
	}

	CefRefPtr<CefBrowser> browser = m_browser;
	if (browser) {
		m_browser = nullptr;

#if defined(Q_OS_WIN)
		HWND hwnd = (HWND)browser->GetHost()->GetWindowHandle();
		if (hwnd) {
			ShowWindow(hwnd, SW_HIDE);
			SetParent(hwnd, nullptr);
		}
#elif defined(Q_OS_MACOS)
		void *view = (id)browser->GetHost()->GetWindowHandle();
		if (*((bool *)view))
			((void (*)(id, SEL))objc_msgSend)((id)view, sel_getUid("removeFromSuperview"));
#endif
	}

	executeOnCef([browser, restart, this]() {
		if (browser) {
			browser->GetHost()->WasHidden(true);
			browser->GetHost()->CloseBrowser(true);
		}

		if (!restart) {
			deleteLater();
		} else {
			QMetaObject::invokeMethod(this, "init");
		}
	});
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::paintEvent(QPaintEvent *event)
{
	QPainter p(this);
	p.fillRect(rect(), m_initBkgColor);
}

//PRISM/Zhangdewen/20230428/#/size not changed
bool QCefWidgetImpl::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
	MSG *msg = (MSG *)message;
	switch (msg->message) {
	case WM_SIZE:
		resize(false, QSize(LOWORD(msg->lParam), HIWORD(msg->lParam)));
		break;
	default:
		break;
	}
	return QWidget::nativeEvent(eventType, message, result);
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::setURL(const std::string &url)
{
	m_url = url;
	if (m_browser) {
		m_browser->GetMainFrame()->LoadURL(url);
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::reloadPage()
{
	if (m_browser) {
		m_browser->ReloadIgnoreCache();
	}
}

bool QCefWidgetImpl::zoomPage(int direction)
{
	if (!m_browser || direction < -1 || direction > 1)
		return false;

	CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
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

void QCefWidgetImpl::executeJavaScript(const std::string &script)
{
	if (!m_browser)
		return;

	CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
	std::string url = frame->GetURL();
	frame->ExecuteJavaScript(script, url, 0);
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::sendMsg(const std::wstring &type, const std::wstring &msg)
{
	if (!m_browser) {
		return;
	}

	executeOnCef([browser = m_browser, type = std::wstring(type.c_str()), msg = std::wstring(msg.c_str())]() {
		CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("sendToBrowser");
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		args->SetString(0, type);
		args->SetString(1, msg);
		browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
	});
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::resize(bool bImmediately, QSize size)
{
#if defined(Q_OS_WIN)
	if (size.isEmpty())
		size = this->size() * devicePixelRatioF();
	auto func = [this, size]() {
		if (!cef_widgets_is_valid(this) || !m_browser)
			return;

		HWND hwnd = m_browser->GetHost()->GetWindowHandle();
		MoveWindow(hwnd, 0, 0, size.width(), size.height(), TRUE);
	};

	if (bImmediately) {
		func();
	} else {
		executeOnCef(func);
	}
#endif
}
void QCefWidgetImpl::CloseSafely()
{
	emit readyToClose();
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::executeOnCef(std::function<void()> func)
{
	QueueCEFTask([=]() { func(); });
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::executeOnCef(std::function<void(CefRefPtr<CefBrowser>)> func)
{
	CefRefPtr<CefBrowser> browser = m_browser;
	if (browser) {
		QueueCEFTask([=]() { func(browser); });
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::attachBrowser(CefRefPtr<CefBrowser> browser)
{
	m_browser = browser;
	resize(true);

	if (m_popup) {
		QMetaObject::invokeMethod(m_popup, "open");
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::detachBrowser()
{
	m_browser = nullptr;

	if (m_popup) {
		QMetaObject::invokeMethod(m_popup, "accept");
		m_popup = nullptr;
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetImpl::onLoadEnded()
{
	if (m_showAtLoadEnded && !isVisible() && parentWidget()) {
		this->setGeometry(parentWidget()->rect());
		show();
	}
}

QCefWidgetInternal::QCefWidgetInternal(QWidget *parent, const std::string &url, CefRefPtr<CefRequestContext> rqc,
				       //PRISM/Zhangdewen/20230117/#/libbrowser
				       const std::string &script, const QCefWidgetImpl::Headers &headers,
				       bool allowPopups, bool callInit, QCefBrowserPopupDialog *popup, bool locked,
				       const QColor &initBkgColor, const std::string &css, bool showAtLoadEnded)
	: PLSQCefWidget(parent)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	blog(LOG_INFO, "try to init cef browser from QCefWidgetInternal init method");
	obs_browser_initialize();
	setAttribute(Qt::WA_NativeWindow);
	setFocusPolicy(Qt::ClickFocus);

	m_impl = new QCefWidgetImpl(this, url, script, rqc, headers, allowPopups, popup, initBkgColor, css,
				    showAtLoadEnded);
	cef_widgets_add(m_impl, locked);

	connect(m_impl, &QCefWidgetImpl::titleChanged, this, &QCefWidgetInternal::titleChanged);
	connect(m_impl, &QCefWidgetImpl::urlChanged, this, &QCefWidgetInternal::urlChanged);
	connect(m_impl, &QCefWidgetImpl::loadEnded, this, &QCefWidgetInternal::loadEnded);
	connect(m_impl, &QCefWidgetImpl::msgRecevied, this, &QCefWidgetInternal::msgRecevied);

	if (callInit) {
		m_impl->init();
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
QCefWidgetInternal::QCefWidgetInternal(QCefBrowserPopupDialog *parent, QCefWidgetImpl *checkImpl, bool locked)
	: QCefWidgetInternal(parent, checkImpl->m_url, checkImpl->m_rqc, checkImpl->m_script, checkImpl->m_headers,
			     checkImpl->m_allowPopups, false, parent, locked)
{
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	closeBrowser();
}

//PRISM/Zhangdewen/20230117/#/libbrowser
bool QCefWidgetInternal::event(QEvent *event)
{
	bool result = QCefWidget::event(event);
	if (event->type() == QEvent::ParentChange && m_impl) {
		m_impl->resize(false);
	}

	//PRISM/Renjinbo/20240308/#4627/make main thread to get is dockWidget
	if (event->type() == QEvent::ShowToParent || event->type() == QEvent::ParentChange) {
		if (m_impl) {
			bool isDockWidget = false;
			QWidget *parent = m_impl;
			while (parent) {
				if (parent->inherits("QDockWidget")) {
					isDockWidget = true;
					break;
				}
				parent = parent->parentWidget();
			}
			m_impl->m_isDockWidget = isDockWidget;
		}
	}

	return result;
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetInternal::paintEvent(QPaintEvent *event)
{
	if (m_impl) {
		QPainter p(this);
		p.fillRect(rect(), m_impl->m_initBkgColor);
	}
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		m_impl->setGeometry(0, 0, event->size().width(), event->size().height());
		//PRISM/Renjinbo/20231129/#3223/ui size error
		if (event->oldSize() == QSize(-1, -1)) {
			m_impl->resize(false, event->size());
			m_impl->resize(true, event->size());
		}
	}
}

void QCefWidgetInternal::setURL(const std::string &url)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		m_impl->setURL(url);
	}
}

void QCefWidgetInternal::setStartupScript(const std::string &script)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		m_impl->m_script = script;
	}
}

void QCefWidgetInternal::executeJavaScript(const std::string &script)
{
	if (m_impl) {
		m_impl->executeJavaScript(script);
	}
}

void QCefWidgetInternal::allowAllPopups(bool allow)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		m_impl->m_allowPopups = allow;
	}
}

bool QCefWidgetInternal::zoomPage(int direction)
{
	if (m_impl) {
		return m_impl->zoomPage(direction);
	}
	return false;
}

void QCefWidgetInternal::closeBrowser()
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		cef_widgets_remove(m_impl);
		m_impl->destroy();
		m_impl = nullptr;
	}
}

void QCefWidgetInternal::reloadPage()
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (m_impl) {
		m_impl->reloadPage();
	}
}

//PRISM/Zhangdewen/20230117/#/libbrowser
void QCefWidgetInternal::sendMsg(const std::wstring &type, const std::wstring &msg)
{
	if (m_impl) {
		m_impl->sendMsg(type, msg);
	}
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QCefBrowserPopupDialog::QCefBrowserPopupDialog(QCefWidgetImpl *checkImpl, QWidget *parent, bool locked)
	: QDialog(parent),
	  m_checkImpl(checkImpl)
{
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowTitle(" ");
	setWindowIcon(QIcon());
	setGeometry(parent->geometry());

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_internal = new QCefWidgetInternal(this, checkImpl, locked);
	layout->addWidget(m_internal);
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QCefBrowserPopupDialog::~QCefBrowserPopupDialog()
{
	m_checkImpl = nullptr;
	delete m_internal;
	m_internal = nullptr;
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QCefBrowserPopupDialog *QCefBrowserPopupDialog::create(CefWindowHandle &handle, QCefWidgetImpl *checkImpl,
						       QWidget *parent, bool locked)
{
	if (QThread::currentThread() == qApp->thread()) {
		QCefBrowserPopupDialog *dialog = new QCefBrowserPopupDialog(checkImpl, parent, locked);
		handle = (CefWindowHandle)dialog->impl()->winId();
		return dialog;
	}

	QCefBrowserPopupDialog *dialog = nullptr;
	QMetaObject::invokeMethod(
		qApp,
		[&dialog, &handle, checkImpl, parent, locked]() { dialog = create(handle, checkImpl, parent, locked); },
		Qt::BlockingQueuedConnection);

	return dialog;
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
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		args->SetString(0, event);
		args->SetString(1, json);
		SendBrowserProcessMessage(browser, PID_RENDERER, msg);
	};

	cef_widgets_foreach([&](auto item) { item->executeOnCef(func); });

	//PRISM/Zhangdewen/20200921/#/fix event not notify web source
	DispatchJSEvent(event, json);
}

struct QCefInternal : PLSQCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	virtual QCefWidget *create_widget(QWidget *parent, const std::string &url,
					  QCefCookieManager *cookie_manager) override;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url, const std::string &script,
		      QCefCookieManager *cookie_manager,
		      const std::map<std::string, std::string> &headers = std::map<std::string, std::string>(),
		      bool allowPopups = false, const QColor &initBkgColor = Qt::white,
		      const std::string &css = std::string(), bool showAtLoadEnded = false) override;

	virtual QCefCookieManager *create_cookie_manager(const std::string &storage_path,
							 bool persist_session_cookies) override;

	virtual BPtr<char> get_cookie_path(const std::string &storage_path) override;

	virtual void add_popup_whitelist_url(const std::string &url, QObject *obj) override;
	virtual void add_force_popup_url(const std::string &url, QObject *obj) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	//PRISM/Renjinbo/20241122/#PRISM_PC_NELO-82/add log
	blog(LOG_INFO, "try to init cef browser from QCefInternal init_browser method");
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

QCefWidget *QCefInternal::create_widget(QWidget *parent, const std::string &url, QCefCookieManager *cm)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	return create_widget(parent, url, {}, cm);
}

//PRISM/Zhangdewen/20230117/#/libbrowser
QCefWidget *QCefInternal::create_widget(QWidget *parent, const std::string &url, const std::string &script,
					QCefCookieManager *cm, const std::map<std::string, std::string> &headers,
					bool allowPopups, const QColor &initBkgColor, const std::string &css,
					bool showAtLoadEnded)
{
	QCefCookieManagerInternal *cmi = reinterpret_cast<QCefCookieManagerInternal *>(cm);

	return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr, script, headers, allowPopups, true, nullptr,
				      false, initBkgColor, css, showAtLoadEnded);
}

QCefCookieManager *QCefInternal::create_cookie_manager(const std::string &storage_path, bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(storage_path, persist_session_cookies);
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
