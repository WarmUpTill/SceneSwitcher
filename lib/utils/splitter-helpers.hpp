#pragma once
#include "export-symbol-helper.hpp"

#include <obs-data.h>
#include <QList>
#include <string>

namespace advss {

void SaveSplitterPos(const QList<int> &sizes, obs_data_t *obj,
		     const std::string &name);
void LoadSplitterPos(QList<int> &sizes, obs_data_t *obj,
		     const std::string &name);

} // namespace advss
