#pragma once
#include <vector>
#include <string>
#include <QStringList>
#include <chrono>

namespace advss {

enum class HotkeyType;

// TODO: Implement for MacOS and Linux
extern std::chrono::high_resolution_clock::time_point lastMouseLeftClickTime;
extern std::chrono::high_resolution_clock::time_point lastMouseMiddleClickTime;
extern std::chrono::high_resolution_clock::time_point lastMouseRightClickTime;

void GetWindowList(std::vector<std::string> &windows);
void GetWindowList(QStringList &windows);
void GetCurrentWindowTitle(std::string &title);
bool IsFullscreen(const std::string &title);
bool IsMaximized(const std::string &title);
int SecondsSinceLastInput();
void GetProcessList(QStringList &processes);
void GetForegroundProcessName(std::string &name);
bool IsInFocus(const QString &executable);
void PressKeys(const std::vector<HotkeyType> keys, int duration);
void PlatformInit();
void PlatformCleanup();

} // namespace advss
