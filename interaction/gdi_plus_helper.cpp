#include "gdi_plus_helper.h"
#include <Windows.h>
#include <gdiplus.h>

static ULONG_PTR gdiplus_token;

void GDIPlusHelper::StartupGDIPlus()
{
	Gdiplus::GdiplusStartupInput startup_input;
	GdiplusStartup(&gdiplus_token, &startup_input, NULL);
}

void GDIPlusHelper::ShutdownGDIPlus()
{
	Gdiplus::GdiplusShutdown(gdiplus_token);
}
