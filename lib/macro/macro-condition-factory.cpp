#include "macro-condition-factory.hpp"

#include <mutex>

namespace advss {

namespace {

class MacroConditionUnknown : public MacroCondition {
public:
	MacroConditionUnknown(Macro *m) : MacroCondition(m) {}
	bool CheckCondition() { return false; }
	bool Save(obs_data_t *obj) const { return MacroCondition::Save(obj); };
	bool Load(obs_data_t *obj) { return MacroCondition::Load(obj); };
	std::string GetId() const { return "unknown"; }
};

} // namespace

static std::mutex mutex;

std::map<std::string, MacroConditionInfo> &MacroConditionFactory::GetMap()
{
	static std::map<std::string, MacroConditionInfo> _methods;
	return _methods;
}

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

bool MacroConditionFactory::Deregister(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (GetMap().count(id) == 0) {
		return false;
	}
	GetMap().erase(id);
	return true;
}

static std::shared_ptr<MacroCondition> createUnknownCondition(Macro *m)
{
	return std::make_shared<MacroConditionUnknown>(m);
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id, Macro *m)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._create(m);
	}
	return createUnknownCondition(m);
}

static QWidget *createUnknownConditionWidget()
{
	return new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.unknown"));
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidget(parent, cond);
	}
	return createUnknownConditionWidget();
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return obs_module_text("AdvSceneSwitcher.condition.unknown");
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationModifier(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._useDurationModifier;
	}
	return false;
}

bool CanCreateDefaultCondition()
{
	const auto condition = MacroConditionFactory::Create(
		MacroCondition::GetDefaultID().data(), nullptr);
	if (!condition) {
		return false;
	}
	return condition->GetId() == MacroCondition::GetDefaultID().data();
}

} // namespace advss
