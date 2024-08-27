#include "log-helper.hpp"
#include "switcher-data.hpp"

namespace advss {

bool VerboseLoggingEnabled()
{
	return GetSwitcher() &&
	       GetSwitcher()->logLevel == SwitcherData::LogLevel::VERBOSE;
}

bool ActionLoggingEnabled()
{
	return GetSwitcher() &&
	       (GetSwitcher()->logLevel == SwitcherData::LogLevel::LOG_ACTION ||
		VerboseLoggingEnabled());
}

bool MacroLoggingEnabled()
{
	return GetSwitcher() &&
	       (GetSwitcher()->logLevel == SwitcherData::LogLevel::LOG_MACRO ||
		ActionLoggingEnabled());
}

} // namespace advss
