#include "macro-condition-run.hpp"
#include "utility.hpp"

#include <QProcess>
#include <QDesktopServices>

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

	switch (_procStatus) {
	case MacroConditionRun::Status::FAILED_TO_START:
		SetVariableValue("Failed to start process");
		ret = false;
		break;
	case MacroConditionRun::Status::TIMEOUT:
		SetVariableValue("Timeout while running process");
		ret = false;
		break;
	case MacroConditionRun::Status::OK:
		ret = _checkExitCode ? _exitCode == _procExitCode : true;
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
	QProcess process;
	process.setWorkingDirectory(
		QString::fromStdString(_procConfig.WorkingDir()));
	process.start(QString::fromStdString(_procConfig.Path()),
		      _procConfig.Args());
	int timeout = _timeout.Milliseconds();

	vblog(LOG_INFO, "run \"%s\" with a timeout of %d ms",
	      _procConfig.Path().c_str(), timeout);

	bool procFinishedInTime = process.waitForFinished(timeout);

	if (!procFinishedInTime) {
		if (process.error() == QProcess::FailedToStart) {
			vblog(LOG_INFO, "failed to start \"%s\"!",
			      _procConfig.Path().c_str());
			_procStatus = Status::FAILED_TO_START;
		} else {
			vblog(LOG_INFO,
			      "timeout while running \"%s\"\nAttempting to kill process!",
			      _procConfig.Path().c_str());
			process.kill();
			process.waitForFinished();
			_procStatus = Status::TIMEOUT;
		}
	}

	bool validExitCode = process.exitStatus() == QProcess::NormalExit;

	if ((_checkExitCode && !validExitCode) || !procFinishedInTime) {
		_threadDone = true;
		return;
	}

	_procExitCode = process.exitCode();
	_procStatus = Status::OK;
	_threadDone = true;
}

bool MacroConditionRun::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_procConfig.Save(obj);
	obs_data_set_bool(obj, "checkExitCode", _checkExitCode);
	obs_data_set_int(obj, "exitCode", _exitCode);
	_timeout.Save(obj, "timeout");
	return true;
}

bool MacroConditionRun::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_procConfig.Load(obj);
	_checkExitCode = obs_data_get_bool(obj, "checkExitCode");
	_exitCode = obs_data_get_int(obj, "exitCode");
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
	_exitCode->setValue(_entryData->_exitCode);
}

void MacroConditionRunEdit::TimeoutChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_timeout = dur;
}

void MacroConditionRunEdit::CheckExitCodeChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_checkExitCode = state;
}

void MacroConditionRunEdit::ExitCodeChanged(int exitCode)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_exitCode = exitCode;
}

void MacroConditionRunEdit::ProcessConfigChanged(const ProcessConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_procConfig = conf;
	adjustSize();
	updateGeometry();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
