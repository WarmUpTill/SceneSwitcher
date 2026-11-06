#include "cursor-helpers.hpp"

namespace advss {

// Implemented in Windows specific implementation file

#ifndef _WIN32
std::chrono::high_resolution_clock::time_point GetLastMouseLeftClickTime()
{
	return {};
}
std::chrono::high_resolution_clock::time_point GetLastMouseMiddleClickTime()
{
	return {};
}
std::chrono::high_resolution_clock::time_point GetLastMouseRightClickTime()
{
	return {};
}
bool IsMouseLeftButtonDown()
{
	return false;
}
bool IsMouseMiddleButtonDown()
{
	return false;
}
bool IsMouseRightButtonDown()
{
	return false;
}
#endif

} // namespace advss
