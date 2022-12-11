#include "macro-action-variable.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionVariable::id = "variable";

bool MacroActionVariable::_registered = MacroActionFactory::Register(
	MacroActionVariable::id,
	{MacroActionVariable::Create, MacroActionVariableEdit::Create,
	 "AdvSceneSwitcher.action.variable"});

static std::map<MacroActionVariable::Type, std::string> waitTypes = {
	{MacroActionVariable::Type::SET,
	 "AdvSceneSwitcher.action.variable.type.set"},
	{MacroActionVariable::Type::APPEND,
	 "AdvSceneSwitcher.action.variable.type.append"},
	{MacroActionVariable::Type::APPEND_VAR,
	 "AdvSceneSwitcher.action.variable.type.appendVar"},
	{MacroActionVariable::Type::INCREMENT,
	 "AdvSceneSwitcher.action.variable.type.increment"},
	{MacroActionVariable::Type::DECREMENT,
	 "AdvSceneSwitcher.action.variable.type.decrement"},
};

static void apppend(Variable &var, const std::string &value)
{
	auto currentValue = var.Value();
	var.SetValue(currentValue + value);
}

static void modifyNumValue(Variable &var, double val, const bool increment)
{
	double current;
	if (!var.DoubleValue(current)) {
		return;
	}
	if (increment) {
		var.SetValue(current + val);
	} else {
		var.SetValue(current - val);
	}
}

bool MacroActionVariable::PerformAction()
{
	auto var = GetVariableByName(_variableName);
	if (!var) {
		return true;
	}

	switch (_type) {
	case MacroActionVariable::Type::SET:
		var->SetValue(_strValue);
		break;
	case MacroActionVariable::Type::APPEND:
		apppend(*var, _strValue);
		break;
	case MacroActionVariable::Type::APPEND_VAR: {
		auto var2 = GetVariableByName(_variable2Name);
		if (!var2) {
			return true;
		}
		apppend(*var, var2->Value());
		break;
	}
	case MacroActionVariable::Type::INCREMENT:
		modifyNumValue(*var, _numValue, true);
		break;
	case MacroActionVariable::Type::DECREMENT:
		modifyNumValue(*var, _numValue, false);
		break;
	}

	return true;
}

bool MacroActionVariable::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "variableName", _variableName.c_str());
	obs_data_set_string(obj, "variable2Name", _variable2Name.c_str());
	obs_data_set_string(obj, "strValue", _strValue.c_str());
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	return true;
}

bool MacroActionVariable::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_variableName = obs_data_get_string(obj, "variableName");
	_variable2Name = obs_data_get_string(obj, "variable2Name");
	_strValue = obs_data_get_string(obj, "strValue");
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Type>(obs_data_get_int(obj, "condition"));
	return true;
}

std::string MacroActionVariable::GetShortDesc() const
{
	return _variableName;
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : waitTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionVariableEdit::MacroActionVariableEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: QWidget(parent),
	  _variables(new VariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _actions(new QComboBox()),
	  _strValue(new QLineEdit()),
	  _numValue(new QDoubleSpinBox())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateTypeSelection(_actions);

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(editingFinished()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(_numValue, SIGNAL(valueChanged(double)), this,
			 SLOT(NumValueChanged(double)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables}, {"{{variables2}}", _variables2},
		{"{{actions}}", _actions},     {"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
	};

	auto mainLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.variable.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionVariableEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_variables->SetVariable(_entryData->_variableName);
	_variables2->SetVariable(_entryData->_variable2Name);
	_actions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setText(QString::fromStdString(_entryData->_strValue));
	_numValue->setValue(_entryData->_numValue);
	SetWidgetVisibility();
}

void MacroActionVariableEdit::VariableChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variableName = text.toStdString();
}

void MacroActionVariableEdit::Variable2Changed(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variable2Name = text.toStdString();
}

void MacroActionVariableEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroActionVariable::Type>(value);
	SetWidgetVisibility();
}

void MacroActionVariableEdit::StrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_strValue = _strValue->text().toStdString();
}

void MacroActionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_numValue = val;
}

void MacroActionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	auto type = _entryData->_type;
	switch (type) {
	case MacroActionVariable::Type::SET:
		_variables2->hide();
		_strValue->show();
		_numValue->hide();
		break;
	case MacroActionVariable::Type::APPEND:
		_variables2->hide();
		_strValue->show();
		_numValue->hide();
		break;
	case MacroActionVariable::Type::APPEND_VAR:
		_variables2->show();
		_strValue->hide();
		_numValue->hide();
		break;
	case MacroActionVariable::Type::INCREMENT:
		_variables2->hide();
		_strValue->hide();
		_numValue->show();
		break;
	case MacroActionVariable::Type::DECREMENT:
		_variables2->hide();
		_strValue->hide();
		_numValue->show();
		break;
	}

	adjustSize();
	updateGeometry();
}
