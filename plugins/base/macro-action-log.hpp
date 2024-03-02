#pragma once
#include "macro-action-edit.hpp"
#include "variable-text-edit.hpp"

namespace advss {

class MacroActionLog : public MacroAction {
public:
	MacroActionLog(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	StringVariable _logMessage =
		obs_module_text("AdvSceneSwitcher.action.log.placeholder");

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionLogEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionLogEdit(QWidget *parent,
			   std::shared_ptr<MacroActionLog> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionLogEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionLog>(action));
	}

private slots:
	void LogMessageChanged();

private:
	VariableTextEdit *_logMessage;
	std::shared_ptr<MacroActionLog> _entryData;
	bool _loading = true;
};

} // namespace advss
