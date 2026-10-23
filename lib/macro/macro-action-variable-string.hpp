#pragma once
#include "macro-action-variable-group.hpp"
#include "regex-config.hpp"
#include "resizing-text-edit.hpp"
#include "single-char-selection.hpp"
#include "variable-spinbox.hpp"

namespace advss {

class VariableActionString : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	enum class Direction { LEFT, RIGHT };
	enum class CaseType {
		LOWER_CASE,
		UPPER_CASE,
		CAPITALIZED,
		START_CASE,
	};

	IntVariable _subStringStart = 0;
	IntVariable _subStringSize = 0;
	RegexConfig _subStringRegex = RegexConfig::PartialMatchRegexConfig();
	std::string _regexPattern = ".*";
	IntVariable _regexMatchIdx = 0;
	RegexConfig _findRegex;
	StringVariable _findStr = obs_module_text(
		"AdvSceneSwitcher.action.variable.findAndReplace.find");
	StringVariable _replaceStr = obs_module_text(
		"AdvSceneSwitcher.action.variable.findAndReplace.replace");
	Direction _direction = Direction::LEFT;
	IntVariable _stringLength = 1;
	char _paddingChar = '0';
	CaseType _caseType = CaseType::LOWER_CASE;

private:
	void HandleIndexSubString(Variable &);
	void HandleRegexSubString(Variable &);
	void HandleFindAndReplace(Variable &);
	void HandleCaseChange(Variable &);
};

class VariableActionStringEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionStringEdit(QWidget *parent,
				 std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void SubStringStartChanged(const NumberVariable<int> &start);
	void SubStringSizeChanged(const NumberVariable<int> &size);
	void SubStringRegexChanged(const RegexConfig &conf);
	void RegexPatternChanged();
	void RegexMatchIdxChanged(const NumberVariable<int> &index);
	void FindStrValueChanged();
	void FindRegexChanged(const RegexConfig &conf);
	void ReplaceStrValueChanged();
	void DirectionChanged(int);
	void StringLengthChanged(const NumberVariable<int> &);
	void CharSelectionChanged(const QString &);
	void CaseTypeChanged(int index);

private:
	VariableActionString *_data;

	QVBoxLayout *_substringLayout;
	QHBoxLayout *_subStringControlsLayout;
	VariableSpinBox *_subStringStart;
	VariableSpinBox *_subStringSize;
	RegexConfigWidget *_subStringRegex;
	ResizingPlainTextEdit *_regexPattern;
	VariableSpinBox *_regexMatchIdx;
	QHBoxLayout *_findReplaceLayout;
	RegexConfigWidget *_findRegex;
	VariableTextEdit *_findStr;
	VariableTextEdit *_replaceStr;
	QComboBox *_direction;
	VariableSpinBox *_stringLength;
	SingleCharSelection *_paddingCharSelection;
	FilterComboBox *_caseType;
};

} // namespace advss
