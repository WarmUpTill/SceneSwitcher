#include "macro-condition-variable.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionVariable::id = "variable";

bool MacroConditionVariable::_registered = MacroConditionFactory::Register(
	MacroConditionVariable::id,
	{MacroConditionVariable::Create, MacroConditionVariableEdit::Create,
	 "AdvSceneSwitcher.condition.variable"});

const static std::map<MacroConditionVariable::Condition, std::string>
	conditionTypes = {
		{MacroConditionVariable::Condition::EQUALS,
		 "AdvSceneSwitcher.condition.variable.type.compare"},
		{MacroConditionVariable::Condition::IS_EMPTY,
		 "AdvSceneSwitcher.condition.variable.type.empty"},
		{MacroConditionVariable::Condition::IS_NUMBER,
		 "AdvSceneSwitcher.condition.variable.type.number"},
		{MacroConditionVariable::Condition::LESS_THAN,
		 "AdvSceneSwitcher.condition.variable.type.lessThan"},
		{MacroConditionVariable::Condition::GREATER_THAN,
		 "AdvSceneSwitcher.condition.variable.type.greaterThan"},
		{MacroConditionVariable::Condition::VALUE_CHANGED,
		 "AdvSceneSwitcher.condition.variable.type.valueChanged"},
		{MacroConditionVariable::Condition::EQUALS_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.equalsVariable"},
		{MacroConditionVariable::Condition::LESS_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.lessThanVariable"},
		{MacroConditionVariable::Condition::GREATER_THAN_VARIABLE,
		 "AdvSceneSwitcher.condition.variable.type.greaterThanVariable"},
};

static bool isNumber(const Variable &var)
{
	return var.DoubleValue().has_value();
}

static bool compareNumber(const Variable &var, double value, bool less)
{
	auto varValue = var.DoubleValue();
	if (!varValue.has_value()) {
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
		return _regex.Matches(var.Value(), _strValue);
	}
	return std::string(_strValue) == var.Value();
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
	auto var1 = _variable.lock();
	auto var2 = _variable2.lock();
	if (!var1 || !var2) {
		return false;
	}

	auto val1 = var1->DoubleValue();
	auto val2 = var2->DoubleValue();
	bool validNumbers = val1.has_value() && val2.has_value();

	switch (_type) {
	case MacroConditionVariable::Condition::EQUALS_VARIABLE:
		return var1->Value() == var2->Value() ||
		       (validNumbers && val1 == val2);
	case MacroConditionVariable::Condition::LESS_THAN_VARIABLE:
		return validNumbers && val1 < val2;
	case MacroConditionVariable::Condition::GREATER_THAN_VARIABLE:
		return validNumbers && val1 > val2;
	default:
		blog(LOG_WARNING,
		     "Unexpected call of %s with condition type %d", __func__,
		     static_cast<int>(_type));
	}

	return false;
}

bool MacroConditionVariable::CheckCondition()
{
	auto var = _variable.lock();
	if (!var) {
		return false;
	}

	switch (_type) {
	case MacroConditionVariable::Condition::EQUALS:
		return Compare(*var);
	case MacroConditionVariable::Condition::IS_EMPTY:
		return var->Value().empty();
	case MacroConditionVariable::Condition::IS_NUMBER:
		return isNumber(*var);
	case MacroConditionVariable::Condition::LESS_THAN:
		return compareNumber(*var, _numValue, true);
	case MacroConditionVariable::Condition::GREATER_THAN:
		return compareNumber(*var, _numValue, false);
	case MacroConditionVariable::Condition::VALUE_CHANGED:
		return ValueChanged(*var);
	case MacroConditionVariable::Condition::EQUALS_VARIABLE:
		return CompareVariables();
	case MacroConditionVariable::Condition::LESS_THAN_VARIABLE:
		return CompareVariables();
	case MacroConditionVariable::Condition::GREATER_THAN_VARIABLE:
		return CompareVariables();
	}

	return false;
}

bool MacroConditionVariable::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "variableName",
			    GetWeakVariableName(_variable).c_str());
	obs_data_set_string(obj, "variable2Name",
			    GetWeakVariableName(_variable2).c_str());
	_strValue.Save(obj, "strValue");
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	_regex.Save(obj);
	return true;
}

bool MacroConditionVariable::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_variable =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));
	_variable2 = GetWeakVariableByName(
		obs_data_get_string(obj, "variable2Name"));
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

std::string MacroConditionVariable::GetShortDesc() const
{
	return GetWeakVariableName(_variable);
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
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
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
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables},
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

void MacroConditionVariableEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_variables->SetVariable(_entryData->_variable);
	_variables2->SetVariable(_entryData->_variable2);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->setValue(_entryData->_numValue);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroConditionVariableEdit::VariableChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionVariableEdit::Variable2Changed(const QString &text)
{

	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_variable2 = GetWeakVariableByQString(text);
}

void MacroConditionVariableEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type =
		static_cast<MacroConditionVariable::Condition>(value);
	SetWidgetVisibility();
}

void MacroConditionVariableEdit::StrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroConditionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_numValue = val;
}

void MacroConditionVariableEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
			   MacroConditionVariable::Condition::EQUALS);
	_strValue->setVisible(_entryData->_type ==
			      MacroConditionVariable::Condition::EQUALS);
	_numValue->setVisible(
		_entryData->_type ==
			MacroConditionVariable::Condition::LESS_THAN ||
		_entryData->_type ==
			MacroConditionVariable::Condition::GREATER_THAN);
	_numValue->setVisible(
		_entryData->_type ==
			MacroConditionVariable::Condition::LESS_THAN ||
		_entryData->_type ==
			MacroConditionVariable::Condition::GREATER_THAN);
	_variables2->setVisible(
		_entryData->_type ==
			MacroConditionVariable::Condition::EQUALS_VARIABLE ||
		_entryData->_type ==
			MacroConditionVariable::Condition::LESS_THAN_VARIABLE ||
		_entryData->_type ==
			MacroConditionVariable::Condition::GREATER_THAN_VARIABLE);

	adjustSize();
	updateGeometry();
}

} // namespace advss
