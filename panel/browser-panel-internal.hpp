#pragma once

#include <qglobal.h>

#if defined(Q_OS_WIN)
#include "browser-panel-internal_win.hpp"
#elif defined(Q_OS_MACOS)
#include "browser-panel-internal_mac.hpp"
#endif
