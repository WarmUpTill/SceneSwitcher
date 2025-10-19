#include "macro-signals.hpp"

namespace advss {

MacroSignalManager::MacroSignalManager(QObject *parent) : QObject(parent) {}

MacroSignalManager *MacroSignalManager::Instance()
{
	static MacroSignalManager manager;
	return &manager;
}

} // namespace advss
