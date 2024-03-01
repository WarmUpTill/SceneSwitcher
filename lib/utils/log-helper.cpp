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
	       (GetSwitcher()->logLevel ==
			SwitcherData::LogLevel::PRINT_ACTION ||
		GetSwitcher()->logLevel == SwitcherData::LogLevel::VERBOSE);
}

} // namespace advss
