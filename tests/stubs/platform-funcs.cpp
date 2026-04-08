#include "platform-funcs.hpp"
#include "selection-helpers.hpp"

#include <QComboBox>
#include <optional>
#include <string>
#include <vector>

namespace advss {

namespace {

std::string g_foregroundProcessName;
std::string g_foregroundProcessPath;
QStringList g_processList;
QStringList g_processPathsFromName;

} // namespace

void SetStubForegroundProcessName(const std::string &name)
{
	g_foregroundProcessName = name;
}

void SetStubForegroundProcessPath(const std::string &path)
{
	g_foregroundProcessPath = path;
}

void SetStubProcessList(const QStringList &list)
{
	g_processList = list;
}

void SetStubProcessPaths(const QStringList &paths)
{
	g_processPathsFromName = paths;
}

// --- platform-funcs.hpp stubs ---

std::vector<std::string> GetWindowList()
{
	return {};
}

std::string GetCurrentWindowTitle()
{
	return {};
}

bool IsFullscreen(const std::string &)
{
	return false;
}

bool IsMaximized(const std::string &)
{
	return false;
}

std::optional<std::string> GetTextInWindow(const std::string &)
{
	return {};
}

int SecondsSinceLastInput()
{
	return 0;
}

QStringList GetProcessList()
{
	return g_processList;
}

std::string GetForegroundProcessName()
{
	return g_foregroundProcessName;
}

std::string GetForegroundProcessPath()
{
	return g_foregroundProcessPath;
}

QStringList GetProcessPathsFromName(const QString &)
{
	return g_processPathsFromName;
}

bool IsInFocus(const QString &)
{
	return false;
}

// --- selection-helpers.hpp stubs ---

void PopulateProcessSelection(QComboBox *, bool) {}

} // namespace advss
