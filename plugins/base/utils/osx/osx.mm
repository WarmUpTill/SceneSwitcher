#include "hotkey-helpers.hpp"

namespace advss {

static bool canSimulateKeyPresses = false;

bool CanSimulateKeyPresses()
{
	return canSimulateKeyPresses;
}

void PressKeys(const std::vector<HotkeyType> keys, int duration)
{
	// Not supported on MacOS
	return;
}

} // namespace advss
