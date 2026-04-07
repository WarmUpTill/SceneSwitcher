#pragma once
#include "export-symbol-helper.hpp"

#include <vector>
#include <string>
#include <QStringList>
#include <chrono>
#include <optional>

namespace advss {

enum class HotkeyType;

EXPORT std::vector<std::string> GetWindowList();
EXPORT std::string GetCurrentWindowTitle();
EXPORT bool IsFullscreen(const std::string &title);
EXPORT bool IsMaximized(const std::string &title);
EXPORT std::optional<std::string> GetTextInWindow(const std::string &window);
EXPORT int SecondsSinceLastInput();
EXPORT QStringList GetProcessList();
EXPORT std::string GetForegroundProcessName();
EXPORT std::string GetForegroundProcessPath();
EXPORT QStringList GetProcessPathsFromName(const QString &name);
EXPORT bool IsInFocus(const QString &executable);
void PlatformInit();
void PlatformCleanup();

} // namespace advss
