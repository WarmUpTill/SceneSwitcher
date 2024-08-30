#pragma once
#include "macro-action.hpp"

#include <memory>

namespace advss {

struct MacroActionInfo {
	using CreateActionWidget = QWidget *(*)(QWidget *parent,
						std::shared_ptr<MacroAction>);

	std::function<std::shared_ptr<MacroAction>(Macro *m)> _create = nullptr;
	CreateActionWidget _createWidget = nullptr;
	std::string _name;
};

class MacroActionFactory {
public:
	MacroActionFactory() = delete;

	EXPORT static bool Register(const std::string &id, MacroActionInfo);
	static bool Deregister(const std::string &id);
	static std::shared_ptr<MacroAction> Create(const std::string &id,
						   Macro *m);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroAction> action);
	static auto GetActionTypes() { return GetMap(); }
	static std::string GetActionName(const std::string &id);
	static std::string GetIdByName(const QString &name);

private:
	static std::map<std::string, MacroActionInfo> &GetMap();
};

bool CanCreateDefaultAction();

} // namespace advss
