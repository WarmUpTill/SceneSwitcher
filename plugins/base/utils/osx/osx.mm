#include "hotkey-helpers.hpp"

namespace advss {

static bool canSimulateKeyPresses = false;

bool CanSimulateKeyPresses()
{
	return canSimulateKeyPresses;
}

void PressKeys(const std::vector<HotkeyType> &, int)
{
	// Not supported on MacOS
	return;
}

} // namespace advss
