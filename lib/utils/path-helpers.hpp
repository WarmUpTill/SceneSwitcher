#pragma once
#include "export-symbol-helper.hpp"

#include <QString>
#include <string>

namespace advss {

EXPORT std::string GetDataFilePath(const std::string &file);
QString GetDefaultSettingsSaveLocation();

} // namespace advss
