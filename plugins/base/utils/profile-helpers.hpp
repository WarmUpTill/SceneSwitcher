#pragma once
#include <QComboBox>
#include <string>

namespace advss {

void PopulateProfileSelection(QComboBox *list);
std::string GetPathInProfileDir(const char *filePath);

} // namespace advss
