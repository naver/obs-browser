#pragma once
#if defined(_WIN32)
#include <windows.h>
using WindowHandle = HWND;
#elif defined(__APPLE__)
using WindowHandle = void *;
#endif

using SOURCE_HANDLE = void *;
