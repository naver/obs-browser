#include "interaction_window_helper.h"

#define DEFAULT_WINDOW_WIDTH 664
#define DEFAULT_WINDOW_HEIGHT 562

WindowSize WindowHelper::size = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT};

WindowSize WindowHelper::GetWindowSize()
{
	return WindowHelper::size;
}
void WindowHelper::SetWindowSize(const WindowSize &size)
{
	WindowHelper::size.width = size.width;
	WindowHelper::size.height = size.height;
}
