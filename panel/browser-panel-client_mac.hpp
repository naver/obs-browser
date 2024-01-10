#pragma once

#include "cef-headers.hpp"
#include "browser-panel-internal.hpp"

#include <string>
#include <QDialog>

class QCefBrowserClient : public CefClient,
			  public CefDisplayHandler,
			  public CefRequestHandler,
			  public CefLifeSpanHandler,
			  public CefContextMenuHandler,
			  public CefLoadHandler,
			  public CefKeyboardHandler,
			  public CefFocusHandler,
			  public CefJSDialogHandler,
			  //PRISM/Zhangdewen/20230308/#/libbrowser
			  public CefResourceRequestHandler {

public:
	inline QCefBrowserClient(
		QCefWidgetInternal *widget_, const std::string &script_,
		bool allowAllPopups_,
		//PRISM/Zhangdewen/20230308/#/libbrowser
		const std::map<std::string, std::string> &headers_,
		const std::string &css_)
		: widget(widget_),
		  script(script_),
		  allowAllPopups(allowAllPopups_),
		  //PRISM/Zhangdewen/20230308/#/libbrowser
		  headers(headers_),
		  css(css_)
	{
	}

	/* CefClient */
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;
	virtual CefRefPtr<CefFocusHandler> GetFocusHandler() override;
	virtual CefRefPtr<CefContextMenuHandler>
	GetContextMenuHandler() override;
	virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override;

	/* CefDisplayHandler */
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
				   const CefString &title) override;

	/* CefRequestHandler */
	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefRequest> request,
				    bool user_gesture,
				    bool is_redirect) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefLoadHandler::ErrorCode errorCode,
				 const CefString &errorText,
				 const CefString &failedUrl) override;

	virtual bool OnOpenURLFromTab(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url,
		CefRequestHandler::WindowOpenDisposition target_disposition,
		bool user_gesture) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url, const CefString &target_frame_name,
		CefLifeSpanHandler::WindowOpenDisposition target_disposition,
		bool user_gesture, const CefPopupFeatures &popupFeatures,
		CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client,
		CefBrowserSettings &settings,
		CefRefPtr<CefDictionaryValue> &extra_info,
		bool *no_javascript_access) override;

	/* CefFocusHandler */
	virtual bool OnSetFocus(CefRefPtr<CefBrowser> browser,
				CefFocusHandler::FocusSource source) override;

	/* CefContextMenuHandler */
	virtual void
	OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
			    CefRefPtr<CefFrame> frame,
			    CefRefPtr<CefContextMenuParams> params,
			    CefRefPtr<CefMenuModel> model) override;

#if defined(_WIN32)
	virtual bool
	RunContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		       CefRefPtr<CefContextMenuParams> params,
		       CefRefPtr<CefMenuModel> model,
		       CefRefPtr<CefRunContextMenuCallback> callback) override;
#endif

	virtual bool OnContextMenuCommand(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefContextMenuParams> params, int command_id,
		CefContextMenuHandler::EventFlags event_flags) override;

	/* CefLoadHandler */
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
			       CefRefPtr<CefFrame> frame,
			       int httpStatusCode) override;

	/* CefKeyboardHandler */
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				   const CefKeyEvent &event,
				   CefEventHandle os_event,
				   bool *is_keyboard_shortcut) override;

	/* CefJSDialogHandler */
	virtual bool OnJSDialog(CefRefPtr<CefBrowser> browser,
				const CefString &origin_url,
				CefJSDialogHandler::JSDialogType dialog_type,
				const CefString &message_text,
				const CefString &default_prompt_text,
				CefRefPtr<CefJSDialogCallback> callback,
				bool &suppress_message) override;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message) override;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	virtual ReturnValue
	OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
			     CefRefPtr<CefFrame> frame,
			     CefRefPtr<CefRequest> request,
			     CefRefPtr<CefCallback> callback) override;

	QCefWidgetInternal *widget = nullptr;
	std::string script;
	bool allowAllPopups;
	//PRISM/Zhangdewen/20230308/#/libbrowser
	std::map<std::string, std::string> headers;
	std::string css;

	IMPLEMENT_REFCOUNTING(QCefBrowserClient);
};

//PRISM/Renjinbo/20230516//navershopping should popup window - add class PLSCefBrowserPopupDialog
class PLSCefBrowserPopupDialog : public QDialog {
	Q_OBJECT
public:
	PLSCefBrowserPopupDialog(QCefWidgetInternal *parent,
				 const std::string &_url);
	~PLSCefBrowserPopupDialog() override;

public:
	QCefWidgetInternal *internal() { return m_internal; }

protected:
	void closeEvent(QCloseEvent *) override;

private:
	QCefWidgetInternal *m_internal = nullptr;
};

//PRISM/Renjinbo/20230516//navershopping should popup windowadd - add class PLSPopupClientHandlerStd
class PLSPopupClientHandlerStd : public QCefBrowserClient {
public:
	inline PLSPopupClientHandlerStd(
		PLSCefBrowserPopupDialog *dialog, const std::string &script_,
		bool allowAllPopups_,
		const std::map<std::string, std::string> &headers_,
		const std::string &css_)
		: QCefBrowserClient(dialog->internal(), script_,
				    allowAllPopups_, headers_, css_),
		  m_dialog(dialog)
	{
	}

private:
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	PLSCefBrowserPopupDialog *m_dialog;
};
