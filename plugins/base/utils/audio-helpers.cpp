#include "audio-helpers.hpp"
#include "obs-module-helper.hpp"

#include <cmath>
#include <obs.hpp>

namespace advss {

void PopulateMonitorTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audio.monitor.monitorOnly"));
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.both"));
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
