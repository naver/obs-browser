#pragma once
#include "browser-panel.hpp"

struct PLSQCefCookieManager : public QCefCookieManager {
	virtual ~PLSQCefCookieManager() override {}

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

class PLSQCefWidget : public QCefWidget {
	Q_OBJECT

protected:
	inline PLSQCefWidget(QWidget *parent) : QCefWidget(parent) {}

public:
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void sendMsg(const std::wstring &type,
			     const std::wstring &msg) = 0;

signals:
	//PRISM/Zhangdewen/20230117/#/libbrowser
	void loadEnded();
	void msgRecevied(const QString &type, const QString &msg);
	//PRISM/ChengBing/20230510/#add browserCLose signal,close loginView cef exception
	void browserClosed();
};

/* ------------------------------------------------------------------------- */

struct PLSQCef : public QCef {
	PLSQCef() : QCef() {}
	virtual ~PLSQCef() override {}

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
};
