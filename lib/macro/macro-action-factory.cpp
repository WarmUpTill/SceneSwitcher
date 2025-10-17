#include "macro-action-factory.hpp"
#include "macro-segment-unknown.hpp"

#include <mutex>

namespace advss {

using MacroActionUnknown = MacroSegmentUnknown<MacroAction>;

static std::recursive_mutex mutex;

std::map<std::string, MacroActionInfo> &MacroActionFactory::GetMap()
{
	static std::map<std::string, MacroActionInfo> _methods;
	return _methods;
}

bool MacroActionFactory::Register(const std::string &id, MacroActionInfo info)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

bool MacroActionFactory::Deregister(const std::string &id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (GetMap().count(id) == 0) {
		return false;
	}
	GetMap().erase(id);
	return true;
}

static std::shared_ptr<MacroAction> createUnknownAction(Macro *m,
							const std::string &id)
{
	return std::make_shared<MacroActionUnknown>(m, id);
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string &id,
							Macro *m)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._create(m);
	}

	return createUnknownAction(m, id);
}

QWidget *MacroActionFactory::CreateWidget(const std::string &id,
					  QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidget(parent, action);
	}

	return CreateUnknownSegmentWidget(true);
}

std::string MacroActionFactory::GetActionName(const std::string &id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return obs_module_text("AdvSceneSwitcher.action.unknown");
}

std::string MacroActionFactory::GetIdByName(const QString &name)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool CanCreateDefaultAction()
{
	const auto action = MacroActionFactory::Create(
		MacroAction::GetDefaultID().data(), nullptr);
	if (!action) {
		return false;
	}
	return action->GetId() == MacroAction::GetDefaultID().data();
}

} // namespace advss
