#pragma once
#include "macro-action-variable-group.hpp"
#include "string-list.hpp"
#include "value-input.hpp"
#include "variable-line-edit.hpp"

namespace advss {

class VariableActionInput : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	bool _useCustomPrompt = false;
	StringVariable _inputPrompt = obs_module_text(
		"AdvSceneSwitcher.action.variable.askForValuePrompt");
	bool _useInputPlaceholder = false;
	StringVariable _inputPlaceholder =
		obs_module_text("AdvSceneSwitcher.enterText");
	ValueInputType _inputType = ValueInputType::String;
	StringList _inputOptions = {"value1", "value2", "value3"};
#ifdef _WIN32
	StringVariable _envVariableName = "USERPROFILE";
#else
	StringVariable _envVariableName = "HOME";
#endif

private:
	bool AskForUserInput(MacroActionVariable &, Variable &);
};

class VariableActionInputEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionInputEdit(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void UseCustomPromptChanged(int);
	void InputPromptChanged();
	void UseInputPlaceholderChanged(int);
	void InputPlaceholderChanged();
	void InputTypeChanged(int);
	void InputOptionsChanged(const StringList &);
	void EnvVariableChanged();

private:
	VariableActionInput *_data;

	QHBoxLayout *_promptLayout;
	QCheckBox *_useCustomPrompt;
	VariableLineEdit *_inputPrompt;
	QHBoxLayout *_placeholderLayout;
	QCheckBox *_useInputPlaceholder;
	VariableLineEdit *_inputPlaceholder;
	QHBoxLayout *_inputTypeLayout;
	QComboBox *_inputType;
	StringListEdit *_inputOptions;
	VariableLineEdit *_envVariable;
};

} // namespace advss
