#pragma once

#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <QWidget>

#include <functional>
#include <string>

#if defined(__APPLE__)
#include <dlfcn.h>
#endif

#ifdef ENABLE_WAYLAND
#include <obs-nix-platform.h>
#endif

struct QCefCookieManager {
	virtual ~QCefCookieManager() {}

	virtual bool DeleteCookies(const std::string &url,
				   const std::string &name) = 0;
	virtual bool SetStoragePath(const std::string &storage_path,
				    bool persist_session_cookies = false) = 0;
	virtual bool FlushStore() = 0;

	typedef std::function<void(bool)> cookie_exists_cb;

	virtual void CheckForCookie(const std::string &site,
				    const std::string &cookie,
				    cookie_exists_cb callback) = 0;

	//PRISM/Zhangdewen/20230117/#/libbrowser
	struct Cookie {
		std::string name;
		std::string value;
		std::string domain;
		std::string path;
		bool isOnlyHttp = false;
	};
	virtual void ReadCookies(
		const std::string &site,
		const std::function<void(const std::list<Cookie> &)> &cookies,
		bool isOnlyHttp) = 0;
	virtual void
	ReadAllCookies(const std::function<void(const std::list<Cookie> &)>
			       &cookies) = 0;
	virtual bool SetCookie(const std::string &url, const std::string &name,
			       const std::string &value,
			       const std::string &domain,
			       const std::string &path, bool isOnlyHttp) = 0;
};

/* ------------------------------------------------------------------------- */

class QCefWidget : public QWidget {
	Q_OBJECT

protected:
	inline QCefWidget(QWidget *parent) : QWidget(parent) {}

public:
	virtual void setURL(const std::string &url) = 0;
	virtual void setStartupScript(const std::string &script) = 0;
	virtual void allowAllPopups(bool allow) = 0;
	virtual void reloadPage() = 0;
	virtual void closeBrowser() = 0;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void sendMsg(const std::wstring &type,
			     const std::wstring &msg) = 0;

signals:
	void titleChanged(const QString &title);
	void urlChanged(const QString &url);
	//PRISM/Zhangdewen/20230117/#/libbrowser
	void loadEnded();
	void msgRecevied(const QString &type, const QString &msg);
	//PRISM/ChengBing/20230510/#add browserCLose signal,close loginView cef exception
	void browserClosed();
};

/* ------------------------------------------------------------------------- */

struct QCef {
	virtual ~QCef() {}

	virtual bool init_browser(void) = 0;
	virtual bool initialized(void) = 0;
	virtual bool wait_for_browser_init(void) = 0;

	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      QCefCookieManager *cookie_manager = nullptr) = 0;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      const std::string &script,
		      QCefCookieManager *cookie_manager,
		      const std::map<std::string, std::string> &headers =
			      std::map<std::string, std::string>(),
		      bool allowPopups = false,
		      const QColor &initBkgColor = Qt::white,
		      const std::string &css = std::string(),
		      bool showAtLoadEnded = false) = 0;

	virtual QCefCookieManager *
	create_cookie_manager(const std::string &storage_path,
			      bool persist_session_cookies = false) = 0;

	virtual BPtr<char> get_cookie_path(const std::string &storage_path) = 0;

	virtual void add_popup_whitelist_url(const std::string &url,
					     QObject *obj) = 0;
	virtual void add_force_popup_url(const std::string &url,
					 QObject *obj) = 0;
};

static inline void *get_browser_lib()
{
	// Disable panels on Wayland for now
	bool isWayland = false;
#ifdef ENABLE_WAYLAND
	isWayland = obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND;
#endif
	if (isWayland)
		return nullptr;

	obs_module_t *browserModule = obs_get_module("obs-browser");

	if (!browserModule)
		return nullptr;

	return obs_get_module_lib(browserModule);
}

static inline QCef *obs_browser_init_panel(void)
{
	void *lib = get_browser_lib();
	QCef *(*create_qcef)(void) = nullptr;

	if (!lib)
		return nullptr;

	create_qcef =
		(decltype(create_qcef))os_dlsym(lib, "obs_browser_create_qcef");

	if (!create_qcef)
		return nullptr;

	return create_qcef();
}

static inline int obs_browser_qcef_version(void)
{
	void *lib = get_browser_lib();
	int (*qcef_version)(void) = nullptr;

	if (!lib)
		return 0;

	qcef_version = (decltype(qcef_version))os_dlsym(
		lib, "obs_browser_qcef_version_export");

	if (!qcef_version)
		return 0;

	return qcef_version();
}
