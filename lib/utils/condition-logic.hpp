#pragma once
#include <map>
#include <obs-data.h>
#include <string>

class QComboBox;

namespace advss {

class Logic {
public:
	static constexpr auto rootOffset = 100;

	enum class Type {
		ROOT_NONE = 0,
		ROOT_NOT,
		ROOT_LAST,

		// leave some space for potential expansion
		NONE = rootOffset,
		AND,
		OR,
		AND_NOT,
		OR_NOT,
		LAST,
	};

	Logic(Type type) { _type = type; }

	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);

	Type GetType() const { return _type; }
	void SetType(const Type &type) { _type = type; }
	bool IsRootType() const;
	static bool IsNegationType(Logic::Type);
	bool IsValidSelection(bool isRootCondition) const;

	static bool ApplyConditionLogic(Type, bool currentMatchResult,
					bool conditionMatched,
					const char *context);

	static void PopulateLogicTypeSelection(QComboBox *list,
					       bool isRootCondition);

private:
	Type _type = Type::NONE;
	static const std::map<Type, const char *> localeMap;
};

} // namespace advss
