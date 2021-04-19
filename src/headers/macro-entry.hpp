#pragma once
#include <string>
#include <deque>
#include <memory>
#include <unordered_map>
#include <obs.hpp>
#include <obs-module.h>

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
	std::string _name;
};

class MacroCondition {
public:
	virtual bool CheckCondition() = 0;
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual int GetId() = 0;
	LogicType GetLogicType() { return _logic; }

	static std::unordered_map<LogicType, LogicTypeInfo> logicTypes;

private:
	LogicType _logic;

};

class MacroAction {
public:
	virtual bool PerformAction() = 0;
};

class Macro {

public:
	Macro(std::string name = "");
	virtual ~Macro();

	bool checkMatch();
	bool performAction();
	std::string Name() { return _name; }
	void SetName(std::string name) { _name = name; }
	std::deque<std::shared_ptr<MacroCondition>> &Conditions()
	{
		return _conditions;
	}
	std::deque<std::shared_ptr<MacroAction>> &Actions() { return _actions; }

	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);

private:
	std::string _name = "";
	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;
};
