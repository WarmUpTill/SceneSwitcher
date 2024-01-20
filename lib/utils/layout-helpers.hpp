#pragma once
#include "export-symbol-helper.hpp"

#include <QLayout>
#include <QWidget>
#include <string>
#include <unordered_map>

namespace advss {

EXPORT void
PlaceWidgets(std::string text, QBoxLayout *layout,
	     const std::unordered_map<std::string, QWidget *> &placeholders,
	     bool addStretch = true);
void DeleteLayoutItemWidget(QLayoutItem *item);
EXPORT void ClearLayout(QLayout *layout, int afterIdx = 0);
EXPORT void SetLayoutVisible(QLayout *layout, bool visible);
EXPORT void SetGridLayoutRowVisible(QGridLayout *layout, int row, bool visible);
EXPORT void AddStretchIfNecessary(QBoxLayout *layout);
EXPORT void RemoveStretchIfPresent(QBoxLayout *layout);
EXPORT void MinimizeSizeOfColumn(QGridLayout *layout, int idx);

} // namespace advss
