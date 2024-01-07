#pragma once
#include "macro-action-edit.hpp"
#include "macro-segment-selection.hpp"
#include "regex-config.hpp"
#include "resizing-text-edit.hpp"
#include "scene-selection.hpp"
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
	bool PostLoad() override;
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionVariable>(m);
	}
	void SetSegmentIndexValue(int);
	int GetSegmentIndexValue() const;

	enum class Type {
		SET_FIXED_VALUE,
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
	};

	Type _type = Type::SET_FIXED_VALUE;
	std::weak_ptr<Variable> _variable;
	std::weak_ptr<Variable> _variable2;
	StringVariable _strValue = "";
	double _numValue = 0;
	int _subStringStart = 0;
	int _subStringSize = 0;
	RegexConfig _subStringRegex = RegexConfig::PartialMatchRegexConfig();
	std::string _regexPattern = ".*";
	int _regexMatchIdx = 0;
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

private:
	void DecrementCurrentSegmentVariableRef();
	void HandleIndexSubString(Variable *);
	void HandleRegexSubString(Variable *);
	void HandleFindAndReplace(Variable *);
	void HandleMathExpression(Variable *);
	void SetToSceneItemName(Variable *);

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
	void NumValueChanged(double);
	void SegmentIndexChanged(const IntVariable &);
	void UpdateSegmentVariableValue();
	void MacroSegmentOrderChanged();
	void SubStringStartChanged(int val);
	void SubStringSizeChanged(int val);
	void SubStringRegexChanged(const RegexConfig &conf);
	void RegexPatternChanged();
	void RegexMatchIdxChanged(int val);
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

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	void SetSegmentValueError(const QString &);

	VariableSelection *_variables;
	VariableSelection *_variables2;
	FilterComboBox *_actions;
	VariableTextEdit *_strValue;
	QDoubleSpinBox *_numValue;
	MacroSegmentSelection *_segmentIdx;
	QLabel *_segmentValueStatus;
	ResizingPlainTextEdit *_segmentValue;
	QVBoxLayout *_substringLayout;
	QHBoxLayout *_subStringIndexEntryLayout;
	QHBoxLayout *_subStringRegexEntryLayout;
	QSpinBox *_subStringStart;
	QSpinBox *_subStringSize;
	RegexConfigWidget *_substringRegex;
	ResizingPlainTextEdit *_regexPattern;
	QSpinBox *_regexMatchIdx;
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
	VariableSpinBox *_sceneItemIndex;
	QHBoxLayout *_entryLayout;

	std::shared_ptr<MacroActionVariable> _entryData;
	QTimer _timer;
	bool _loading = true;
};

} // namespace advss
