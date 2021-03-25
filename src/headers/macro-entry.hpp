#pragma once

#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

enum class LogicType {
	NONE,
	AND,
	OR,
	AND_NOT,
	OR_NOT,

	// conditions specific to first entry
	ROOT_NONE = 100,
	ROOT_NOT
};

struct LogicTypeInfo {
	std ::string _name;
};

class MacroCondition {
public:
	virtual bool CheckCondition() = 0;
	virtual bool Save() = 0;
	virtual bool Load() = 0;
	LogicType GetLogicType() { return _logic; }

	static std::unordered_map<LogicType, LogicTypeInfo> logicTypes;

private:
	LogicType _logic;
	int _id;
};

class MacroAction {
public:
	virtual bool PerformAction() = 0;

private:
	int _id;
};

class MacroEntry {

public:
	MacroEntry();
	virtual ~MacroEntry();

	bool checkMatch();
	bool performAction();

private:
	std::string _name = "";
	std::deque<std::unique_ptr<MacroCondition>> _conditions;
	std::deque<std::unique_ptr<MacroAction>> _actions;
};
