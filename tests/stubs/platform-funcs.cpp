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
std::vector<WindowInfo> g_windows;

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

void SetStubWindows(const std::vector<WindowInfo> &windows)
{
	g_windows = windows;
}

// --- platform-funcs.hpp stubs ---

std::vector<WindowInfo> GetWindows(const WindowQueryOptions &)
{
	return g_windows;
}

std::string GetCurrentWindowTitle()
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
void PopulateWindowSelection(QComboBox *, bool) {}

} // namespace advss
