#include "obs-module-helper.hpp"
#include "switcher-data.hpp"

const char *obs_module_text(const char *text)
{
	if (!advss::switcher) {
		return "";
	}
	return advss::switcher->Translate(text);
}

obs_module_t *obs_current_module()
{
	if (!advss::switcher) {
		return nullptr;
	}
	return advss::switcher->GetModule();
}
