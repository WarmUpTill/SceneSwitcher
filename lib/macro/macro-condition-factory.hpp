#pragma once
#include "macro-condition.hpp"

#include <memory>

namespace advss {

struct MacroConditionInfo {
	using CreateConditionWidget =
		QWidget *(*)(QWidget *parent, std::shared_ptr<MacroCondition>);

	std::function<std::shared_ptr<MacroCondition>(Macro *m)> _create =
		nullptr;
	CreateConditionWidget _createWidget = nullptr;
	std::string _name;
	bool _useDurationModifier = true;
};

class MacroConditionFactory {
public:
	MacroConditionFactory() = delete;
	EXPORT static bool Register(const std::string &, MacroConditionInfo);
	EXPORT static bool Deregister(const std::string &);
	static std::shared_ptr<MacroCondition> Create(const std::string &,
						      Macro *m);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroCondition>);
	static auto GetConditionTypes() { return GetMap(); }
	static std::string GetConditionName(const std::string &);
	static std::string GetIdByName(const QString &name);
	static bool UsesDurationModifier(const std::string &id);

private:
	static std::map<std::string, MacroConditionInfo> &GetMap();
};

bool CanCreateDefaultCondition();

} // namespace advss
