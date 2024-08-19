#pragma once

#include "cef-headers.hpp"
#include "browser-panel-internal.hpp"

#include <string>

class QCefBrowserClient : public CefClient,
			  public CefDisplayHandler,
			  public CefRequestHandler,
			  public CefLifeSpanHandler,
			  public CefContextMenuHandler,
			  public CefLoadHandler,
			  public CefKeyboardHandler,
			  public CefFocusHandler,
			  public CefJSDialogHandler,
			  //PRISM/Zhangdewen/20230117/#/libbrowser
			  public CefResourceRequestHandler {

public:
	//PRISM/Zhangdewen/20230117/#/libbrowser
	inline QCefBrowserClient(QCefWidgetImpl *impl)
		: QCefBrowserClient(impl, impl)
	{
	}
	//PRISM/Zhangdewen/20230117/#/libbrowser
	inline QCefBrowserClient(QCefWidgetImpl *impl,
				 QCefWidgetImpl *checkImpl)
		: m_impl(impl), m_checkImpl(checkImpl)
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
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request, bool is_navigation,
		bool is_download, const CefString &request_initiator,
		bool &disable_default_handling) override
	{
		return this;
	}

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
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 TransitionType transition_type) override;
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
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message) override;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual void
	OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
				  TerminationStatus status) override;
	//PRISM/Zhangdewen/20230117/#/libbrowser
	virtual ReturnValue
	OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
			     CefRefPtr<CefFrame> frame,
			     CefRefPtr<CefRequest> request,
			     CefRefPtr<CefCallback> callback) override;

	//PRISM/Renjinbo/20231124/#2094 crash. cef shutdown must wait for all browsers to close, otherwise it will crash.
	void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	//PRISM/Zhangdewen/20230117/#/libbrowser
	QCefWidgetImpl *m_impl = nullptr;
	QCefWidgetImpl *m_checkImpl = nullptr;

	IMPLEMENT_REFCOUNTING(QCefBrowserClient);
};

//PRISM/Zhangdewen/20230117/#/libbrowser
class QCefBrowserPopupClient : public QCefBrowserClient {
	friend class QCefBrowserClient;
	friend class QCefWidgetImpl;
	friend class QCefWidgetInternal;

private:
	inline QCefBrowserPopupClient(QCefBrowserPopupDialog *dialog)
		: QCefBrowserClient(dialog->impl(), dialog->checkImpl())
	{
	}

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
};
