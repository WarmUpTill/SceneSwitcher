#include "macro-condition-run.hpp"
#include "layout-helpers.hpp"

#include <QProcess>

namespace advss {

const std::string MacroConditionRun::id = "run";

bool MacroConditionRun::_registered = MacroConditionFactory::Register(
	MacroConditionRun::id,
	{MacroConditionRun::Create, MacroConditionRunEdit::Create,
	 "AdvSceneSwitcher.condition.run"});

MacroConditionRun::~MacroConditionRun()
{
	if (_thread.joinable()) {
		_thread.join();
	}
}

bool MacroConditionRun::CheckCondition()
{
	if (!_threadDone) {
		return false;
	}

	bool ret = false;

	switch (_error) {
	case ProcessConfig::ProcStartError::FAILED_TO_START:
		SetVariableValue("Failed to start process");
		break;
	case ProcessConfig::ProcStartError::TIMEOUT:
		SetVariableValue("Timeout while running process");
		break;
	case ProcessConfig::ProcStartError::CRASH:
		SetVariableValue("Timeout while running process");
		break;
	case ProcessConfig::ProcStartError::NONE:
		ret = _checkExitCode ? _exitCodeToCheck == _procExitCode : true;
		SetVariableValue(std::to_string(_procExitCode));
		break;
	default:
		break;
	}

	if (_thread.joinable()) {
		_thread.join();
	}

	_threadDone = false;
	_thread = std::thread(&MacroConditionRun::RunProcess, this);

	return ret;
}

void MacroConditionRun::RunProcess()
{
	auto result = _procConfig.StartProcessAndWait(_timeout.Milliseconds());

	if (std::holds_alternative<ProcessConfig::ProcStartError>(result)) {
		_error = std::get<ProcessConfig::ProcStartError>(result);
	} else {
		_error = ProcessConfig::ProcStartError::NONE;
		_procExitCode = std::get<int>(result);
	}

	_threadDone = true;
	SetTempVarValues();
}

void MacroConditionRun::SetupTempVars()
{
	MacroCondition::SetupTempVars();

	AddTempvar(
		"process.id",
		obs_module_text("AdvSceneSwitcher.tempVar.run.process.id"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.id.description"));
	AddTempvar(
		"process.exitCode",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.exitCode"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.exitCode.description"));
	AddTempvar(
		"process.stream.output",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.stream.output"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.stream.output.description"));
	AddTempvar(
		"process.stream.error",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.stream.error"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.run.process.stream.error.description"));
}

void MacroConditionRun::SetTempVarValues()
{
	SetTempVarValue("process.id", _procConfig.GetProcessId());
	SetTempVarValue("process.exitCode", _procConfig.GetProcessExitCode());
	SetTempVarValue("process.stream.output",
			_procConfig.GetProcessOutputStream());
	SetTempVarValue("process.stream.error",
			_procConfig.GetProcessErrorStream());
}

bool MacroConditionRun::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_procConfig.Save(obj);
	obs_data_set_bool(obj, "checkExitCode", _checkExitCode);
	obs_data_set_int(obj, "exitCode", _exitCodeToCheck);
	_timeout.Save(obj, "timeout");
	return true;
}

bool MacroConditionRun::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_procConfig.Load(obj);
	_checkExitCode = obs_data_get_bool(obj, "checkExitCode");
	_exitCodeToCheck = obs_data_get_int(obj, "exitCode");
	_timeout.Load(obj, "timeout");
	return true;
}

std::string MacroConditionRun::GetShortDesc() const
{
	return _procConfig.UnresolvedPath();
}

MacroConditionRunEdit::MacroConditionRunEdit(
	QWidget *parent, std::shared_ptr<MacroConditionRun> entryData)
	: QWidget(parent),
	  _procConfig(new ProcessConfigEdit(this)),
	  _checkExitCode(new QCheckBox()),
	  _exitCode(new QSpinBox()),
	  _timeout(new DurationSelection(this, false, 0.1))
{
	_exitCode->setMinimum(-99999);
	_exitCode->setMaximum(999999);

	QWidget::connect(_procConfig,
			 SIGNAL(ConfigChanged(const ProcessConfig &)), this,
			 SLOT(ProcessConfigChanged(const ProcessConfig &)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(TimeoutChanged(const Duration &)));
	QWidget::connect(_checkExitCode, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckExitCodeChanged(int)));
	QWidget::connect(_exitCode, SIGNAL(valueChanged(int)), this,
			 SLOT(ExitCodeChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkExitCode}}", _checkExitCode},
		{"{{exitCode}}", _exitCode},
		{"{{timeout}}", _timeout},
	};

	auto exitLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.run.entry.exit"),
		exitLayout, widgetPlaceholders);
	auto timeoutLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.run.entry"),
		     timeoutLayout, widgetPlaceholders);

	auto *layout = new QVBoxLayout;
	layout->addLayout(timeoutLayout);
	layout->addWidget(_procConfig);
	layout->addLayout(exitLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionRunEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_procConfig->SetProcessConfig(_entryData->_procConfig);
	_timeout->SetDuration(_entryData->_timeout);
	_checkExitCode->setChecked(_entryData->_checkExitCode);
	_exitCode->setValue(_entryData->_exitCodeToCheck);
}

void MacroConditionRunEdit::TimeoutChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_timeout = dur;
}

void MacroConditionRunEdit::CheckExitCodeChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_checkExitCode = state;
}

void MacroConditionRunEdit::ExitCodeChanged(int exitCode)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_exitCodeToCheck = exitCode;
}

void MacroConditionRunEdit::ProcessConfigChanged(const ProcessConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_procConfig = conf;
	adjustSize();
	updateGeometry();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
