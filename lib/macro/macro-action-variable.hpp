#pragma once
#include "macro-action-edit.hpp"
#include "help-icon.hpp"
#include "macro-segment-selection.hpp"
#include "regex-config.hpp"
#include "resizing-text-edit.hpp"
#include "scene-selection.hpp"
#include "single-char-selection.hpp"
#include "variable-line-edit.hpp"
#include "variable-text-edit.hpp"
#include "variable-spinbox.hpp"

namespace advss {

class MacroActionVariable : public MacroAction {
public:
	MacroActionVariable(Macro *m) : MacroAction(m) {}
	~MacroActionVariable();
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad();
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void SetSegmentIndexValue(int);
	int GetSegmentIndexValue() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		SET_VALUE,
		APPEND,
		APPEND_VAR,
		INCREMENT,
		DECREMENT,
		SET_CONDITION_VALUE,
		SET_ACTION_VALUE,
		ROUND_TO_INT,
		SUBSTRING,
		FIND_AND_REPLACE,
		MATH_EXPRESSION,
		USER_INPUT,
		ENV_VARIABLE,
		SCENE_ITEM_COUNT,
		STRING_LENGTH,
		EXTRACT_JSON,
		SET_TO_TEMPVAR,
		SCENE_ITEM_NAME,
		PAD,
		TRUNCATE,
		SWAP_VALUES,
		TRIM,
		CHANGE_CASE,
		RANDOM_NUMBER,
		QUERY_JSON,
		ARRAY_JSON,
	};

	Action _action = Action::SET_VALUE;
	std::weak_ptr<Variable> _variable;
	std::weak_ptr<Variable> _variable2;
	StringVariable _strValue = "";
	DoubleVariable _numValue = 0;
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
	StringVariable _mathExpression = obs_module_text(
		"AdvSceneSwitcher.action.variable.mathExpression.example");
	bool _useCustomPrompt = false;
	StringVariable _inputPrompt = obs_module_text(
		"AdvSceneSwitcher.action.variable.askForValuePrompt");
	bool _useInputPlaceholder = false;
	StringVariable _inputPlaceholder =
		obs_module_text("AdvSceneSwitcher.enterText");
#ifdef _WIN32
	StringVariable _envVariableName = "USERPROFILE";
#else
	StringVariable _envVariableName = "HOME";
#endif
	SceneSelection _scene;
	TempVariableRef _tempVar;
	IntVariable _sceneItemIndex = 1;

	enum class Direction { LEFT, RIGHT };
	Direction _direction = Direction::LEFT;
	IntVariable _stringLength = 1;
	char _paddingChar = '0';

	enum class CaseType {
		LOWER_CASE,
		UPPER_CASE,
		CAPITALIZED,
		START_CASE,
	};

	CaseType _caseType = CaseType::LOWER_CASE;
	DoubleVariable _randomNumberStart = 0;
	DoubleVariable _randomNumberEnd = 100;
	bool _generateInteger = true;

	StringVariable _jsonQuery = "$.some.nested.value";
	IntVariable _jsonIndex = 0;

private:
	void DecrementCurrentSegmentVariableRef();
	void HandleIndexSubString(Variable *);
	void HandleRegexSubString(Variable *);
	void HandleFindAndReplace(Variable *);
	void HandleMathExpression(Variable *);
	void HandleCaseChange(Variable *);
	void SetToSceneItemName(Variable *);
	void GenerateRandomNumber(Variable *);

	std::weak_ptr<MacroSegment> _macroSegment;
	int _segmentIdxLoadValue = -1;
	static bool _registered;
	static const std::string id;
};

class MacroActionVariableEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionVariableEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionVariable> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionVariableEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionVariable>(action));
	}

private slots:
	void VariableChanged(const QString &);
	void Variable2Changed(const QString &);
	void ActionChanged(int);
	void StrValueChanged();
	void NumValueChanged(const NumberVariable<double> &value);
	void SegmentIndexChanged(const IntVariable &);
	void UpdateSegmentVariableValue();
	void MacroSegmentOrderChanged();
	void SubStringStartChanged(const NumberVariable<int> &start);
	void SubStringSizeChanged(const NumberVariable<int> &size);
	void SubStringRegexChanged(const RegexConfig &conf);
	void RegexPatternChanged();
	void RegexMatchIdxChanged(const NumberVariable<int> &index);
	void FindStrValueChanged();
	void FindRegexChanged(const RegexConfig &conf);
	void ReplaceStrValueChanged();
	void MathExpressionChanged();
	void UseCustomPromptChanged(int);
	void InputPromptChanged();
	void UseInputPlaceholderChanged(int);
	void InputPlaceholderChanged();
	void EnvVariableChanged();
	void SceneChanged(const SceneSelection &);
	void SelectionChanged(const TempVariableRef &var);
	void SceneItemIndexChanged(const NumberVariable<int> &);
	void DirectionChanged(int);
	void StringLengthChanged(const NumberVariable<int> &);
	void CharSelectionChanged(const QString &);
	void CaseTypeChanged(int index);
	void RandomNumberStartChanged(const NumberVariable<double> &);
	void RandomNumberEndChanged(const NumberVariable<double> &);
	void GenerateIntegerChanged(int);
	void JsonQueryChanged();
	void JsonIndexChanged(const NumberVariable<int> &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	void SetSegmentValueError(const QString &);

	VariableSelection *_variables;
	VariableSelection *_variables2;
	FilterComboBox *_actions;
	VariableTextEdit *_strValue;
	VariableDoubleSpinBox *_numValue;
	MacroSegmentSelection *_segmentIdx;
	QLabel *_segmentValueStatus;
	ResizingPlainTextEdit *_segmentValue;
	QVBoxLayout *_substringLayout;
	QHBoxLayout *_subStringIndexEntryLayout;
	QHBoxLayout *_subStringRegexEntryLayout;
	VariableSpinBox *_subStringStart;
	VariableSpinBox *_subStringSize;
	RegexConfigWidget *_substringRegex;
	ResizingPlainTextEdit *_regexPattern;
	VariableSpinBox *_regexMatchIdx;
	QHBoxLayout *_findReplaceLayout;
	RegexConfigWidget *_findRegex;
	VariableTextEdit *_findStr;
	VariableTextEdit *_replaceStr;
	VariableLineEdit *_mathExpression;
	QLabel *_mathExpressionResult;
	QHBoxLayout *_promptLayout;
	QCheckBox *_useCustomPrompt;
	VariableLineEdit *_inputPrompt;
	QHBoxLayout *_placeholderLayout;
	QCheckBox *_useInputPlaceholder;
	VariableLineEdit *_inputPlaceholder;
	VariableLineEdit *_envVariable;
	SceneSelectionWidget *_scenes;
	TempVariableSelection *_tempVars;
	HelpIcon *_tempVarsHelp;
	VariableSpinBox *_sceneItemIndex;
	QComboBox *_direction;
	VariableSpinBox *_stringLength;
	SingleCharSelection *_paddingCharSelection;
	FilterComboBox *_caseType;
	VariableDoubleSpinBox *_randomNumberStart;
	VariableDoubleSpinBox *_randomNumberEnd;
	QCheckBox *_generateInteger;
	QVBoxLayout *_randomLayout;
	VariableLineEdit *_jsonQuery;
	QLabel *_jsonQueryHelp;
	VariableSpinBox *_jsonIndex;
	QHBoxLayout *_entryLayout;

	std::shared_ptr<MacroActionVariable> _entryData;
	QTimer _timer;
	bool _loading = true;
};

} // namespace advss
