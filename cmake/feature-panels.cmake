find_package(Qt6 REQUIRED Widgets)

add_library(browser-panels INTERFACE)
add_library(OBS::browser-panels ALIAS browser-panels)

target_sources(browser-panels INTERFACE panel/browser-panel.hpp panel/PLSBrowserPanel.h)

target_include_directories(browser-panels INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/panel")

target_compile_definitions(browser-panels INTERFACE BROWSER_AVAILABLE)

#PRSIM/RenJinbo/20231031/obs 30 upgrade -> start
# obs old code
# target_sources(obs-browser PRIVATE panel/browser-panel-client.hpp panel/browser-panel-internal.hpp
#                                    panel/browser-panel.cpp panel/browser-panel-client.cpp)

qt_wrap_cpp(browser-panel_MOC panel/browser-panel.hpp panel/PLSBrowserPanel.h)
target_sources(
    obs-browser
    PRIVATE panel/browser-panel-client.hpp
              panel/browser-panel-internal.hpp
              ${browser-panel_MOC})
if(OS_WINDOWS)
 target_sources(
   obs-browser
   PRIVATE panel/browser-panel_win.cpp
             panel/browser-panel-client_win.cpp
             panel/browser-panel-client_win.hpp
             panel/browser-panel-internal_win.hpp)
elseif(OS_MACOS)
 target_sources(
   obs-browser
   PRIVATE panel/browser-panel_mac.cpp
             panel/browser-panel-client_mac.cpp
             panel/browser-panel-client_mac.hpp
             panel/browser-panel-internal_mac.hpp)
endif()
#PRSIM/RenJinbo/20231031/obs 30 upgrade -> end

target_link_libraries(obs-browser PRIVATE OBS::browser-panels Qt::Widgets)

set_target_properties(
  obs-browser
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

if(OS_WINDOWS)
  set_property(SOURCE browser-app.hpp PROPERTY SKIP_AUTOMOC TRUE)
endif()
