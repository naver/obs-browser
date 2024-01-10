#pragma once

struct WindowSize {
	long width;
	long height;
	WindowSize(long width_, long height_) : width(width_), height(height_)
	{
	}
};
class WindowHelper {
public:
	static WindowSize size;
	static WindowSize GetWindowSize();
	static void SetWindowSize(const WindowSize &size);
};
