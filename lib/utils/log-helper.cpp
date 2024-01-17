#include "log-helper.hpp"
#include "switcher-data.hpp"

namespace advss {

bool VerboseLoggingEnabled()
{
	return GetSwitcher() && GetSwitcher()->verbose;
}

} // namespace advss
