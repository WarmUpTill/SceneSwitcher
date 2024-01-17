#pragma once
#include "export-symbol-helper.hpp"

#include <vector>
#include <string>
#include <QStringList>
#include <chrono>
#include <optional>

namespace advss {

enum class HotkeyType;

EXPORT void GetWindowList(std::vector<std::string> &windows);
EXPORT void GetWindowList(QStringList &windows);
EXPORT void GetCurrentWindowTitle(std::string &title);
EXPORT bool IsFullscreen(const std::string &title);
EXPORT bool IsMaximized(const std::string &title);
EXPORT std::optional<std::string> GetTextInWindow(const std::string &window);
EXPORT int SecondsSinceLastInput();
EXPORT void GetProcessList(QStringList &processes);
EXPORT void GetForegroundProcessName(std::string &name);
EXPORT bool IsInFocus(const QString &executable);
void PlatformInit();
void PlatformCleanup();

} // namespace advss
