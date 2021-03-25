#pragma once

#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

enum class LogicTypeRoot { NONE, NOT };
enum class LogicType { NONE, AND, OR, AND_NOT, OR_NOT };

struct LogicTypeInfo {
	std ::string _name;
	//operator???
};

class MacroCondition {
public:
	virtual bool CheckCondition() = 0;
	virtual bool Save() = 0;
	virtual bool Load() = 0;

	static std::unordered_map<LogicType, LogicTypeInfo> logicTypes;
	static std::unordered_map<LogicTypeRoot, LogicTypeInfo> logicTypesRoot;

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

private:
	std::string _name = "";
	std::deque<std::unique_ptr<MacroCondition>> _conditions;
	std::deque<std::unique_ptr<MacroAction>> _actions;
};
