#include "macro-condition-factory.hpp"
#include "macro-segment-unknown.hpp"

#include <mutex>

namespace advss {

using MacroConditionUnknown = MacroSegmentUnknown<MacroCondition>;

static std::recursive_mutex mutex;

std::map<std::string, MacroConditionInfo> &MacroConditionFactory::GetMap()
{
	static std::map<std::string, MacroConditionInfo> _methods;
	return _methods;
}

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

bool MacroConditionFactory::Deregister(const std::string &id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (GetMap().count(id) == 0) {
		return false;
	}
	GetMap().erase(id);
	return true;
}

static std::shared_ptr<MacroCondition>
createUnknownCondition(Macro *m, const std::string &id)
{
	return std::make_shared<MacroConditionUnknown>(m, id);
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id, Macro *m)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._create(m);
	}
	return createUnknownCondition(m, id);
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidget(parent, cond);
	}
	return CreateUnknownSegmentWidget(false);
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return obs_module_text("AdvSceneSwitcher.condition.unknown");
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationModifier(const std::string &id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex);
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
