#include "obs-module-helper.hpp"
#ifndef UNIT_TEST
#include "switcher-data.hpp"
#endif

#ifndef UNIT_TEST
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

#else

const char *obs_module_text(const char *text)
{
	return text;
};
obs_module_t *obs_current_module()
{
	return nullptr;
}
#endif
