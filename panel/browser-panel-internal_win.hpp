#pragma once

#include <QTimer>
#include <QPointer>
#include "PLSBrowserPanel.h"
#include "cef-headers.hpp"

#include <vector>
#include <mutex>

//PRISM/Zhangdewen/20230117/#/libbrowser
#include <qdialog.h>

struct PopupWhitelistInfo {
	std::string url;
	QPointer<QObject> obj;

	inline PopupWhitelistInfo(const std::string &url_, QObject *obj_) : url(url_), obj(obj_) {}
};

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */

//PRISM/Zhangdewen/20230117/#/libbrowser
class QCefBrowserClient;
class QCefBrowserPopupClient;
class QCefBrowserPopupDialog;
class QCefWidgetImpl;
class QCefWidgetInternal;

//PRISM/Zhangdewen/20230117/#/libbrowser
bool cef_widgets_is_valid(QCefWidgetImpl *impl);
bool cef_widgets_sync_call(QCefWidgetImpl *impl, std::function<void(QCefWidgetImpl *)> call);

//PRISM/Zhangdewen/20230117/#/libbrowser
class QCefWidgetImpl : public QWidget {
	Q_OBJECT

public:
	using Rqc = CefRefPtr<CefRequestContext>;
	using Headers = std::map<std::string, std::string>;

public:
	QCefWidgetImpl(QWidget *parent, const std::string &url, const std::string &script, Rqc rqc,
		       const Headers &headers, bool allowPopups, QCefBrowserPopupDialog *popup,
		       const QColor &initBkgColor, const std::string &css, bool showAtLoadEnded);
	~QCefWidgetImpl();

	CefRefPtr<CefBrowser> m_browser;

	std::string m_url;
	std::string m_script;
	CefRefPtr<CefRequestContext> m_rqc;
	Headers m_headers;

	bool m_allowPopups = false;
	QList<CefRefPtr<QCefBrowserPopupClient>> m_popups;
	QCefBrowserPopupDialog *m_popup = nullptr;
	QColor m_initBkgColor{Qt::white};
	std::string m_css;
	bool m_showAtLoadEnded = false;
	//PRISM/Renjinbo/20240308/#4627/make main thread to get is dockWidget
	std::atomic<bool> m_isDockWidget{false};
	//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
	int restartCount = 0;

	friend class QCefWidgetInternal;
	friend class QCefBrowserClient;

private slots:
	void init();
	void destroy(bool restart = false);

public:
	void paintEvent(QPaintEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

	void setURL(const std::string &url);
	void reloadPage();
	bool zoomPage(int direction);
	void executeJavaScript(const std::string &script);
	void sendMsg(const std::wstring &type, const std::wstring &msg);
	void resize(bool bImmediately, QSize size = QSize());

	static void executeOnCef(std::function<void()> func);
	void executeOnCef(std::function<void(CefRefPtr<CefBrowser>)> func);

	void attachBrowser(CefRefPtr<CefBrowser> browser);
	void detachBrowser();
	void CloseSafely();

signals:
	void titleChanged(const QString &title);
	void urlChanged(const QString &url);
	void loadEnded();
	void msgRecevied(const QString &type, const QString &msg);
	void readyToClose();

private slots:
	void onLoadEnded();
};

class QCefWidgetInternal : public PLSQCefWidget {
	Q_OBJECT

public:
	QCefWidgetInternal(QWidget *parent, const std::string &url, CefRefPtr<CefRequestContext> rqc,
			   //PRISM/Zhangdewen/20230117/#/libbrowser
			   const std::string &script, const QCefWidgetImpl::Headers &headers, bool allowPopups,
			   bool callInit, QCefBrowserPopupDialog *popup = nullptr, bool locked = false,
			   const QColor &initBkgColor = Qt::white, const std::string &css = std::string(),
			   bool showAtLoadEnded = false);
	//PRISM/Zhangdewen/20230117/#/libbrowser
	QCefWidgetInternal(QCefBrowserPopupDialog *parent, QCefWidgetImpl *checkImpl, bool locked = false);
	~QCefWidgetInternal();

	//PRISM/Zhangdewen/20230117/#/libbrowser
	QCefWidgetImpl *m_impl = nullptr;

	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual bool event(QEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;

	virtual void setURL(const std::string &url) override;
	virtual void setStartupScript(const std::string &script) override;
	virtual void allowAllPopups(bool allow) override;
	virtual void closeBrowser() override;
	virtual void reloadPage() override;
	virtual bool zoomPage(int direction) override;
	virtual void executeJavaScript(const std::string &script) override;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void sendMsg(const std::wstring &type, const std::wstring &msg) override;
};

//PRISM/Zhangdewen/20230117/#/libbrowser
class QCefBrowserPopupDialog : public QDialog {
	Q_OBJECT

private:
	QCefBrowserPopupDialog(QCefWidgetImpl *checkImpl, QWidget *parent = nullptr, bool locked = false);
	~QCefBrowserPopupDialog() override;

public:
	static QCefBrowserPopupDialog *create(CefWindowHandle &handle, QCefWidgetImpl *checkImpl, QWidget *parent,
					      bool locked = false);

public:
	QCefWidgetImpl *checkImpl() { return m_checkImpl; }
	QCefWidgetImpl *impl() { return m_internal->m_impl; }
	QCefWidgetInternal *internal() { return m_internal; }

private:
	QCefWidgetImpl *m_checkImpl = nullptr;
	QCefWidgetInternal *m_internal = nullptr;
};
