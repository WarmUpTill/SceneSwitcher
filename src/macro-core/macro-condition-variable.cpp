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
	if (_regex) {
		try {
			std::regex expr(_strValue);
			return std::regex_match(var.Value(), expr);
		} catch (const std::regex_error &) {
			return false;
		}
	} else {
		return _strValue == var.Value();
	}
}

bool MacroConditionVariable::ValueChanged(const Variable &var)
{
	bool changed = var.Value() != _lastValue;
	if (changed) {
		_lastValue = var.Value();
	}
	return changed;
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
	}

	return false;
}

bool MacroConditionVariable::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "variableName", _variableName.c_str());
	obs_data_set_string(obj, "strValue", _strValue.c_str());
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	obs_data_set_bool(obj, "regex", _regex);
	return true;
}

bool MacroConditionVariable::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_variableName = obs_data_get_string(obj, "variableName");
	_strValue = obs_data_get_string(obj, "strValue");
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Type>(obs_data_get_int(obj, "condition"));
	_regex = obs_data_get_bool(obj, "regex");
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
	  _conditions(new QComboBox()),
	  _strValue(new QLineEdit()),
	  _numValue(new QDoubleSpinBox()),
	  _regex(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.condition.variable.regex")))
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateConditionSelection(_conditions);

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(editingFinished()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(_numValue, SIGNAL(valueChanged(double)), this,
			 SLOT(NumValueChanged(double)));
	QWidget::connect(_regex, SIGNAL(stateChanged(int)), this,
			 SLOT(RegexChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables}, {"{{conditions}}", _conditions},
		{"{{strValue}}", _strValue},   {"{{numValue}}", _numValue},
		{"{{regex}}", _regex},
	};

	QHBoxLayout *layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.variable.entry"),
		layout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(layout);
	mainLayout->addWidget(_regex);
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
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setText(QString::fromStdString(_entryData->_strValue));
	_numValue->setValue(_entryData->_numValue);
	_regex->setChecked(_entryData->_regex);
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
	_entryData->_strValue = _strValue->text().toStdString();
}

void MacroConditionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_numValue = val;
}

void MacroConditionVariableEdit::RegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = state;
}

void MacroConditionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	switch (_entryData->_type) {
	case MacroConditionVariable::Type::EQUALS:
		_regex->show();
		_strValue->show();
		_numValue->hide();
		break;
	case MacroConditionVariable::Type::IS_EMPTY:
		_regex->hide();
		_strValue->hide();
		_numValue->hide();
		break;
	case MacroConditionVariable::Type::IS_NUMBER:
		_regex->hide();
		_strValue->hide();
		_numValue->hide();
		break;
	case MacroConditionVariable::Type::LESS_THAN:
		_regex->hide();
		_strValue->hide();
		_numValue->show();
		break;
	case MacroConditionVariable::Type::GREATER_THAN:
		_regex->hide();
		_strValue->hide();
		_numValue->show();
		break;
	case MacroConditionVariable::Type::VALUE_CHANGED:
		_regex->hide();
		_strValue->hide();
		_numValue->hide();
		break;
	}

	adjustSize();
	updateGeometry();
}
