#pragma once
#include <chrono>

namespace advss {

std::chrono::high_resolution_clock::time_point GetLastMouseLeftClickTime();
std::chrono::high_resolution_clock::time_point GetLastMouseMiddleClickTime();
std::chrono::high_resolution_clock::time_point GetLastMouseRightClickTime();
bool IsMouseLeftButtonDown();
bool IsMouseMiddleButtonDown();
bool IsMouseRightButtonDown();
std::chrono::high_resolution_clock::time_point GetLastPenTipClickTime();
std::chrono::high_resolution_clock::time_point GetLastPenBarrelClickTime();
bool IsPenTipDown();
bool IsPenBarrelDown();

} // namespace advss
