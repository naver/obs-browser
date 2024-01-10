#pragma once
#include "interaction_def.h"
#include "interaction_main.h"
//#include "interaction_button.h"
#include "interaction_util_mac.hpp"
#include "interaction_manager.h"
#include "interaction_view_mac.h"
#include <obs-data.h>
#include <memory>

#include <include/internal/cef_ptr.h>
#include <include/internal/cef_types_wrappers.h>
#include <include/cef_render_handler.h>
#include <include/cef_browser.h>

using namespace interaction;

class BrowserClient;
class BrowserInteractionMainMac : public BrowserInteractionMain {
public:
    static std::vector<std::string> font_list;
    static WindowHandle prism_main;
    
    static void SetGlobalParam(obs_data_t *private_data);
    static void RegisterClassName();
    static void UnregisterClassName();
    
    BrowserInteractionMainMac(SOURCE_HANDLE src);
    virtual ~BrowserInteractionMainMac();
    
    // Warning : Should be invoked in CEF's main thread
    void CreateInteractionUI() override;
    void WaitDestroyInteractionUI() override;
    void ShowInteractionUI(bool show) override;
    void BringUIToTop() override;
    void SetInteractionInfo(int source_width, int source_height,
                            CefRefPtr<CefBrowser> cefBrowser) override;
    void GetInteractionInfo(int &source_width, int &source_height,
                            CefRefPtr<CefBrowser> &cefBrowser) override;
    void OnImeCompositionRangeChanged(
                                      CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
                                      const CefRenderHandler::RectList &character_bounds) override;
    bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX,
                        int viewY, int &screenX, int &screenY) override;
    
    bool
    OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor,
                   cef_cursor_type_t type,
                   const CefCursorInfo &custom_cursor_info) override;
    
    // Can be invoked out of CEF's main thread
    void PostWindowTitle(const char *str) override;
    void PostDestroyInteractionUI() override;
    WindowHandle GetInteractionView() override;
    WindowHandle GetInteractionMain() override;
    bool IsResizing() override;
    
protected:
    
    void OnDestroy();
    void OnPaint();
    void OnSizing();
    void OnSize();
    void OnDpiChanged(float dpi);
    void OnCloseButton();
    
    bool ShouldClose();
    void SaveUserSize();
    void UpdateLayout();
    void UpdateImage(float dpi);
    void ShowUI();
    void MovePosition();
    void DrawTitle();
    void SetWindowSize(int left, int top, float dpi);
    
private:
    // We won't destroy window during browser source's lifetime
    bool should_close = false;
    // Set true after creating UI.
    bool window_created = false;
    // interaction main window handle
    WindowHandle interaction_wnd = NULL;
    // interaction main view handle
    InteractionViewMac view_wnd;
};
