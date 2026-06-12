#include "macro-action-wait-variable.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

// TODO:
// Merge with "wait" action?
// Expose temp var to indicate timeout or no timeout?

namespace advss {

const std::string MacroActionWaitVariable::id = "wait_variable";

bool MacroActionWaitVariable::_registered = MacroActionFactory::Register(
	MacroActionWaitVariable::id,
	{MacroActionWaitVariable::Create, MacroActionWaitVariableEdit::Create,
	 "AdvSceneSwitcher.action.waitVariable"});

static const std::map<MacroActionWaitVariable::Condition, std::string>
	conditionTypes = {
		{MacroActionWaitVariable::Condition::EQUALS,
		 "AdvSceneSwitcher.action.waitVariable.condition.equals"},
		{MacroActionWaitVariable::Condition::DOES_NOT_EQUAL,
		 "AdvSceneSwitcher.action.waitVariable.condition.doesNotEqual"},
		{MacroActionWaitVariable::Condition::IS_EMPTY,
		 "AdvSceneSwitcher.action.waitVariable.condition.isEmpty"},
		{MacroActionWaitVariable::Condition::LESS_THAN,
		 "AdvSceneSwitcher.action.waitVariable.condition.lessThan"},
		{MacroActionWaitVariable::Condition::GREATER_THAN,
		 "AdvSceneSwitcher.action.waitVariable.condition.greaterThan"},
};

bool MacroActionWaitVariable::ConditionIsMet() const
{
	auto var = _variable.lock();
	if (!var) {
		return false;
	}

	switch (_condition) {
	case Condition::EQUALS:
		if (_regex.Enabled()) {
			return _regex.Matches(var->Value(), _strValue);
		}
		return var->Value() == std::string(_strValue);
	case Condition::DOES_NOT_EQUAL:
		if (_regex.Enabled()) {
			return !_regex.Matches(var->Value(), _strValue);
		}
		return var->Value() != std::string(_strValue);
	case Condition::IS_EMPTY:
		return var->Value().empty();
	case Condition::LESS_THAN: {
		auto val = var->DoubleValue();
		return val.has_value() && *val < (double)_numValue;
	}
	case Condition::GREATER_THAN: {
		auto val = var->DoubleValue();
		return val.has_value() && *val > (double)_numValue;
	}
	}
	return false;
}

bool MacroActionWaitVariable::PerformAction()
{
	auto var = _variable.lock();
	if (!var) {
		return true;
	}

	const auto deadline =
		_useTimeout
			? std::chrono::high_resolution_clock::now() +
				  std::chrono::milliseconds(
					  (int)(_timeout.Milliseconds()))
			: std::chrono::high_resolution_clock::time_point::max();

	SetMacroAbortWait(false);
	SuspendLock suspendLock(*this);
	auto cvLock = var->GetCVLock();

	//while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
	//	if (GetMacroWaitCV().wait_until(*lock, time) ==
	//	    std::cv_status::timeout) {
	//		break;
	//	}
	//}

	while (!MacroWaitShouldAbort() && !MacroIsStopped(GetMacro())) {
		if (ConditionIsMet()) {
			return true;
		}
		const auto now = std::chrono::high_resolution_clock::now();
		if (now >= deadline) {
			return true;
		}

		const auto wakeAt =
			std::min(now + std::chrono::milliseconds(50), deadline);
		var->GetCV().wait_until(cvLock, wakeAt);
	}

	return true;
}

bool MacroActionWaitVariable::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "variableName",
			    GetWeakVariableName(_variable).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_strValue.Save(obj, "strValue");
	_numValue.Save(obj, "numValue");
	_regex.Save(obj);
	obs_data_set_bool(obj, "useTimeout", _useTimeout);
	_timeout.Save(obj, "timeout");
	return true;
}

bool MacroActionWaitVariable::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_variable =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_strValue.Load(obj, "strValue");
	_numValue.Load(obj, "numValue");
	_regex.Load(obj);
	_useTimeout = obs_data_get_bool(obj, "useTimeout");
	_timeout.Load(obj, "timeout");
	return true;
}

std::string MacroActionWaitVariable::GetShortDesc() const
{
	return GetWeakVariableName(_variable);
}

std::shared_ptr<MacroAction> MacroActionWaitVariable::Create(Macro *m)
{
	return std::make_shared<MacroActionWaitVariable>(m);
}

std::shared_ptr<MacroAction> MacroActionWaitVariable::Copy() const
{
	return std::make_shared<MacroActionWaitVariable>(*this);
}

void MacroActionWaitVariable::ResolveVariablesToFixedValues()
{
	_strValue.ResolveVariables();
	_numValue.ResolveVariables();
	_timeout.ResolveVariables();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionWaitVariableEdit::MacroActionWaitVariableEdit(
	QWidget *parent, std::shared_ptr<MacroActionWaitVariable> entryData)
	: QWidget(parent),
	  _variable(new VariableSelection(this)),
	  _condition(new QComboBox()),
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
	  _numValue(new VariableDoubleSpinBox()),
	  _regex(new RegexConfigWidget(parent)),
	  _useTimeout(new QCheckBox()),
	  _timeout(new DurationSelection()),
	  _mainLayout(new QHBoxLayout()),
	  _timeoutLayout(new QHBoxLayout())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateConditionSelection(_condition);

	QWidget::connect(_variable, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(
		_numValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(NumValueChanged(const NumberVariable<double> &)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_useTimeout, SIGNAL(stateChanged(int)), this,
			 SLOT(UseTimeoutChanged(int)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(TimeoutChanged(const Duration &)));

	std::unordered_map<std::string, QWidget *> mainPlaceholders = {
		{"{{variable}}", _variable}, {"{{condition}}", _condition},
		{"{{strValue}}", _strValue}, {"{{numValue}}", _numValue},
		{"{{regex}}", _regex},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.waitVariable.layout.main"),
		_mainLayout, mainPlaceholders);

	std::unordered_map<std::string, QWidget *> timeoutPlaceholders = {
		{"{{useTimeout}}", _useTimeout},
		{"{{timeout}}", _timeout},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.waitVariable.layout.timeout"),
		_timeoutLayout, timeoutPlaceholders);

	auto *layout = new QVBoxLayout();
	layout->addLayout(_mainLayout);
	layout->addLayout(_timeoutLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWaitVariableEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_variable->SetVariable(_entryData->_variable);
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->SetValue(_entryData->_numValue);
	_regex->SetRegexConfig(_entryData->_regex);
	_useTimeout->setChecked(_entryData->_useTimeout);
	_timeout->SetDuration(_entryData->_timeout);
	SetWidgetVisibility();
}

void MacroActionWaitVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const auto cond = _entryData->_condition;
	const bool isStringComparison =
		cond == MacroActionWaitVariable::Condition::EQUALS ||
		cond == MacroActionWaitVariable::Condition::DOES_NOT_EQUAL;
	_strValue->setVisible(isStringComparison);
	_regex->setVisible(isStringComparison);
	_numValue->setVisible(
		cond == MacroActionWaitVariable::Condition::LESS_THAN ||
		cond == MacroActionWaitVariable::Condition::GREATER_THAN);
	_timeout->setVisible(_entryData->_useTimeout);

	adjustSize();
	updateGeometry();
}

void MacroActionWaitVariableEdit::VariableChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWaitVariableEdit::ConditionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_condition =
		static_cast<MacroActionWaitVariable::Condition>(value);
	SetWidgetVisibility();
}

void MacroActionWaitVariableEdit::StrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionWaitVariableEdit::NumValueChanged(
	const NumberVariable<double> &val)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_numValue = val;
}

void MacroActionWaitVariableEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroActionWaitVariableEdit::UseTimeoutChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useTimeout = state != Qt::Unchecked;
	SetWidgetVisibility();
}

void MacroActionWaitVariableEdit::TimeoutChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_timeout = dur;
}

} // namespace advss
