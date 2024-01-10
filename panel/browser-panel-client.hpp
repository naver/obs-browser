#pragma once

#include <qglobal.h>

#if defined(Q_OS_WIN)
#include "browser-panel-client_win.hpp"
#elif defined(Q_OS_MACOS)
#include "browser-panel-client_mac.hpp"
#endif
