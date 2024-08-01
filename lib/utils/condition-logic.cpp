#include "condition-logic.hpp"
#include "obs-module-helper.hpp"
#include "log-helper.hpp"

#include <cassert>
#include <QComboBox>

namespace advss {

const std::map<Logic::Type, const char *> Logic::localeMap = {
	{Logic::Type::NONE, {"AdvSceneSwitcher.logic.none"}},
	{Logic::Type::AND, {"AdvSceneSwitcher.logic.and"}},
	{Logic::Type::OR, {"AdvSceneSwitcher.logic.or"}},
	{Logic::Type::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{Logic::Type::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
	{Logic::Type::ROOT_NONE, {"AdvSceneSwitcher.logic.rootNone"}},
	{Logic::Type::ROOT_NOT, {"AdvSceneSwitcher.logic.not"}},
};

bool Logic::ApplyConditionLogic(Type type, bool currentMatchResult,
				bool conditionMatched, const char *context)
{
	if (!context) {
		context = "";
	}

	switch (type) {
	case Type::ROOT_NONE:
		return conditionMatched;
	case Type::ROOT_NOT:
		return !conditionMatched;
	case Type::ROOT_LAST:
		break;
	case Type::NONE:
		vblog(LOG_INFO, "skipping condition check for '%s'", context);
		return currentMatchResult;
	case Type::AND:
		return currentMatchResult && conditionMatched;
	case Type::OR:
		return currentMatchResult || conditionMatched;
	case Type::AND_NOT:
		return currentMatchResult && !conditionMatched;
	case Type::OR_NOT:
		return currentMatchResult || !conditionMatched;
	case Type::LAST:
	default:
		blog(LOG_WARNING, "ignoring invalid logic check (%s)", context);
		return currentMatchResult;
	}

	return currentMatchResult;
}

void Logic::PopulateLogicTypeSelection(QComboBox *list, bool isRootCondition)
{
	auto compare = isRootCondition
			       ? std::function<bool(int)>{[](int typeValue) {
					 return typeValue < rootOffset;
				 }}
			       : std::function<bool(int)>{[](int typeValue) {
					 return typeValue >= rootOffset;
				 }};
	for (const auto &[type, name] : localeMap) {
		const int typeValue = static_cast<int>(type);
		if (compare(typeValue)) {
			list->addItem(obs_module_text(name), typeValue);
		}
	}
}

void Logic::Save(obs_data_t *obj, const char *name) const
{
	obs_data_set_int(obj, name, static_cast<int>(_type));
}

void Logic::Load(obs_data_t *obj, const char *name)
{
	_type = static_cast<Type>(obs_data_get_int(obj, name));
}

bool Logic::IsRootType() const
{
	return _type < static_cast<Type>(rootOffset);
}

bool Logic::IsNegationType(Logic::Type type)
{
	return type == Type::ROOT_NOT || type == Type::AND_NOT ||
	       type == Type::OR_NOT;
	;
}

bool Logic::IsValidSelection(bool isRootCondition) const
{
	if (!IsRootType() == isRootCondition) {
		return false;
	}
	if (IsRootType() &&
	    (_type < Type::ROOT_NONE || _type >= Type::ROOT_LAST)) {
		return false;
	}
	if (!IsRootType() &&
	    (_type <= Type::ROOT_LAST || _type >= Type::LAST)) {
		return false;
	}
	return true;
}

} // namespace advss
