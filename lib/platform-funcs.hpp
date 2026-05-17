#pragma once
#include "export-symbol-helper.hpp"

#include <vector>
#include <string>
#include <QStringList>
#include <chrono>
#include <optional>

namespace advss {

enum class HotkeyType;

struct WindowGeometry {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

EXPORT std::vector<std::string> GetWindowList();
EXPORT std::string GetCurrentWindowTitle();
EXPORT bool IsFullscreen(const std::string &title);
EXPORT bool IsMaximized(const std::string &title);
EXPORT std::optional<std::string> GetTextInWindow(const std::string &window);
EXPORT std::optional<WindowGeometry>
GetWindowGeometry(const std::string &title);
EXPORT int SecondsSinceLastInput();
EXPORT QStringList GetProcessList();
EXPORT std::string GetForegroundProcessName();
EXPORT std::string GetForegroundProcessPath();
EXPORT QStringList GetProcessPathsFromName(const QString &name);
EXPORT bool IsInFocus(const QString &executable);
void PlatformInit();
void PlatformCleanup();

} // namespace advss
