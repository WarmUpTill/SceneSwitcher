#pragma once
#include "macro-action-edit.hpp"

#include <variable-text-edit.hpp>

namespace advss {

class MacroActionWidget : public MacroAction {
public:
	MacroActionWidget(Macro *m) : MacroAction(m) {}
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionWidget>(m);
	}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };

	enum class Action {
		MESSAGE_DIALOG,
		ERROR_DIALOG,
	};

	Action _action = Action::MESSAGE_DIALOG;
	StringVariable _message = obs_module_text(
		"AdvSceneSwitcher.action.widget.messageDialog.message");
	StringVariable _errorMessage = obs_module_text(
		"AdvSceneSwitcher.action.widget.errorDialog.errorMessage");

private:
	static bool _registered;
	static const std::string id;

	void DisplayMessageDialog() const;
	void DisplayErrorDialog() const;
};

class MacroActionWidgetEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWidgetEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWidget> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWidgetEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWidget>(action));
	}
	void UpdateEntryData();

protected:
	std::shared_ptr<MacroActionWidget> _entryData;

private slots:
	void ActionChanged(int);
	void MessageChanged();
	void ErrorMessageChanged();

private:
	void SetupWidgetVisibility();

	bool _loading = true;

	QComboBox *_actions;
	VariableTextEdit *_message;
	VariableTextEdit *_errorMessage;
};

} // namespace advss
