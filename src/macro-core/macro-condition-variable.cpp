#include "macro-condition-edit.hpp"
#include "macro-condition-variable.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionVariable::id = "variable";

bool MacroConditionVariable::_registered = MacroConditionFactory::Register(
	MacroConditionVariable::id,
	{MacroConditionVariable::Create, MacroConditionVariableEdit::Create,
	 "AdvSceneSwitcher.condition.variable"});

const static std::map<MacroConditionVariable::Type, std::string>
	conditionTypes = {
		{MacroConditionVariable::Type::EQUALS,
		 "AdvSceneSwitcher.condition.variable.type.compare"},
		{MacroConditionVariable::Type::IS_EMPTY,
		 "AdvSceneSwitcher.condition.variable.type.empty"},
		{MacroConditionVariable::Type::IS_NUMBER,
		 "AdvSceneSwitcher.condition.variable.type.number"},
		{MacroConditionVariable::Type::LESS_THAN,
		 "AdvSceneSwitcher.condition.variable.type.lessThan"},
		{MacroConditionVariable::Type::GREATER_THAN,
		 "AdvSceneSwitcher.condition.variable.type.greaterThan"},
		{MacroConditionVariable::Type::VALUE_CHANGED,
		 "AdvSceneSwitcher.condition.variable.type.valueChanged"},
		{MacroConditionVariable::Type::EQUALS_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.equalsVariable"},
		{MacroConditionVariable::Type::LESS_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.lessThanVariable"},
		{MacroConditionVariable::Type::GREATER_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.greaterThanVariable"},
};

static bool isNumber(const Variable &var)
{
	double _;
	return var.DoubleValue(_);
}

static bool compareNumber(const Variable &var, double value, bool less)
{
	double varValue;

	if (!var.DoubleValue(varValue)) {
		return false;
	}
	if (less) {
		return varValue < value;
	}
	return varValue > value;
}

bool MacroConditionVariable::Compare(const Variable &var) const
{
	if (_regex.Enabled()) {
		auto expr = _regex.GetRegularExpression(_strValue);
		if (!expr.isValid()) {
			return false;
		}
		auto match = expr.match(QString::fromStdString(var.Value()));
		return match.hasMatch();
	}

	return _strValue == var.Value();
}

bool MacroConditionVariable::ValueChanged(const Variable &var)
{
	bool changed = var.Value() != _lastValue;
	if (changed) {
		_lastValue = var.Value();
	}
	return changed;
}

bool MacroConditionVariable::CompareVariables()
{
	auto var1 = GetVariableByName(_variableName);
	auto var2 = GetVariableByName(_variable2Name);
	if (!var1 || !var2) {
		return false;
	}

	double val1, val2;
	bool validNumbers = var1->DoubleValue(val1);
	validNumbers = validNumbers && var2->DoubleValue(val2);

	switch (_type) {
	case MacroConditionVariable::Type::EQUALS_VARIABLE:
		return var1->Value() == var2->Value() ||
		       (validNumbers && val1 == val2);
	case MacroConditionVariable::Type::LESS_THAN_VARIABLE:
		return validNumbers && val1 < val2;
	case MacroConditionVariable::Type::GREATER_THAN_VARIABLE:
		return validNumbers && val1 > val2;
	}

	return false;
}

bool MacroConditionVariable::CheckCondition()
{
	auto var = GetVariableByName(_variableName);
	if (!var) {
		return false;
	}

	switch (_type) {
	case MacroConditionVariable::Type::EQUALS:
		return Compare(*var);
	case MacroConditionVariable::Type::IS_EMPTY:
		return var->Value().empty();
	case MacroConditionVariable::Type::IS_NUMBER:
		return isNumber(*var);
	case MacroConditionVariable::Type::LESS_THAN:
		return compareNumber(*var, _numValue, true);
	case MacroConditionVariable::Type::GREATER_THAN:
		return compareNumber(*var, _numValue, false);
	case MacroConditionVariable::Type::VALUE_CHANGED:
		return ValueChanged(*var);
	case MacroConditionVariable::Type::EQUALS_VARIABLE:
		return CompareVariables();
	case MacroConditionVariable::Type::LESS_THAN_VARIABLE:
		return CompareVariables();
	case MacroConditionVariable::Type::GREATER_THAN_VARIABLE:
		return CompareVariables();
	}

	return false;
}

bool MacroConditionVariable::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "variableName", _variableName.c_str());
	obs_data_set_string(obj, "variable2Name", _variable2Name.c_str());
	obs_data_set_string(obj, "strValue", _strValue.c_str());
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	_regex.Save(obj);
	return true;
}

bool MacroConditionVariable::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_variableName = obs_data_get_string(obj, "variableName");
	_variable2Name = obs_data_get_string(obj, "variable2Name");
	_strValue = obs_data_get_string(obj, "strValue");
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Type>(obs_data_get_int(obj, "condition"));
	_regex.Load(obj);
	// TODO: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	return true;
}

std::string MacroConditionVariable::GetShortDesc() const
{
	return _variableName;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionVariableEdit::MacroConditionVariableEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVariable> entryData)
	: QWidget(parent),
	  _variables(new VariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _conditions(new QComboBox()),
	  _strValue(new ResizingPlainTextEdit(this, 5, 1, 1)),
	  _numValue(new QDoubleSpinBox()),
	  _regex(new RegexConfigWidget(parent))
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateConditionSelection(_conditions);

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(_numValue, SIGNAL(valueChanged(double)), this,
			 SLOT(NumValueChanged(double)));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables},
		{"{{variables2}}", _variables2},
		{"{{conditions}}", _conditions},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{regex}}", _regex},
	};

	QHBoxLayout *layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.variable.entry"),
		layout, widgetPlaceholders);

	auto *regexLayout = new QHBoxLayout;
	regexLayout->addWidget(_regex);
	regexLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(layout);
	mainLayout->addLayout(regexLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVariableEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_variables->SetVariable(_entryData->_variableName);
	_variables2->SetVariable(_entryData->_variable2Name);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setPlainText(QString::fromStdString(_entryData->_strValue));
	_numValue->setValue(_entryData->_numValue);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroConditionVariableEdit::VariableChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variableName = text.toStdString();
}

void MacroConditionVariableEdit::Variable2Changed(const QString &text)
{

	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variable2Name = text.toStdString();
}

void MacroConditionVariableEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroConditionVariable::Type>(value);
	SetWidgetVisibility();
}

void MacroConditionVariableEdit::StrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
}

void MacroConditionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_numValue = val;
}

void MacroConditionVariableEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_regex->setVisible(_entryData->_type ==
			   MacroConditionVariable::Type::EQUALS);
	_strValue->setVisible(_entryData->_type ==
			      MacroConditionVariable::Type::EQUALS);
	_numValue->setVisible(
		_entryData->_type == MacroConditionVariable::Type::LESS_THAN ||
		_entryData->_type ==
			MacroConditionVariable::Type::GREATER_THAN);
	_numValue->setVisible(
		_entryData->_type == MacroConditionVariable::Type::LESS_THAN ||
		_entryData->_type ==
			MacroConditionVariable::Type::GREATER_THAN);
	_variables2->setVisible(
		_entryData->_type ==
			MacroConditionVariable::Type::EQUALS_VARIABLE ||
		_entryData->_type ==
			MacroConditionVariable::Type::LESS_THAN_VARIABLE ||
		_entryData->_type ==
			MacroConditionVariable::Type::GREATER_THAN_VARIABLE);

	adjustSize();
	updateGeometry();
}
