#pragma once
#include <QComboBox>

namespace advss {

void PopulateMonitorTypeSelection(QComboBox *list);
float DecibelToPercent(float db);
float PercentToDecibel(float percent);

} // namespace advss
