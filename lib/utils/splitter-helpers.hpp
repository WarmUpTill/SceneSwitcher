#pragma once
#include "export-symbol-helper.hpp"

#include <obs-data.h>

#include <QList>
#include <QSplitter>

#include <string>

namespace advss {

void SaveSplitterPos(const QList<int> &sizes, obs_data_t *obj,
		     const std::string &name);
void LoadSplitterPos(QList<int> &sizes, obs_data_t *obj,
		     const std::string &name);

void CenterSplitterPosition(QSplitter *splitter);
void SetSplitterPositionByFraction(QSplitter *splitter, double fraction);
void MaximizeFirstSplitterEntry(QSplitter *splitter);
void ReduceSizeOfSplitterIdx(QSplitter *splitter, int idx);

} // namespace advss
