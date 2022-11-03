#pragma once
#include "hotkey.hpp"

#include <vector>
#include <string>
#include <QStringList>

void GetWindowList(std::vector<std::string> &windows);
void GetWindowList(QStringList &windows);
void GetCurrentWindowTitle(std::string &title);
bool isFullscreen(const std::string &title);
bool isMaximized(const std::string &title);
std::pair<int, int> getCursorPos();
int secondsSinceLastInput();
void GetProcessList(QStringList &processes);
void GetForegroundProcessName(std::string &name);
bool isInFocus(const QString &executable);
void PressKeys(const std::vector<HotkeyType> keys, int duration);
void PlatformInit();
void PlatformCleanup();
