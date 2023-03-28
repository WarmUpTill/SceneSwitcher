#pragma once
#include "macro-action-edit.hpp"
#include "regex-config.hpp"
#include "resizing-text-edit.hpp"
#include "variable-line-edit.hpp"

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
	};

	Type _type = Type::SET_FIXED_VALUE;
	std::weak_ptr<Variable> _variable;
	std::weak_ptr<Variable> _variable2;
	std::string _strValue = "";
	double _numValue = 0;
	int _subStringStart = 0;
	int _subStringSize = 0;
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig();
	std::string _regexPattern = ".*";
	int _regexMatchIdx = 0;
	std::string _findStr = obs_module_text(
		"AdvSceneSwitcher.action.variable.findAndReplace.find");
	std::string _replaceStr = obs_module_text(
		"AdvSceneSwitcher.action.variable.findAndReplace.replace");
	StringVariable _mathExpression = obs_module_text(
		"AdvSceneSwitcher.action.variable.mathExpression.example");

private:
	void DecrementCurrentSegmentVariableRef();
	void HandleIndexSubString(Variable *);
	void HandleRegexSubString(Variable *);
	void HandleFindAndReplace(Variable *);
	void HandleMathExpression(Variable *);

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
	void SegmentIndexChanged(int val);
	void UpdateSegmentVariableValue();
	void MacroSegmentOrderChanged();
	void SubStringStartChanged(int val);
	void SubStringSizeChanged(int val);
	void RegexChanged(RegexConfig conf);
	void RegexPatternChanged();
	void RegexMatchIdxChanged(int val);
	void FindStrValueChanged();
	void ReplaceStrValueChanged();
	void MathExpressionChanged();

signals:
	void HeaderInfoChanged(const QString &);

protected:
	VariableSelection *_variables;
	VariableSelection *_variables2;
	QComboBox *_actions;
	ResizingPlainTextEdit *_strValue;
	QDoubleSpinBox *_numValue;
	QSpinBox *_segmentIdx;
	QLabel *_segmentValueStatus;
	ResizingPlainTextEdit *_segmentValue;
	QVBoxLayout *_substringLayout;
	QHBoxLayout *_subStringIndexEntryLayout;
	QHBoxLayout *_subStringRegexEntryLayout;
	QSpinBox *_subStringStart;
	QSpinBox *_subStringSize;
	RegexConfigWidget *_regex;
	ResizingPlainTextEdit *_regexPattern;
	QSpinBox *_regexMatchIdx;
	QHBoxLayout *_findReplaceLayout;
	ResizingPlainTextEdit *_findStr;
	ResizingPlainTextEdit *_replaceStr;
	VariableLineEdit *_mathExpression;
	QLabel *_mathExpressionResult;
	std::shared_ptr<MacroActionVariable> _entryData;

private:
	void MarkSelectedSegment();
	void SetWidgetVisibility();
	void SetSegmentValueError(const QString &);

	QTimer _timer;
	bool _loading = true;
};
