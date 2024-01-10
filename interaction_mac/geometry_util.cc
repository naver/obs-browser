// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "geometry_util.h"

#include <cmath>

namespace client {

int LogicalToDevice(int value, float device_scale_factor) {
  float scaled_val = static_cast<float>(value) * device_scale_factor;
  return static_cast<int>(std::floor(scaled_val));
}

CefRect LogicalToDevice(const CefRect& value, float device_scale_factor) {
  return CefRect(LogicalToDevice(value.x, device_scale_factor),
                 LogicalToDevice(value.y, device_scale_factor),
                 LogicalToDevice(value.width, device_scale_factor),
                 LogicalToDevice(value.height, device_scale_factor));
}

int DeviceToLogical(int value, float device_scale_factor) {
  float scaled_val = static_cast<float>(value) / device_scale_factor;
  return static_cast<int>(std::floor(scaled_val));
}

void DeviceToLogical(CefMouseEvent& value, float device_scale_factor) {
  value.x = DeviceToLogical(value.x, device_scale_factor);
  value.y = DeviceToLogical(value.y, device_scale_factor);
}

void DeviceToLogical(CefTouchEvent& value, float device_scale_factor) {
  value.x = DeviceToLogical(value.x, device_scale_factor);
  value.y = DeviceToLogical(value.y, device_scale_factor);
}


void GetScaleAndCenterPos(int frame_cx, int frame_cy,
                    int window_cx, int window_cy, int &x,
                    int &y, float &scale)
{
    int new_cx, new_cy;

    double window_aspect = double(window_cx) / double(window_cy);
    double base_aspect = double(frame_cx) / double(frame_cy);

    if (window_aspect > base_aspect) {
        scale = float(window_cy) / float(frame_cy);
        new_cx = int(double(window_cy) * base_aspect);
        new_cy = window_cy;
    } else {
        scale = float(window_cx) / float(frame_cx);
        new_cx = window_cx;
        new_cy = int(float(window_cx) / base_aspect);
    }

    x = window_cx / 2 - new_cx / 2;
    y = window_cy / 2 - new_cy / 2;
}

}  // namespace client
