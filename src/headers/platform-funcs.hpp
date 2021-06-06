#pragma once

void GetWindowList(std::vector<std::string> &windows);
void GetWindowList(QStringList &windows);
void GetCurrentWindowTitle(std::string &title);
bool isFullscreen(const std::string &title);
bool isMaximized(const std::string &title);
std::pair<int, int> getCursorPos();
int secondsSinceLastInput();
void GetProcessList(QStringList &processes);
bool isInFocus(const QString &executable);
bool GetCurrentVirtualDesktop(long &desktop);
bool GetVirtualDesktopCount(long &ndesktops);
