#include "audio-helpers.hpp"
#include "obs-module-helper.hpp"

#include <cmath>
#include <obs.hpp>

namespace advss {

// OBS 33 replaced monitoring types with a bool and dropped "monitor only"
static constexpr bool monitorOnlySupported = LIBOBS_API_VER <
					     MAKE_SEMANTIC_VERSION(33, 0, 0);

void PopulateMonitorTypeSelection(QComboBox *list,
				  bool alwaysIncludeMonitorOnly)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.none"),
		      OBS_MONITORING_TYPE_NONE);
	if (monitorOnlySupported || alwaysIncludeMonitorOnly) {
		list->addItem(
			obs_module_text(
				"AdvSceneSwitcher.audio.monitor.monitorOnly"),
			OBS_MONITORING_TYPE_MONITOR_ONLY);
	}
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.both"),
		      OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
}

obs_monitoring_type ToSupportedMonitoringType(obs_monitoring_type type)
{
	if (!monitorOnlySupported && type == OBS_MONITORING_TYPE_MONITOR_ONLY) {
		return OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT;
	}
	return type;
}

void SetMonitoringType(obs_source_t *source, obs_monitoring_type type)
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(33, 0, 0)
	obs_source_set_monitoring_enabled(source,
					  type != OBS_MONITORING_TYPE_NONE);
#else
	obs_source_set_monitoring_type(source, type);
#endif
}

obs_monitoring_type GetMonitoringType(const obs_source_t *source)
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(33, 0, 0)
	return obs_source_get_monitoring_enabled(source)
		       ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
		       : OBS_MONITORING_TYPE_NONE;
#else
	return obs_source_get_monitoring_type(source);
#endif
}

float DecibelToPercent(float db)
{
	return isfinite((double)db) ? powf(10.0f, db / 20.0f) : 0.0f;
}

float PercentToDecibel(float percent)
{
	return (percent == 0.0f) ? -INFINITY : (20.0f * log10f(percent));
}

} // namespace advss
