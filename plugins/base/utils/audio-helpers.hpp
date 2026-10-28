#pragma once
#include <obs.h>
#include <QComboBox>

namespace advss {

void PopulateMonitorTypeSelection(QComboBox *list,
				  bool alwaysIncludeMonitorOnly = false);
void SetMonitoringType(obs_source_t *source, obs_monitoring_type type);
obs_monitoring_type GetMonitoringType(const obs_source_t *source);
obs_monitoring_type ToSupportedMonitoringType(obs_monitoring_type type);
float DecibelToPercent(float db);
float PercentToDecibel(float percent);

} // namespace advss
