#pragma once

#include <QTimer>
#include <QPointer>
#include "browser-panel.hpp"
#include "cef-headers.hpp"

#include <vector>
#include <mutex>

struct PopupWhitelistInfo {
	std::string url;
	QPointer<QObject> obj;

	inline PopupWhitelistInfo(const std::string &url_, QObject *obj_)
		: url(url_), obj(obj_)
	{
	}
};

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */

class QCefWidgetInternal : public QCefWidget {
	Q_OBJECT

public:
	QCefWidgetInternal(
		QWidget *parent, const std::string &url,
		CefRefPtr<CefRequestContext> rqc,
		//PRISM/Zhangdewen/20230308/#/libbrowser
		const std::string &script = {},
		const std::map<std::string, std::string> &headers = {},
		bool allowPopups = false, const QColor &bkgColor = Qt::white,
		const std::string &css = {});

	~QCefWidgetInternal();

	CefRefPtr<CefBrowser> cefBrowser;
	std::string url;
	std::string script;
	CefRefPtr<CefRequestContext> rqc;
	QTimer timer;
    bool isCallClosed{false};
#ifndef __APPLE__
	QPointer<QWindow> window;
	QPointer<QWidget> container;
#endif
	bool allowAllPopups_ = false;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	std::map<std::string, std::string> headers;
	QColor bkgColor{Qt::white};
	std::string css;

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;

	virtual void setURL(const std::string &url) override;
	virtual void setStartupScript(const std::string &script) override;
	virtual void allowAllPopups(bool allow) override;
	virtual void closeBrowser() override;
	virtual void reloadPage() override;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual void sendMsg(const std::wstring &type,
			     const std::wstring &msg) override;

	void Resize();

#ifdef __linux__
private:
	bool needsDeleteXdndProxy = true;
	void unsetToplevelXdndProxy();
#endif

public slots:
	void Init();
};
