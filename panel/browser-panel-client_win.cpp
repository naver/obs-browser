#include "browser-panel-client.hpp"
#include <util/dstr.h>

#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QMenu>
#include <QThread>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <QLabel>

#include <obs-module.h>
#ifdef _WIN32
#include <windows.h>
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif

#include "pls-frontend-api.h"

#define MENU_ITEM_DEVTOOLS MENU_ID_CUSTOM_FIRST
#define MENU_ITEM_MUTE MENU_ID_CUSTOM_FIRST + 1

/* CefClient */
CefRefPtr<CefLoadHandler> QCefBrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> QCefBrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefRequestHandler> QCefBrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> QCefBrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefFocusHandler> QCefBrowserClient::GetFocusHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> QCefBrowserClient::GetContextMenuHandler()
{
	return this;
}

CefRefPtr<CefKeyboardHandler> QCefBrowserClient::GetKeyboardHandler()
{
	return this;
}

CefRefPtr<CefJSDialogHandler> QCefBrowserClient::GetJSDialogHandler()
{
	return this;
}

/* CefDisplayHandler */
void QCefBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
				      const CefString &title)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	bool processed = false;
	cef_widgets_sync_call(m_impl, [this, &processed, browser,
				       &title](QCefWidgetImpl *impl) {
		if (impl->m_browser && impl->m_browser->IsSame(browser)) {
			QString qt_title = QString::fromStdString(title);
			QMetaObject::invokeMethod(impl, "titleChanged",
						  Q_ARG(QString, qt_title));
			processed = true;
		}
	});
	if (!processed) {
		if (title.compare("DevTools") == 0)
			return;

		CefWindowHandle handle = browser->GetHost()->GetWindowHandle();

#ifdef Q_OS_WIN
		SetWindowTextW(handle, title.ToWString().c_str());
#endif
	}
}

/* CefRequestHandler */
bool QCefBrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				       CefRefPtr<CefFrame>,
				       CefRefPtr<CefRequest> request, bool,
				       bool)
{
	std::string str_url = request->GetURL();

	//PRISM/Zhangdewen/20230117/#/libbrowser
	cef_widgets_sync_call(m_checkImpl, [str_url](QCefWidgetImpl *impl) {
		QString qt_url = QString::fromStdString(str_url);
		QMetaObject::invokeMethod(impl, "urlChanged",
					  Q_ARG(QString, qt_url));
	});
	return false;
}

bool QCefBrowserClient::OnOpenURLFromTab(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	CefRequestHandler::WindowOpenDisposition, bool)
{
	std::string str_url = target_url;

	/* Open tab popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefBrowserClient::OnLoadError(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame> frame,
				    CefLoadHandler::ErrorCode errorCode,
				    const CefString &errorText,
				    const CefString &failedUrl)
{
	UNUSED_PARAMETER(browser);
	if (errorCode == ERR_ABORTED)
		return;

	struct dstr html;
	char *path = obs_module_file("error.html");
	char *errorPage = os_quick_read_utf8_file(path);

	dstr_init_copy(&html, errorPage);

	dstr_replace(&html, "%%ERROR_URL%%", failedUrl.ToString().c_str());

	dstr_replace(&html, "Error.Title", obs_module_text("Error.Title"));
	dstr_replace(&html, "Error.Description",
		     obs_module_text("Error.Description"));
	dstr_replace(&html, "Error.Retry",
		     obs_module_text("Error.Retry.Prism"));
	const char *translError;
	std::string errorKey = "ErrorCode." + errorText.ToString();
	if (obs_module_get_string(errorKey.c_str(),
				  (const char **)&translError)) {
		dstr_replace(&html, "%%ERROR_CODE%%", translError);
	} else {
		dstr_replace(&html, "%%ERROR_CODE%%",
			     errorText.ToString().c_str());
	}
	auto isBrowerDock = [](QWidget *widget) {
		QWidget *tmp = widget;
		while (tmp) {
			if (tmp->objectName() == "browserDock") {
				return true;
			}
			tmp = tmp->parentWidget();
		}
		return false;
	};

	if (isBrowerDock(m_impl)) {
		dstr_replace(&html, "%%BG_COLOR%%", "#272727");
	} else {
		dstr_replace(&html, "%%BG_COLOR%%", "#1c1c1d");
	}

	frame->LoadURL(
		"data:text/html;base64," +
		CefURIEncode(CefBase64Encode(html.array, html.len), false)
			.ToString());

	dstr_free(&html);
	bfree(path);
	bfree(errorPage);
}

/* CefLifeSpanHandler */
bool QCefBrowserClient::OnBeforePopup(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	const CefString &, CefLifeSpanHandler::WindowOpenDisposition, bool,
	const CefPopupFeatures &, CefWindowInfo &windowInfo,
	CefRefPtr<CefClient> &browserClient, CefBrowserSettings &,
	CefRefPtr<CefDictionaryValue> &, bool *)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	std::string str_url = target_url;

	bool processed = false;
	cef_widgets_sync_call(m_impl, [&processed, &browserClient, &windowInfo,
				       this](QCefWidgetImpl *impl) {
		if (!impl->m_allowPopups) {
			return;
		}

		QWidget *parent = impl;

		CefWindowHandle handle = nullptr;
		QCefBrowserPopupDialog *dialog = QCefBrowserPopupDialog::create(
			handle, m_checkImpl, parent, true);
		CefRefPtr<QCefBrowserPopupClient> browserPopupClient =
			new QCefBrowserPopupClient(dialog);
		browserClient = browserPopupClient;
		impl->m_popups.append(browserPopupClient);
		windowInfo.SetAsChild(handle, CefRect(0, 0, dialog->width(),
						      dialog->height()));

		processed = true;
	});
	if (processed) {
		return false;
	}

	/* Open popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

bool QCefBrowserClient::OnSetFocus(CefRefPtr<CefBrowser>,
				   CefFocusHandler::FocusSource source)
{
	/* Don't steal focus when the webpage navigates. This is especially
	   obvious on startup when the user has many browser docks defined,
	   as each one will steal focus one by one, resulting in poor UX.
	 */
	switch (source) {
	case FOCUS_SOURCE_NAVIGATION:
		return true;
	default:
		return false;
	}
}

void QCefBrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
					    CefRefPtr<CefFrame>,
					    CefRefPtr<CefContextMenuParams>,
					    CefRefPtr<CefMenuModel> model)
{
	if (model->IsVisible(MENU_ID_BACK) &&
	    (!model->IsVisible(MENU_ID_RELOAD) &&
	     !model->IsVisible(MENU_ID_RELOAD_NOCACHE))) {
		model->InsertItemAt(
			2, MENU_ID_RELOAD_NOCACHE,
			QObject::tr("RefreshBrowser").toUtf8().constData());
	}
	if (model->IsVisible(MENU_ID_PRINT)) {
		model->Remove(MENU_ID_PRINT);
	}
	if (model->IsVisible(MENU_ID_VIEW_SOURCE)) {
		model->Remove(MENU_ID_VIEW_SOURCE);
	}
	model->AddSeparator();
	model->InsertItemAt(model->GetCount(), MENU_ITEM_DEVTOOLS,
			    obs_module_text("Inspect"));
	model->InsertCheckItemAt(model->GetCount(), MENU_ITEM_MUTE,
				 QObject::tr("Mute").toUtf8().constData());
	model->SetChecked(MENU_ITEM_MUTE, browser->GetHost()->IsAudioMuted());
}

#if defined(_WIN32)
bool QCefBrowserClient::RunContextMenu(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
	CefRefPtr<CefContextMenuParams>, CefRefPtr<CefMenuModel> model,
	CefRefPtr<CefRunContextMenuCallback> callback)
{
	std::vector<std::tuple<std::string, int, bool, int, bool>> menu_items;
	menu_items.reserve(model->GetCount());
	for (int i = 0; i < model->GetCount(); i++) {
		menu_items.push_back(
			{model->GetLabelAt(i), model->GetCommandIdAt(i),
			 model->IsEnabledAt(i), model->GetTypeAt(i),
			 model->IsCheckedAt(i)});
	}

	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(),
		[menu_items, callback]() {
			QMenu contextMenu;
			std::string name;
			int command_id;
			bool enabled;
			int type_id;
			bool check;

			for (const std::tuple<std::string, int, bool, int, bool>
				     &menu_item : menu_items) {
				std::tie(name, command_id, enabled, type_id,
					 check) = menu_item;
				switch (type_id) {
				case MENUITEMTYPE_CHECK:
				case MENUITEMTYPE_COMMAND: {
					QAction *item =
						new QAction(name.c_str());
					item->setEnabled(enabled);
					if (type_id == MENUITEMTYPE_CHECK) {
						item->setCheckable(true);
						item->setChecked(check);
					}
					item->setProperty("cmd_id", command_id);
					contextMenu.addAction(item);
				} break;
				case MENUITEMTYPE_SEPARATOR:
					contextMenu.addSeparator();
					break;
				}
			}

			QAction *action = contextMenu.exec(QCursor::pos());
			if (action) {
				QVariant cmdId = action->property("cmd_id");
				callback.get()->Continue(cmdId.toInt(),
							 EVENTFLAG_NONE);
			} else {
				callback.get()->Cancel();
			}
		});
	return true;
}
#endif

bool QCefBrowserClient::OnContextMenuCommand(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>,
	CefRefPtr<CefContextMenuParams> params, int command_id,
	CefContextMenuHandler::EventFlags)
{
	if (command_id < MENU_ID_CUSTOM_FIRST)
		return false;
	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	CefWindowInfo windowInfo;
	QPoint pos;
	QString title;
	switch (command_id) {
	case MENU_ITEM_DEVTOOLS:
#if defined(_WIN32)
		title = QString(obs_module_text("DevTools"))
				.arg("" /*widget->parentWidget()->windowTitle()*/);
		windowInfo.SetAsPopup(host->GetWindowHandle(),
				      title.toUtf8().constData());
#endif
		pos = QCursor::pos(); //widget->mapToGlobal(QPoint(0, 0));
		windowInfo.bounds.x = pos.x();
		windowInfo.bounds.y = pos.y() + 30;
		windowInfo.bounds.width = 900;
		windowInfo.bounds.height = 700;
		host->ShowDevTools(
			windowInfo, host->GetClient(), CefBrowserSettings(),
			{params.get()->GetXCoord(), params.get()->GetYCoord()});
		return true;
	case MENU_ITEM_MUTE:
		host->SetAudioMuted(!host->IsAudioMuted());
		return true;
	}
	return false;
}

void QCefBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
				  CefRefPtr<CefFrame> frame, int)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	if (frame->IsMain()) {
		std::string script;
		std::string css;
		cef_widgets_sync_call(
			m_impl, [&script, &css](const QCefWidgetImpl *impl) {
				script = impl->m_script;
				css = impl->m_css;
			});

		if (!script.empty()) {
			frame->ExecuteJavaScript(script, CefString(), 0);
		}
		if (!css.empty()) {
			std::string encCss =
				CefURIEncode(css, false).ToString();

			std::string scriptCss;
			scriptCss +=
				"const initCSS = document.createElement('style');";
			scriptCss +=
				"initCSS.innerHTML = decodeURIComponent(\"" +
				encCss + "\");";
			scriptCss +=
				"document.querySelector('head').appendChild(initCSS);";

			frame->ExecuteJavaScript(scriptCss, CefString(), 0);
		}
	}

	//PRISM/Zhangdewen/20230117/#/libbrowser
	cef_widgets_sync_call(m_impl, [browser](QCefWidgetImpl *impl) {
		if (impl->m_browser && browser->IsSame(impl->m_browser)) {
			QMetaObject::invokeMethod(impl, "loadEnded");
		}
	});
}

bool QCefBrowserClient::OnJSDialog(CefRefPtr<CefBrowser>, const CefString &,
				   CefJSDialogHandler::JSDialogType dialog_type,
				   const CefString &message_text,
				   const CefString &default_prompt_text,
				   CefRefPtr<CefJSDialogCallback> callback,
				   bool &)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	QString parentTitle; //widget->parentWidget()->windowTitle();
	std::string default_value = default_prompt_text;
	QString msg(message_text.ToString().c_str());
	// Replace <br> with standard newline as we will render in plaintext
	msg.replace(QRegularExpression("<br\\s{0,1}\\/{0,1}>"), "\n");

	if (dialog_type == JSDIALOGTYPE_PROMPT) {
		auto msgbox = [msg, default_value, callback]() {
			QInputDialog *dlg = new QInputDialog(nullptr);
			dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
			dlg->setWindowFlag(Qt::WindowContextHelpButtonHint,
					   false);
			std::stringstream title;
			title << obs_module_text("Dialog.Prompt") << ": "
			      << obs_module_text("Dialog.BrowserDock");
			dlg->setWindowTitle(title.str().c_str());
			if (!default_value.empty())
				dlg->setTextValue(default_value.c_str());

			auto finished = [callback, dlg](int result) {
				callback.get()->Continue(
					result == QDialog::Accepted,
					dlg->textValue().toUtf8().constData());
			};

			QWidget::connect(dlg, &QInputDialog::finished,
					 finished);
			dlg->open();
			if (QLabel *lbl = dlg->findChild<QLabel *>()) {
				// Force plaintext manually
				lbl->setTextFormat(Qt::PlainText);
			}
			dlg->setLabelText(msg);
		};
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(), msgbox);
		return true;
	}
	auto msgbox = [msg, dialog_type, callback]() {
		QMessageBox *dlg = new QMessageBox(nullptr);
		dlg->setStandardButtons(QMessageBox::Ok);
		dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
		dlg->setTextFormat(Qt::PlainText);
		dlg->setText(msg);
		//PRISM/Aiguanghua/20230530/#1035/Navershopping login failed
		dlg->setStyleSheet(
			"QLabel { color:black; font-size:12px;  } QPushButton{ background-color: #444444; border-radius:3px; min-height:25px; max-height:25px; min-width:58px; max-width:58px;font-size:14px; font-weight: bold; color: #ffffff;} ");
		std::stringstream title;
		switch (dialog_type) {
		case JSDIALOGTYPE_CONFIRM:
			title << obs_module_text("Alert.Title");
			dlg->setIcon(QMessageBox::Question);
			dlg->addButton(QMessageBox::Cancel);
			break;
		case JSDIALOGTYPE_ALERT:
		default:
			title << obs_module_text("Alert.Title");
			dlg->setIcon(QMessageBox::Information);
			break;
		}
		//title << ": " << obs_module_text("Dialog.BrowserDock");
		dlg->setWindowTitle(title.str().c_str());

		auto finished = [callback](int result) {
			callback.get()->Continue(result == QMessageBox::Ok, "");
		};

		QWidget::connect(dlg, &QMessageBox::finished, finished);

		dlg->open();
	};
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  msgbox);
	return true;
#else
	UNUSED_PARAMETER(dialog_type);
	UNUSED_PARAMETER(message_text);
	UNUSED_PARAMETER(default_prompt_text);
	UNUSED_PARAMETER(callback);
	return false;
#endif
}

bool QCefBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				      const CefKeyEvent &event, CefEventHandle,
				      bool *)
{
	if (event.type != KEYEVENT_RAWKEYDOWN)
		return false;

	if (event.windows_key_code == 'R' &&
#ifdef __APPLE__
	    (event.modifiers & EVENTFLAG_COMMAND_DOWN) != 0) {
#else
	    (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
#endif
		browser->ReloadIgnoreCache();
		return true;
	}
	return false;
}

//PRISM/Zhangdewen/20230117/#/libbrowser
bool QCefBrowserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId,
	CefRefPtr<CefProcessMessage> message)
{
	std::string result("{}");
	std::string name = message->GetName();
	std::string param = message->GetArgumentList()->GetString(0);

	if ("sendToCpp" == name && message->GetArgumentList()->GetSize() == 2) {
		QString type = QString::fromStdWString(
			message->GetArgumentList()->GetString(0));
		QString msg = QString::fromStdWString(
			message->GetArgumentList()->GetString(1));
		cef_widgets_sync_call(m_checkImpl, [type,
						    msg](QCefWidgetImpl *impl) {
			QMetaObject::invokeMethod(impl, "msgRecevied",
						  Q_ARG(QString, type),
						  Q_ARG(QString, msg));
		});
		return true;
	} else if ("sendToPrism" == name) {
		pls_frontend_call_web_invoked_cb(param.c_str());
	} else {
		return false;
	}
	return true;
}

//PRISM/Zhangdewen/20230117/#/libbrowser: exception restart
void QCefBrowserClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
						  TerminationStatus status)
{
	cef_widgets_sync_call(m_impl, [this, browser](QCefWidgetImpl *impl) {
		if (impl->m_browser && browser->IsSame(impl->m_browser)) {
			impl->destroy(true);
		}
	});
}

//PRISM/Zhangdewen/20230117/#/libbrowser: modify request headers
QCefBrowserClient::ReturnValue QCefBrowserClient::OnBeforeResourceLoad(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback)
{
	QCefWidgetImpl::Headers headers;
	cef_widgets_sync_call(m_impl, [&headers](QCefWidgetImpl *impl) {
		headers = impl->m_headers;
	});
	for (const auto &header : headers) {
		request->SetHeaderByName(header.first, header.second, true);
	}
	return RV_CONTINUE;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefBrowserPopupClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	cef_widgets_sync_call(m_impl, [browser](QCefWidgetImpl *impl) {
		impl->attachBrowser(browser);
	});

	CefLifeSpanHandler::OnAfterCreated(browser);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
bool QCefBrowserPopupClient::DoClose(CefRefPtr<CefBrowser> browser)
{
	cef_widgets_sync_call(m_impl, [this](QCefWidgetImpl *impl) {
		impl->detachBrowser();
		m_impl = nullptr;
	});

	return CefLifeSpanHandler::DoClose(browser);
}
