/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/dstr.hpp>
#include <obs-module.h>
#include <obs.hpp>
#include <functional>
#include <sstream>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <obs-websocket-api.h>

#include "obs-browser-source.hpp"
#include "browser-scheme.hpp"
#include "browser-app.hpp"
#include "browser-version.h"
#include "pls-frontend-api.h"

#include "cef-headers.hpp"
//PRISM/Xiewei/20230202/for web browser interaction
#include <pls/pls-source.h>
#ifdef _WIN32
#include <util/windows/ComPtr.hpp>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#else
#include "signal-restore.hpp"
#endif

#ifdef ENABLE_WAYLAND
#include <obs-nix-platform.h>
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QApplication>
#include <QThread>
#endif
#include <QApplication>

//PRISM/Chengbing/20231108/#/prism version
#include <pls/pls-obs-api.h>
//PRISM/Renjinbo/20240620/#/add debug port to debug alll cef client
#include <QSettings>
#include <filesystem>
#include "pls/pls-base.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "CEF-based web browser source & panels";
}

using namespace std;

static thread manager_thread;
static bool manager_initialized = false;
os_event_t *cef_started_event = nullptr;
//PRISM/Zhangdewen/20240122/PRISM_PC-2212 can't exit.
static bool manager_destroyed = false;

#if defined(_WIN32)
static int adapterCount = 0;
#endif
static std::wstring deviceId;

bool hwaccel = false;

std::atomic<bool> is_all_browers_close_done = true;

/* ========================================================================= */

#ifdef ENABLE_BROWSER_QT_LOOP
extern MessageObject messageObject;
#endif

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override
	{
#ifdef ENABLE_BROWSER_QT_LOOP
		/* you have to put the tasks on the Qt event queue after this
		 * call otherwise the CEF message pump may stop functioning
		 * correctly, it's only supposed to take 10ms max */
		QMetaObject::invokeMethod(&messageObject, "ExecuteTask", Qt::QueuedConnection,
					  Q_ARG(MessageTask, task));
#else
		task();
#endif
	}

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

bool QueueCEFTask(std::function<void()> task)
{
	//PRISM/Zhangdewen/20230117/#/libbrowser
	CefRefPtr<BrowserTask> t(new BrowserTask(task));
	//PRISM/Zhangdewen/20240122/PRISM_PC-2212 can't exit.
	while ((!manager_destroyed) && (!CefPostTask(TID_UI, t)))
		os_sleep_ms(5);
	return true;
}

//PRISM/Renjinbo/20240620/#/add debug port to debug alll cef client
static void addWebRemoteDebugPort(CefSettings &settings)
{
	int webDebugPort = 0;
#ifdef _WIN32
	webDebugPort = QSettings("NAVER Corporation", "Prism Live Studio").value("WebPort", 0).toInt();
#elif defined(__APPLE__)
	webDebugPort = QSettings("prismlive", "prismlivestudio").value("WebPort", 0).toInt();
#endif
	if (webDebugPort > 0) {
		settings.remote_debugging_port = webDebugPort;
	}
}

/* ========================================================================= */

static const char *default_css = "\
body { \
background-color: rgba(0, 0, 0, 0); \
margin: 0px auto; \
overflow: hidden; \
}";

static void browser_source_get_defaults(obs_data_t *settings)
{
	//PRISM/Xiewei/20221226/obs upgrade: cef interaction
	obs_data_set_default_string(settings, "url", obs_module_text("DefaultURL"));
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_int(settings, "webpage_control_level", (int)DEFAULT_CONTROL_LEVEL);
	obs_data_set_default_string(settings, "css", default_css);
	obs_data_set_default_bool(settings, "reroute_audio", false);
}

static bool is_local_file_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "is_local_file");
	obs_property_t *url = obs_properties_get(props, "url");
	obs_property_t *local_file = obs_properties_get(props, "local_file");
	obs_property_set_visible(url, !enabled);
	obs_property_set_visible(local_file, enabled);

	return true;
}

static bool is_fps_custom(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "fps_custom");
	obs_property_t *fps = obs_properties_get(props, "fps");
	obs_property_set_visible(fps, enabled);

	return true;
}

static obs_properties_t *browser_source_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	DStr path;

	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	obs_property_t *prop = obs_properties_add_bool(props, "is_local_file", obs_module_text("LocalFile"));

	if (bs && !bs->url.empty()) {
		const char *slash;

		dstr_copy(path, bs->url.c_str());
		dstr_replace(path, "\\", "/");
		slash = strrchr(path->array, '/');
		if (slash)
			dstr_resize(path, slash - path->array + 1);
	}

	obs_property_set_modified_callback(prop, is_local_file_modified);
	obs_properties_add_path(props, "local_file", obs_module_text("LocalFile"), OBS_PATH_FILE, "*.*", path->array);
	obs_properties_add_text(props, "url", obs_module_text("URL"), OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1, 8192, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1, 8192, 1);

	obs_properties_add_bool(props, "reroute_audio", obs_module_text("RerouteAudio"));

	obs_property_t *fps_set = obs_properties_add_bool(props, "fps_custom", obs_module_text("CustomFrameRate"));
	obs_property_set_modified_callback(fps_set, is_fps_custom);

#ifndef ENABLE_BROWSER_SHARED_TEXTURE
	obs_property_set_enabled(fps_set, false);
#endif

	obs_properties_add_int(props, "fps", obs_module_text("FPS"), 1, 60, 1);

	obs_property_t *p = obs_properties_add_text(props, "css", obs_module_text("CSS"), OBS_TEXT_MULTILINE);
	obs_property_text_set_monospace(p, true);
	obs_properties_add_bool(props, "shutdown", obs_module_text("ShutdownSourceNotVisible"));
	obs_properties_add_bool(props, "restart_when_active", obs_module_text("RefreshBrowserActive"));

	obs_property_t *controlLevel = obs_properties_add_list(props, "webpage_control_level",
							       obs_module_text("WebpageControlLevel"),
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.None"),
				  (int)ControlLevel::None);
	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.ReadObs"),
				  (int)ControlLevel::ReadObs);
	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.ReadUser"),
				  (int)ControlLevel::ReadUser);
	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.Basic"),
				  (int)ControlLevel::Basic);
	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.Advanced"),
				  (int)ControlLevel::Advanced);
	obs_property_list_add_int(controlLevel, obs_module_text("WebpageControlLevel.Level.All"),
				  (int)ControlLevel::All);

	obs_properties_add_button(props, "refreshnocache", obs_module_text("RefreshNoCache"),
				  [](obs_properties_t *, obs_property_t *, void *data) {
					  static_cast<BrowserSource *>(data)->Refresh();
					  return false;
				  });
	return props;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(src);

	if (bs) {
		obs_source_t *source = bs->source;
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_set_string(settings, "local_file", new_path);
		obs_source_update(source, settings);
	}

	UNUSED_PARAMETER(data);
}

static obs_missing_files_t *browser_source_missingfiles(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	obs_missing_files_t *files = obs_missing_files_create();

	if (bs) {
		obs_source_t *source = bs->source;
		OBSDataAutoRelease settings = obs_source_get_settings(source);

		bool enabled = obs_data_get_bool(settings, "is_local_file");
		const char *path = obs_data_get_string(settings, "local_file");

		if (enabled && strcmp(path, "") != 0) {
			if (!os_file_exists(path)) {
				obs_missing_file_t *file = obs_missing_file_create(
					path, missing_file_callback, OBS_MISSING_FILE_SOURCE, bs->source, NULL);

				obs_missing_files_add_file(files, file);
			}
		}
	}

	return files;
}

static CefRefPtr<BrowserApp> app;
static std::wstring getCefLogFile();
static void BrowserInit(void)
{
	string path = obs_get_module_binary_path(obs_current_module());
	path = path.substr(0, path.find_last_of('/') + 1);
	path += "//obs-browser-page";
#ifdef _WIN32
	path += ".exe";
	CefMainArgs args;
#else
	/* On non-windows platforms, ie macOS, we'll want to pass thru flags to
	 * CEF */
	struct obs_cmdline_args cmdline_args = obs_get_cmdline_args();
	CefMainArgs args(cmdline_args.argc, cmdline_args.argv);
#endif

	BPtr<char> conf_path = obs_module_config_path("");
	os_mkdir(conf_path);

	CefSettings settings;

	//PRISM/Renjinbo/20250312/#/output cef raw log to file and upload to nelo
	settings.log_severity = LOGSEVERITY_WARNING;
	auto logFile = getCefLogFile();
	CefString(&settings.log_file) = logFile;
	char *logFile_utf8 = NULL;
	os_wcs_to_utf8_ptr(logFile.c_str(), 0, &logFile_utf8);
	if (logFile_utf8 != NULL) {
		base_add_global_field_cn("cefLogFile", logFile_utf8);
		bfree(logFile_utf8);
	}

	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true;

	uint32_t obs_ver = obs_get_version();
	uint32_t obs_maj = obs_ver >> 24;
	uint32_t obs_min = (obs_ver >> 16) & 0xFF;
	uint32_t obs_pat = obs_ver & 0xFFFF;

	//PRISM/Renjinbo/20240620/#/add debug port to debug alll cef client
	addWebRemoteDebugPort(settings);

	/* This allows servers the ability to determine that browser panels and
	 * browser sources are coming from OBS. */
	std::stringstream prod_ver;
	prod_ver << "Chrome/";
	prod_ver << std::to_string(cef_version_info(4)) << "." << std::to_string(cef_version_info(5)) << "."
		 << std::to_string(cef_version_info(6)) << "." << std::to_string(cef_version_info(7));
	prod_ver << " OBS/";
	prod_ver << std::to_string(obs_maj) << "." << std::to_string(obs_min) << "." << std::to_string(obs_pat);

	//PRISM/Zhangdewen/20230117/#/modify user agent
	//PRISM/Chengbing/20231108/#/prism version
	prod_ver << " NAVER(pc; prism; prism-pc; " << pls_prism_version_major() << "." << pls_prism_version_minor()
		 << "." << pls_prism_version_patch() << ";)";

#if CHROME_VERSION_BUILD >= 4472
	CefString(&settings.user_agent_product) = prod_ver.str();
#else
	CefString(&settings.product_version) = prod_ver.str();
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
	settings.external_message_pump = true;
	settings.multi_threaded_message_loop = false;
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
	// Override locale path from OBS binary path to plugin binary path
	string locales = obs_get_module_binary_path(obs_current_module());
	locales = locales.substr(0, locales.find_last_of('/') + 1);
	locales += "locales";
	BPtr<char> abs_locales = os_get_abs_path_ptr(locales.c_str());
	CefString(&settings.locales_dir_path) = abs_locales;
#endif

	std::string obs_locale = obs_get_locale();
	std::string accepted_languages;
	if (obs_locale != "en-US") {
		accepted_languages = obs_locale;
		accepted_languages += ",";
		accepted_languages += "en-US,en";
	} else {
		//PRISM/Zhangdewen/20230117/#/modify accept languange
		accepted_languages = "en-US,en,ko-KR,ko";
	}
	//PRISM/Renjinbo/20230124/#2103/right menu local
	CefString(&settings.locale) = obs_locale.substr(0, obs_locale.find_last_of('-'));

	BPtr<char> conf_path_abs = os_get_abs_path_ptr(conf_path);
	CefString(&settings.accept_language_list) = accepted_languages;
#if CHROME_VERSION_BUILD <= 6533
	settings.persist_user_preferences = 1;
#endif
	CefString(&settings.cache_path) = conf_path_abs;
#if !defined(__APPLE__) || defined(ENABLE_BROWSER_LEGACY)
	char *abs_path = os_get_abs_path_ptr(path.c_str());
	CefString(&settings.browser_subprocess_path) = abs_path;
	bfree(abs_path);
#endif

	bool tex_sharing_avail = false;

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	if (hwaccel) {
		obs_enter_graphics();
		hwaccel = tex_sharing_avail = gs_shared_texture_available();
		obs_leave_graphics();
	}
#endif

#if defined(__APPLE__) || defined(_WIN32) || !defined(ENABLE_WAYLAND)
	app = new BrowserApp(tex_sharing_avail);
#else
	app = new BrowserApp(tex_sharing_avail, obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND);
#endif

#ifdef _WIN32
	CefExecuteProcess(args, app, nullptr);
#endif

#if !defined(_WIN32)
	BackupSignalHandlers();
	bool success = CefInitialize(args, settings, app, nullptr);
	RestoreSignalHandlers();
#elif (CHROME_VERSION_BUILD > 3770)
	bool success = CefInitialize(args, settings, app, nullptr);
#else
	/* Massive (but amazing) hack to prevent chromium from modifying our
	 * process tokens and permissions, which caused us problems with winrt,
	 * used with window capture.  Note, the structure internally is just
	 * two pointers normally.  If it causes problems with future versions
	 * we'll just switch back to the static library but I doubt we'll need
	 * to. */
	uintptr_t zeroed_memory_lol[32] = {};
	bool success = CefInitialize(args, settings, app, zeroed_memory_lol);
#endif

	if (!success) {
		blog(LOG_ERROR, "[obs-browser]: CEF failed to initialize. Exit code: %d", CefGetExitCode());
		return;
	}

#if !ENABLE_LOCAL_FILE_URL_SCHEME
	/* Register http://absolute/ scheme handler for older
	 * CEF builds which do not support file:// URLs */
	CefRegisterSchemeHandlerFactory("http", "absolute", new BrowserSchemeHandlerFactory());
#endif
	os_event_signal(cef_started_event);
}

static void BrowserShutdown(void)
{
#if !ENABLE_LOCAL_FILE_URL_SCHEME
	CefClearSchemeHandlerFactories();
#endif
#ifdef ENABLE_BROWSER_QT_LOOP
	while (messageObject.ExecuteNextBrowserTask())
		;
	CefDoMessageLoopWork();
#endif
	CefShutdown();
	app = nullptr;
}

#ifndef ENABLE_BROWSER_QT_LOOP
static void BrowserManagerThread(void)
{
	BrowserInit();
	CefRunMessageLoop();
	//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
	blog(LOG_INFO, "[obs-browser]: browser manager thread exit runloop");
	BrowserShutdown();
	//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
	blog(LOG_INFO, "[obs-browser]: browser manager thread shutdown complete");
}
#endif

extern "C" EXPORT void obs_browser_initialize(void)
{
	if (!os_atomic_set_bool(&manager_initialized, true)) {
		//PRISM/Renjinbo/20241122/#PRISM_PC_NELO-82/add log
		blog(LOG_INFO, "try to init cef browser with create loop thread.");
#ifdef ENABLE_BROWSER_QT_LOOP
		BrowserInit();
#else
		manager_thread = thread(BrowserManagerThread);
#endif
	}
}

void RegisterBrowserSource()
{
	struct obs_source_info info = {};
	info.id = "browser_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
			    OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	info.get_properties = browser_source_get_properties;
	info.get_defaults = browser_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void *) {
		return obs_module_text("BrowserSource");
	};
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		//PRISM/Renjinbo/20241122/#PRISM_PC_NELO-82/add log
		blog(LOG_INFO, "try to init cef browser from source create.");
		obs_browser_initialize();
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.missing_files = browser_source_missingfiles;
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->Update(settings);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->height;
	};
	info.video_tick = [](void *data, float) {
		static_cast<BrowserSource *>(data)->Tick();
	};
	info.video_render = [](void *data, gs_effect_t *) {
		static_cast<BrowserSource *>(data)->Render();
	};
#if CHROME_VERSION_BUILD < 4103
	info.audio_mix = [](void *data, uint64_t *ts_out, struct audio_output_data *audio_output, size_t channels,
			    size_t sample_rate) {
		return static_cast<BrowserSource *>(data)->AudioMix(ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void *data, obs_source_enum_proc_t cb, void *param) {
		static_cast<BrowserSource *>(data)->EnumAudioStreams(cb, param);
	};
#endif
	info.mouse_click = [](void *data, const struct obs_mouse_event *event, int32_t type, bool mouse_up,
			      uint32_t click_count) {
		static_cast<BrowserSource *>(data)->SendMouseClick(event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void *data, const struct obs_mouse_event *event, bool mouse_leave) {
		static_cast<BrowserSource *>(data)->SendMouseMove(event, mouse_leave);
	};
	info.mouse_wheel = [](void *data, const struct obs_mouse_event *event, int x_delta, int y_delta) {
		static_cast<BrowserSource *>(data)->SendMouseWheel(event, x_delta, y_delta);
	};
	info.focus = [](void *data, bool focus) {
		static_cast<BrowserSource *>(data)->SendFocus(focus);
	};
	info.key_click = [](void *data, const struct obs_key_event *event, bool key_up) {
		static_cast<BrowserSource *>(data)->SendKeyClick(event, key_up);
	};
	info.show = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(true);
	};
	info.hide = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(false);
	};
	info.activate = [](void *data) {
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [](void *data) {
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	//PRISM/Xiewei/20230202/for web browser interaction
	struct pls_source_info pls_info = {};
	pls_info.get_private_data = InteractionDelegate::GetBrowserData;
	pls_info.set_private_data = InteractionDelegate::SetBrowserData;
	//PRISM/Zhangdewen/20230202/#/move from //PRISM/RenJinbo/20210603/#none/timer source feature
	pls_info.cef_dispatch_js = [](void *data, const char *event_name, const char *json_data) {
		if (data && event_name && *event_name && json_data && *json_data) {
			static_cast<BrowserSource *>(data)->dispatchJs(event_name, json_data);
		}
	};

	register_pls_source_info(&info, &pls_info);

	obs_register_source(&info);
}

/* ========================================================================= */

extern void DispatchJSEvent(std::string eventName, std::string jsonString, BrowserSource *browser = nullptr);

static void handle_obs_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		DispatchJSEvent("obsStreamingStarting", "null");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		DispatchJSEvent("obsStreamingStarted", "null");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		DispatchJSEvent("obsStreamingStopping", "null");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		DispatchJSEvent("obsStreamingStopped", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		DispatchJSEvent("obsRecordingStarting", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		DispatchJSEvent("obsRecordingStarted", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		DispatchJSEvent("obsRecordingPaused", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		DispatchJSEvent("obsRecordingUnpaused", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		DispatchJSEvent("obsRecordingStopping", "null");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		DispatchJSEvent("obsRecordingStopped", "null");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		DispatchJSEvent("obsReplaybufferStarting", "null");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		DispatchJSEvent("obsReplaybufferStarted", "null");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		DispatchJSEvent("obsReplaybufferSaved", "null");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		DispatchJSEvent("obsReplaybufferStopping", "null");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		DispatchJSEvent("obsReplaybufferStopped", "null");
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
		DispatchJSEvent("obsVirtualcamStarted", "null");
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		DispatchJSEvent("obsVirtualcamStopped", "null");
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		OBSSourceAutoRelease source = obs_frontend_get_current_scene();

		if (!source)
			break;

		const char *name = obs_source_get_name(source);
		if (!name)
			break;

		nlohmann::json json = {{"name", name},
				       {"width", obs_source_get_width(source)},
				       {"height", obs_source_get_height(source)}};

		DispatchJSEvent("obsSceneChanged", json.dump());
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED: {
		struct obs_frontend_source_list list = {};
		obs_frontend_get_scenes(&list);
		std::vector<const char *> scenes_vector;
		for (size_t i = 0; i < list.sources.num; i++) {
			obs_source_t *source = list.sources.array[i];
			scenes_vector.push_back(obs_source_get_name(source));
		}
		nlohmann::json json = scenes_vector;
		obs_frontend_source_list_free(&list);

		DispatchJSEvent("obsSceneListChanged", json.dump());
		break;
	}
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED: {
		OBSSourceAutoRelease source = obs_frontend_get_current_transition();

		if (!source)
			break;

		const char *name = obs_source_get_name(source);
		if (!name)
			break;

		nlohmann::json json = {{"name", name}};

		DispatchJSEvent("obsTransitionChanged", json.dump());
		break;
	}
	case OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED: {
		struct obs_frontend_source_list list = {};
		obs_frontend_get_transitions(&list);
		std::vector<const char *> transitions_vector;
		for (size_t i = 0; i < list.sources.num; i++) {
			obs_source_t *source = list.sources.array[i];
			transitions_vector.push_back(obs_source_get_name(source));
		}
		nlohmann::json json = transitions_vector;
		obs_frontend_source_list_free(&list);

		DispatchJSEvent("obsTransitionListChanged", json.dump());
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		DispatchJSEvent("obsExit", "null");
		break;
	default:;
	}
}

#ifdef _WIN32
static inline void EnumAdapterCount()
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i = 0;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory);
	if (FAILED(hr))
		return;

	while (factory->EnumAdapters1(i++, &adapter) == S_OK) {
		DXGI_ADAPTER_DESC desc;

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		if (i == 1)
			deviceId = desc.Description;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		adapterCount++;
	}
}
#endif

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
#ifdef _WIN32
static const wchar_t *blacklisted_devices[] = {L"Intel", L"Microsoft", L"Radeon HD 8850M", L"Radeon HD 7660", nullptr};

static inline bool is_intel(const std::wstring &str)
{
	return wstrstri(str.c_str(), L"Intel") != 0;
}

static void check_hwaccel_support(void)
{
	/* do not use hardware acceleration if a blacklisted device is the
	 * default and on 2 or more adapters */
	const wchar_t **device = blacklisted_devices;

	if (adapterCount >= 2 || !is_intel(deviceId)) {
		while (*device) {
			if (!!wstrstri(deviceId.c_str(), *device)) {
				hwaccel = false;
				blog(LOG_INFO, "[obs-browser]: "
					       "Blacklisted device "
					       "detected, "
					       "disabling browser "
					       "source hardware "
					       "acceleration.");
				break;
			}
			device++;
		}
	}
}
#else
static void check_hwaccel_support(void)
{
	return;
}
#endif
#endif

//PRISM/ChengBing/20230425/#porting from PRISM 3.1.3
extern void DispatchPrismEvent(const char *eventName, const char *jsonString);
//PRISM/Zhangdewen/20200901/#for chat source
extern void DispatchPrismEvent(obs_source_t *source, const char *eventName, const char *jsonString);
bool obs_module_load(void)
{
	//PRISM/ChengBing/20230425/#porting from PRISM 3.1.3
	pls_frontend_set_dispatch_js_event_cb(DispatchPrismEvent);
	//PRISM/Zhangdewen/20200901/#for chat source
	pls_frontend_set_dispatch_js_event_to_source_cb(DispatchPrismEvent);

#if defined(_WIN32)
	//PRISM/Xiewei/20221226/obs upgrade: cef interaction
	InteractionManager::Instance();
	ImageManager::Instance();

	GDIPlusHelper::StartupGDIPlus();
	InteractionViewWin::RegisterClassName();
	BrowserInteractionMainWin::RegisterClassName();
#endif

#ifdef ENABLE_BROWSER_QT_LOOP
	qRegisterMetaType<MessageTask>("MessageTask");
#endif

	os_event_init(&cef_started_event, OS_EVENT_TYPE_MANUAL);

#if defined(_WIN32) && CHROME_VERSION_BUILD < 5615
	/* CefEnableHighDPISupport doesn't do anything on OS other than Windows. Would also crash macOS at this point as CEF is not directly linked */
	CefEnableHighDPISupport();
#endif

#ifdef _WIN32
	EnumAdapterCount();
#else
#if defined(__APPLE__) && !defined(ENABLE_BROWSER_LEGACY)
	/* Load CEF at runtime as required on macOS */
	CefScopedLibraryLoader library_loader;
	if (!library_loader.LoadInMain())
		return false;
#endif
#endif
	blog(LOG_INFO, "[obs-browser]: Version %s", OBS_BROWSER_VERSION_STRING);
	blog(LOG_INFO, "[obs-browser]: CEF Version %i.%i.%i.%i (runtime), %s (compiled)", cef_version_info(4),
	     cef_version_info(5), cef_version_info(6), cef_version_info(7), CEF_VERSION);

	RegisterBrowserSource();
	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	OBSDataAutoRelease private_data = obs_get_private_data();
	hwaccel = obs_data_get_bool(private_data, "BrowserHWAccel");

	if (hwaccel) {
		check_hwaccel_support();
	}
#endif

#if defined(__APPLE__) && CHROME_VERSION_BUILD < 4183
	// Make sure CEF malloc hijacking happens early in the process
	obs_browser_initialize();
#endif

	return true;
}

void obs_module_post_load(void)
{
	auto vendor = obs_websocket_register_vendor("obs-browser");
	if (!vendor)
		return;

	auto emit_event_request_cb = [](obs_data_t *request_data, obs_data_t *, void *) {
		const char *event_name = obs_data_get_string(request_data, "event_name");
		if (!event_name)
			return;

		OBSDataAutoRelease event_data = obs_data_get_obj(request_data, "event_data");
		const char *event_data_string = event_data ? obs_data_get_json(event_data) : "{}";

		DispatchJSEvent(event_name, event_data_string, nullptr);
	};

	if (!obs_websocket_vendor_register_request(vendor, "emit_event", emit_event_request_cb, nullptr))
		blog(LOG_WARNING, "[obs-browser]: Failed to register obs-websocket request emit_event");
}

void obs_module_unload(void)
{

	//PRISM/Renjinbo/20231124/#2094 crash. cef shutdown must wait for all browsers to close, otherwise it will crash.
	int maxWaitMs = 0;
	while (!is_all_browers_close_done && maxWaitMs < 5000) {
		qApp->processEvents();
		maxWaitMs += 50;
		os_sleep_ms(50);
		blog(LOG_WARNING,
		     "[obs-browser]: obs_module_unload: wait browser close time:%d ms, is all browser close:%d",
		     maxWaitMs, is_all_browers_close_done.load());
	}
	//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
	blog(LOG_INFO, "[obs-browser]: browser start to shutdown or close thread");
#ifdef ENABLE_BROWSER_QT_LOOP
	BrowserShutdown();
#else
	if (manager_thread.joinable()) {
		if (!QueueCEFTask([]() { CefQuitMessageLoop(); }))
			blog(LOG_DEBUG, "[obs-browser]: Failed to post CefQuit task to loop");

		manager_thread.join();
	}
#endif
	//PRISM/Renjinbo/20250306/#PRISM_PC_NELO-196/add track log
	blog(LOG_INFO, "[obs-browser]: browser succeed to shutdown or close thread");

	//PRISM/Zhangdewen/20240122/PRISM_PC-2212 can't exit.
	manager_destroyed = true;

	os_event_destroy(cef_started_event);

	//PRISM/Xiewei/20221226/obs upgrade: cef interaction
#if defined(_WIN32)
	ImageManager::Instance()->ClearImage();
	GDIPlusHelper::ShutdownGDIPlus();
	InteractionViewWin::UnregisterClassName();
	BrowserInteractionMainWin::UnregisterClassName();
#endif
}

//PRISM/Renjinbo/20250312/#/output cef raw log to file and upload to nelo
static std::wstring getCefLogFile()
{
	auto tempDir = std::filesystem::temp_directory_path();
	std::filesystem::path cefDir = tempDir / "PRISMLiveStudio_cef";

	//TODO: jimbo.ren change to filesystem to mkdir
	char *str = NULL;
	os_wcs_to_utf8_ptr(cefDir.generic_wstring().c_str(), 0, &str);
	os_mkdir(str);
	bfree(str);

	char *uuid = os_generate_uuid();
	auto logPath = cefDir / uuid;
	bfree(uuid);
	return logPath.generic_wstring();
}
