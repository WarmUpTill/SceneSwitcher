#pragma once
#include "export-symbol-helper.hpp"

#include <vector>
#include <string>
#include <QStringList>
#include <chrono>
#include <optional>

namespace advss {

enum class HotkeyType;

struct WindowQueryOptions {
	bool geometry = false;
	bool focus = false;
	bool fullscreen = false;
	bool maximized = false;
	bool windowClass = false; // Windows only, no-op on other platforms
	bool text = false; // Windows only, expensive, no-op on other platforms
};

struct WindowInfo {
	std::string title;
	bool focused = false;
	bool fullscreen = false;
	bool maximized = false;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	std::string windowClass;
	std::optional<std::string> text;
};

EXPORT std::vector<WindowInfo>
GetWindows(const WindowQueryOptions &options = {});
EXPORT std::string GetCurrentWindowTitle();
EXPORT int SecondsSinceLastInput();
EXPORT QStringList GetProcessList();
EXPORT std::string GetForegroundProcessName();
EXPORT std::string GetForegroundProcessPath();
EXPORT QStringList GetProcessPathsFromName(const QString &name);
EXPORT bool IsInFocus(const QString &executable);
void PlatformInit();
void PlatformCleanup();

} // namespace advss
