#include "macro-action-factory.hpp"

#include <mutex>

namespace advss {

namespace {

class MacroActionUnknown : public MacroAction {
public:
	MacroActionUnknown(Macro *m) : MacroAction(m) {}
	std::shared_ptr<MacroAction> Copy() const
	{
		return std::make_shared<MacroActionUnknown>(GetMacro());
	}
	bool PerformAction() { return true; };
	bool Save(obs_data_t *obj) const { return MacroAction::Save(obj); };
	bool Load(obs_data_t *obj) { return MacroAction::Load(obj); };
	std::string GetId() const { return "unknown"; }
};

} // namespace

static std::mutex mutex;

std::map<std::string, MacroActionInfo> &MacroActionFactory::GetMap()
{
	static std::map<std::string, MacroActionInfo> _methods;
	return _methods;
}

bool MacroActionFactory::Register(const std::string &id, MacroActionInfo info)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

bool MacroActionFactory::Deregister(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (GetMap().count(id) == 0) {
		return false;
	}
	GetMap().erase(id);
	return true;
}

static std::shared_ptr<MacroAction> createUnknownAction(Macro *m)
{
	return std::make_shared<MacroActionUnknown>(m);
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string &id,
							Macro *m)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._create(m);
	}

	return createUnknownAction(m);
}

static QWidget *createUnknownActionWidget()
{
	return new QLabel(obs_module_text("AdvSceneSwitcher.action.unknown"));
}

QWidget *MacroActionFactory::CreateWidget(const std::string &id,
					  QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidget(parent, action);
	}

	return createUnknownActionWidget();
}

std::string MacroActionFactory::GetActionName(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return obs_module_text("AdvSceneSwitcher.action.unknown");
}

std::string MacroActionFactory::GetIdByName(const QString &name)
{
	std::lock_guard<std::mutex> lock(mutex);
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
	return !!action;
}

} // namespace advss
