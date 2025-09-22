#include "macro-condition-tempvar.hpp"
#include "layout-helpers.hpp"
#include "math-helpers.hpp"

namespace advss {

const std::string MacroConditionTempVar::id = "temp_var";

bool MacroConditionTempVar::_registered = MacroConditionFactory::Register(
	MacroConditionTempVar::id,
	{MacroConditionTempVar::Create, MacroConditionTempVarEdit::Create,
	 "AdvSceneSwitcher.condition.temporaryVariable"});

const static std::map<MacroConditionTempVar::Condition, std::string>
	conditionTypes = {
		{MacroConditionTempVar::Condition::EQUALS,
		 "AdvSceneSwitcher.condition.variable.type.compare"},
		{MacroConditionTempVar::Condition::IS_EMPTY,
		 "AdvSceneSwitcher.condition.variable.type.empty"},
		{MacroConditionTempVar::Condition::IS_NUMBER,
		 "AdvSceneSwitcher.condition.variable.type.number"},
		{MacroConditionTempVar::Condition::LESS_THAN,
		 "AdvSceneSwitcher.condition.variable.type.lessThan"},
		{MacroConditionTempVar::Condition::GREATER_THAN,
		 "AdvSceneSwitcher.condition.variable.type.greaterThan"},
		{MacroConditionTempVar::Condition::VALUE_CHANGED,
		 "AdvSceneSwitcher.condition.variable.type.valueChanged"},
		{MacroConditionTempVar::Condition::EQUALS_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.equalsVariable"},
		{MacroConditionTempVar::Condition::LESS_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.lessThanVariable"},
		{MacroConditionTempVar::Condition::GREATER_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.greaterThanVariable"},
};

static bool isNumber(const TempVariable &var)
{
	auto value = var.Value();
	if (!value) {
		return false;
	}

	return GetDouble(*value).has_value();
}

static bool compareNumber(const TempVariable &var, double value, bool less)
{
	auto tempVarValue = var.Value();
	if (!tempVarValue) {
		return false;
	}

	auto doubleValue = GetDouble(*tempVarValue);
	if (!doubleValue.has_value()) {
		return false;
	}
	if (less) {
		return doubleValue < value;
	}
	return doubleValue > value;
}

bool MacroConditionTempVar::Compare(const TempVariable &var) const
{
	auto value = var.Value();
	if (!value) {
		return false;
	}

	if (_regex.Enabled()) {
		return _regex.Matches(*value, _strValue);
	}

	return std::string(_strValue) == *value;
}

bool MacroConditionTempVar::ValueChanged(const TempVariable &var)
{
	auto value = var.Value();
	if (!value) {
		return false;
	}
	bool changed = *value != _lastValue;
	if (changed) {
		_lastValue = *value;
	}
	return changed;
}

bool MacroConditionTempVar::CompareVariables()
{
	auto var1 = _tempVar.GetTempVariable(GetMacro());
	auto var2 = _variable2.lock();
	if (!var1 || !var2) {
		return false;
	}

	auto tempVarValue = var1->Value();
	if (!tempVarValue) {
		return false;
	}
	auto val1 = GetDouble(*tempVarValue);
	auto val2 = var2->DoubleValue();
	bool validNumbers = val1.has_value() && val2.has_value();

	switch (_type) {
	case MacroConditionTempVar::Condition::EQUALS_VARIABLE:
		return *tempVarValue == var2->Value() ||
		       (validNumbers && val1 == val2);
	case MacroConditionTempVar::Condition::LESS_THAN_VARIABLE:
		return validNumbers && val1 < val2;
	case MacroConditionTempVar::Condition::GREATER_THAN_VARIABLE:
		return validNumbers && val1 > val2;
	default:
		blog(LOG_WARNING,
		     "Unexpected call of %s with condition type %d", __func__,
		     static_cast<int>(_type));
	}

	return false;
}

bool MacroConditionTempVar::CheckCondition()
{
	auto var = _tempVar.GetTempVariable(GetMacro());
	if (!var) {
		return false;
	}

	switch (_type) {
	case MacroConditionTempVar::Condition::EQUALS:
		return Compare(*var);
	case MacroConditionTempVar::Condition::IS_EMPTY:
		return var->Value().has_value() && var->Value()->empty();
	case MacroConditionTempVar::Condition::IS_NUMBER:
		return isNumber(*var);
	case MacroConditionTempVar::Condition::LESS_THAN:
		return compareNumber(*var, _numValue, true);
	case MacroConditionTempVar::Condition::GREATER_THAN:
		return compareNumber(*var, _numValue, false);
	case MacroConditionTempVar::Condition::VALUE_CHANGED:
		return ValueChanged(*var);
	case MacroConditionTempVar::Condition::EQUALS_VARIABLE:
		return CompareVariables();
	case MacroConditionTempVar::Condition::LESS_THAN_VARIABLE:
		return CompareVariables();
	case MacroConditionTempVar::Condition::GREATER_THAN_VARIABLE:
		return CompareVariables();
	}

	return false;
}

bool MacroConditionTempVar::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_tempVar.Save(obj, GetMacro());
	obs_data_set_string(obj, "variableName",
			    GetWeakVariableName(_variable2).c_str());
	_strValue.Save(obj, "strValue");
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	_regex.Save(obj);
	return true;
}

bool MacroConditionTempVar::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_tempVar.Load(obj, GetMacro());
	_variable2 =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));
	_strValue.Load(obj, "strValue");
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_regex.Load(obj);
	// TODO: remove in future version
	if (obs_data_has_user_value(obj, "regex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "regex"));
	}
	return true;
}

std::string MacroConditionTempVar::GetShortDesc() const
{
	auto var = _tempVar.GetTempVariable(GetMacro());
	if (!var) {
		return "";
	}
	return var->Name();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionTempVarEdit::MacroConditionTempVarEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTempVar> entryData)
	: QWidget(parent),
	  _tempVars(new TempVariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _conditions(new QComboBox()),
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
	  _numValue(new QDoubleSpinBox()),
	  _regex(new RegexConfigWidget(parent))
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateConditionSelection(_conditions);

	QWidget::connect(_tempVars,
			 SIGNAL(SelectionChanged(const TempVariableRef &)),
			 this, SLOT(VariableChanged(const TempVariableRef &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(_numValue, SIGNAL(valueChanged(double)), this,
			 SLOT(NumValueChanged(double)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _tempVars},
		{"{{variables2}}", _variables2},
		{"{{conditions}}", _conditions},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{regex}}", _regex},
	};

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.variable.entry"),
		layout, widgetPlaceholders);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionTempVarEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_tempVars->SetVariable(_entryData->_tempVar);
	_variables2->SetVariable(_entryData->_variable2);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->setValue(_entryData->_numValue);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroConditionTempVarEdit::VariableChanged(const TempVariableRef &var)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_tempVar = var;
}

void MacroConditionTempVarEdit::Variable2Changed(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable2 = GetWeakVariableByQString(text);
}

void MacroConditionTempVarEdit::ConditionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_type =
		static_cast<MacroConditionTempVar::Condition>(value);
	SetWidgetVisibility();
}

void MacroConditionTempVarEdit::StrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroConditionTempVarEdit::NumValueChanged(double val)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_numValue = val;
}

void MacroConditionTempVarEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionTempVarEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_regex->setVisible(_entryData->_type ==
			   MacroConditionTempVar::Condition::EQUALS);
	_strValue->setVisible(_entryData->_type ==
			      MacroConditionTempVar::Condition::EQUALS);
	_numValue->setVisible(
		_entryData->_type ==
			MacroConditionTempVar::Condition::LESS_THAN ||
		_entryData->_type ==
			MacroConditionTempVar::Condition::GREATER_THAN);
	_numValue->setVisible(
		_entryData->_type ==
			MacroConditionTempVar::Condition::LESS_THAN ||
		_entryData->_type ==
			MacroConditionTempVar::Condition::GREATER_THAN);
	_variables2->setVisible(
		_entryData->_type ==
			MacroConditionTempVar::Condition::EQUALS_VARIABLE ||
		_entryData->_type ==
			MacroConditionTempVar::Condition::LESS_THAN_VARIABLE ||
		_entryData->_type ==
			MacroConditionTempVar::Condition::GREATER_THAN_VARIABLE);

	adjustSize();
	updateGeometry();
}

} // namespace advss
