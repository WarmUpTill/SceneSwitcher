#include "audio-helpers.hpp"
#include "obs-module-helper.hpp"

#include <obs.hpp>

namespace advss {

void PopulateMonitorTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audio.monitor.monitorOnly"));
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.both"));
}

} // namespace advss
