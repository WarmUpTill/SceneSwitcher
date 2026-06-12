#include "macro-action-wait.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

#include <random>

namespace advss {

const std::string MacroActionWait::id = "wait";

bool MacroActionWait::_registered = MacroActionFactory::Register(
	MacroActionWait::id,
	{MacroActionWait::Create, MacroActionWaitEdit::Create,
	 "AdvSceneSwitcher.action.wait"});

static const std::map<MacroActionWait::Type, std::string> waitTypes = {
	{MacroActionWait::Type::FIXED,
	 "AdvSceneSwitcher.action.wait.type.fixed"},
	{MacroActionWait::Type::RANDOM,
	 "AdvSceneSwitcher.action.wait.type.random"},
	{MacroActionWait::Type::VARIABLE_WAIT,
	 "AdvSceneSwitcher.action.wait.type.variableCondition"},
};

static const std::map<MacroActionWait::Condition, std::string> conditionTypes = {
	{MacroActionWait::Condition::EQUALS,
	 "AdvSceneSwitcher.action.wait.condition.equals"},
	{MacroActionWait::Condition::DOES_NOT_EQUAL,
	 "AdvSceneSwitcher.action.wait.condition.doesNotEqual"},
	{MacroActionWait::Condition::IS_EMPTY,
	 "AdvSceneSwitcher.action.wait.condition.isEmpty"},
	{MacroActionWait::Condition::LESS_THAN,
	 "AdvSceneSwitcher.action.wait.condition.lessThan"},
	{MacroActionWait::Condition::GREATER_THAN,
	 "AdvSceneSwitcher.action.wait.condition.greaterThan"},
};

static std::random_device rd;
static std::default_random_engine re(rd());

static void waitHelper(std::unique_lock<std::mutex> *lock, Macro *macro,
		       std::chrono::high_resolution_clock::time_point &time)
{
	while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
		if (GetMacroWaitCV().wait_until(*lock, time) ==
		    std::cv_status::timeout) {
			break;
		}
	}
}

void MacroActionWait::SetupTempVars()
{
	MacroAction::SetupTempVars();
	if (_waitType != Type::VARIABLE_WAIT) {
		return;
	}
	AddTempvar(
		"timedOut",
		obs_module_text("AdvSceneSwitcher.tempVar.wait.timedOut"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.wait.timedOut.description"));
}

bool MacroActionWait::ConditionIsMet() const
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

bool MacroActionWait::PerformDurationWait()
{
	double sleepDuration;
	if (_waitType == Type::FIXED) {
		sleepDuration = _duration.Seconds();
	} else {
		double min = (_duration.Seconds() < _duration2.Seconds())
				     ? _duration.Seconds()
				     : _duration2.Seconds();
		double max = (_duration.Seconds() < _duration2.Seconds())
				     ? _duration2.Seconds()
				     : _duration.Seconds();
		std::uniform_real_distribution<double> unif(min, max);
		sleepDuration = unif(re);
	}
	vblog(LOG_INFO, "perform action wait with duration of %f",
	      sleepDuration);

	auto time = std::chrono::high_resolution_clock::now() +
		    std::chrono::milliseconds((int)(sleepDuration * 1000));

	SetMacroAbortWait(false);
	{
		SuspendLock suspendLock(*this);
		std::unique_lock<std::mutex> lock(*GetMutex());
		waitHelper(&lock, GetMacro(), time);
	}

	return !MacroWaitShouldAbort();
}

bool MacroActionWait::PerformVariableWait()
{
	auto var = _variable.lock();
	if (!var) {
		return true;
	}

	auto deadline =
		_useTimeout
			? std::chrono::high_resolution_clock::now() +
				  std::chrono::milliseconds((
					  int)(_conditionTimeout.Milliseconds()))
			: std::chrono::high_resolution_clock::time_point::max();

	SetMacroAbortWait(false);
	SuspendLock suspendLock(*this);
	auto cvLock = var->GetCVLock();

	while (!MacroWaitShouldAbort() && !MacroIsStopped(GetMacro())) {
		if (ConditionIsMet()) {
			SetTempVarValue("timedOut", false);
			return true;
		}
		auto now = std::chrono::high_resolution_clock::now();
		if (now >= deadline) {
			SetTempVarValue("timedOut", true);
			return true;
		}
		auto wakeAt =
			std::min(now + std::chrono::milliseconds(50), deadline);
		var->GetCV().wait_until(cvLock, wakeAt);
	}
	return !MacroWaitShouldAbort();
}

bool MacroActionWait::PerformAction()
{
	if (_waitType == Type::VARIABLE_WAIT) {
		return PerformVariableWait();
	}
	return PerformDurationWait();
}

bool MacroActionWait::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "waitType", static_cast<int>(_waitType));
	obs_data_set_int(obj, "version", 1);
	_duration.Save(obj);
	_duration2.Save(obj, "duration2");
	obs_data_set_string(obj, "variableName",
			    GetWeakVariableName(_variable).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_strValue.Save(obj, "strValue");
	_numValue.Save(obj, "numValue");
	_regex.Save(obj);
	obs_data_set_bool(obj, "useTimeout", _useTimeout);
	_conditionTimeout.Save(obj, "conditionTimeout");
	return true;
}

bool MacroActionWait::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_waitType = static_cast<Type>(obs_data_get_int(obj, "waitType"));
	// TODO: remove this fallback
	if (obs_data_get_int(obj, "version") == 1) {
		_duration2.Load(obj, "duration2");
	} else {
		_duration2.Load(obj, "seconds2");
		_duration2.SetUnit(static_cast<Duration::Unit>(
			obs_data_get_int(obj, "displayUnit2")));
	}
	_duration.Load(obj);
	_variable =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_strValue.Load(obj, "strValue");
	_numValue.Load(obj, "numValue");
	_regex.Load(obj);
	_useTimeout = obs_data_get_bool(obj, "useTimeout");
	_conditionTimeout.Load(obj, "conditionTimeout");
	return true;
}

std::string MacroActionWait::GetShortDesc() const
{
	if (_waitType == Type::VARIABLE_WAIT) {
		return GetWeakVariableName(_variable);
	}
	if (_waitType == Type::FIXED) {
		return _duration.ToString();
	}
	return _duration.ToString() + " - " + _duration2.ToString();
}

std::shared_ptr<MacroAction> MacroActionWait::Create(Macro *m)
{
	return std::make_shared<MacroActionWait>(m);
}

std::shared_ptr<MacroAction> MacroActionWait::Copy() const
{
	return std::make_shared<MacroActionWait>(*this);
}

void MacroActionWait::ResolveVariablesToFixedValues()
{
	_duration.ResolveVariables();
	_duration2.ResolveVariables();
	_strValue.ResolveVariables();
	_numValue.ResolveVariables();
	_conditionTimeout.ResolveVariables();
}

template<typename T>
static inline void populateSelection(QComboBox *list,
				     const std::map<T, std::string> &options)
{
	for (const auto &[value, name] : options) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroActionWaitEdit::MacroActionWaitEdit(
	QWidget *parent, std::shared_ptr<MacroActionWait> entryData)
	: QWidget(parent),
	  _duration(new DurationSelection()),
	  _duration2(new DurationSelection()),
	  _variable(new VariableSelection(this)),
	  _variableCondition(new QComboBox()),
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
	  _numValue(new VariableDoubleSpinBox()),
	  _regex(new RegexConfigWidget(parent)),
	  _useTimeout(new QCheckBox()),
	  _conditionTimeout(new DurationSelection()),
	  _waitType(new QComboBox()),
	  _mainLayout(new QHBoxLayout()),
	  _timeoutLayout(new QHBoxLayout())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	populateSelection(_waitType, waitTypes);
	populateSelection(_variableCondition, conditionTypes);

	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_duration2, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(Duration2Changed(const Duration &)));
	QWidget::connect(_waitType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_variable, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variableCondition, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionChanged(int)));
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
	QWidget::connect(_conditionTimeout,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(ConditionTimeoutChanged(const Duration &)));

	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.wait.layout.timeout"),
		_timeoutLayout,
		{
			{"{{useTimeout}}", _useTimeout},
			{"{{conditionTimeout}}", _conditionTimeout},
		});

	auto *outerLayout = new QVBoxLayout();
	outerLayout->addLayout(_mainLayout);
	outerLayout->addLayout(_timeoutLayout);
	setLayout(outerLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWaitEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_waitType->setCurrentIndex(static_cast<int>(_entryData->_waitType));

	switch (_entryData->_waitType) {
	case MacroActionWait::Type::FIXED:
		SetupFixedDurationEdit();
		break;
	case MacroActionWait::Type::RANDOM:
		SetupRandomDurationEdit();
		break;
	case MacroActionWait::Type::VARIABLE_WAIT:
		SetupVariableWaitEdit();
		break;
	}

	_duration->SetDuration(_entryData->_duration);
	_duration2->SetDuration(_entryData->_duration2);
	_variable->SetVariable(_entryData->_variable);
	_variableCondition->setCurrentIndex(
		static_cast<int>(_entryData->_condition));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->SetValue(_entryData->_numValue);
	_regex->SetRegexConfig(_entryData->_regex);
	_useTimeout->setChecked(_entryData->_useTimeout);
	_conditionTimeout->SetDuration(_entryData->_conditionTimeout);
}

void MacroActionWaitEdit::SetupFixedDurationEdit()
{
	_mainLayout->removeWidget(_duration);
	_mainLayout->removeWidget(_duration2);
	_mainLayout->removeWidget(_waitType);
	_mainLayout->removeWidget(_variable);
	_mainLayout->removeWidget(_variableCondition);
	_mainLayout->removeWidget(_strValue);
	_mainLayout->removeWidget(_numValue);
	_mainLayout->removeWidget(_regex);
	ClearLayout(_mainLayout);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.wait.layout.fixed"),
		_mainLayout,
		{{"{{duration}}", _duration}, {"{{waitType}}", _waitType}});
	_duration->show();
	_duration2->hide();
	_variable->hide();
	_variableCondition->hide();
	_strValue->hide();
	_numValue->hide();
	_regex->hide();
	SetLayoutVisible(_timeoutLayout, false);
}

void MacroActionWaitEdit::SetupRandomDurationEdit()
{
	_mainLayout->removeWidget(_duration);
	_mainLayout->removeWidget(_duration2);
	_mainLayout->removeWidget(_waitType);
	_mainLayout->removeWidget(_variable);
	_mainLayout->removeWidget(_variableCondition);
	_mainLayout->removeWidget(_strValue);
	_mainLayout->removeWidget(_numValue);
	_mainLayout->removeWidget(_regex);
	ClearLayout(_mainLayout);
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.wait.layout.random"),
		_mainLayout,
		{{"{{duration}}", _duration},
		 {"{{duration2}}", _duration2},
		 {"{{waitType}}", _waitType}});
	_duration->show();
	_duration2->show();
	_variable->hide();
	_variableCondition->hide();
	_strValue->hide();
	_numValue->hide();
	_regex->hide();
	SetLayoutVisible(_timeoutLayout, false);
}

void MacroActionWaitEdit::SetupVariableWaitEdit()
{
	_mainLayout->removeWidget(_duration);
	_mainLayout->removeWidget(_duration2);
	_mainLayout->removeWidget(_waitType);
	_mainLayout->removeWidget(_variable);
	_mainLayout->removeWidget(_variableCondition);
	_mainLayout->removeWidget(_strValue);
	_mainLayout->removeWidget(_numValue);
	_mainLayout->removeWidget(_regex);
	ClearLayout(_mainLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{waitType}}", _waitType},
		{"{{variable}}", _variable},
		{"{{condition}}", _variableCondition},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{regex}}", _regex},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.wait.layout.variableCondition"),
		_mainLayout, widgetPlaceholders);
	_duration->hide();
	_duration2->hide();
	_variable->show();
	_variableCondition->show();
	SetLayoutVisible(_timeoutLayout, true);
	SetVariableWaitWidgetVisibility();
}

void MacroActionWaitEdit::SetVariableWaitWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const auto cond = _entryData->_condition;
	const bool isStringComparison =
		cond == MacroActionWait::Condition::EQUALS ||
		cond == MacroActionWait::Condition::DOES_NOT_EQUAL;
	_strValue->setVisible(isStringComparison);
	_regex->setVisible(isStringComparison);
	_numValue->setVisible(cond == MacroActionWait::Condition::LESS_THAN ||
			      cond == MacroActionWait::Condition::GREATER_THAN);
	_conditionTimeout->setVisible(_entryData->_useTimeout);

	adjustSize();
	updateGeometry();
}

void MacroActionWaitEdit::TypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	auto type = static_cast<MacroActionWait::Type>(
		_waitType->itemData(idx).toInt());

	switch (type) {
	case MacroActionWait::Type::FIXED:
		SetupFixedDurationEdit();
		break;
	case MacroActionWait::Type::RANDOM:
		SetupRandomDurationEdit();
		break;
	case MacroActionWait::Type::VARIABLE_WAIT:
		SetupVariableWaitEdit();
		break;
	}

	_entryData->_waitType = type;
	_entryData->SetupTempVars();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWaitEdit::DurationChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = dur;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWaitEdit::Duration2Changed(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration2 = dur;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWaitEdit::VariableChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWaitEdit::ConditionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_condition = static_cast<MacroActionWait::Condition>(
		_variableCondition->itemData(idx).toInt());
	SetVariableWaitWidgetVisibility();
}

void MacroActionWaitEdit::StrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionWaitEdit::NumValueChanged(const NumberVariable<double> &val)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_numValue = val;
}

void MacroActionWaitEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroActionWaitEdit::UseTimeoutChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useTimeout = state != Qt::Unchecked;
	SetVariableWaitWidgetVisibility();
}

void MacroActionWaitEdit::ConditionTimeoutChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_conditionTimeout = dur;
}

} // namespace advss
