#include "macro-action-run.hpp"
#include "layout-helpers.hpp"

#include <QProcess>
#include <QDesktopServices>

namespace advss {

const std::string MacroActionRun::id = "run";

bool MacroActionRun::_registered = MacroActionFactory::Register(
	MacroActionRun::id, {MacroActionRun::Create, MacroActionRunEdit::Create,
			     "AdvSceneSwitcher.action.run"});

bool MacroActionRun::PerformAction()
{
	if (_wait) {
		_procConfig.StartProcessAndWait(_timeout.Milliseconds());
		return true;
	}

	bool procStarted = _procConfig.StartProcessDetached();

	// Fall back to using default application to open given file
	if (!procStarted && _procConfig.Args().empty()) {
		vblog(LOG_INFO, "run \"%s\" using QDesktopServices",
		      _procConfig.Path().c_str());
		QDesktopServices::openUrl(QUrl::fromLocalFile(
			QString::fromStdString(_procConfig.Path())));
	}
	return true;
}

void MacroActionRun::LogAction() const
{
	vblog(LOG_INFO, "run \"%s\"", _procConfig.UnresolvedPath().c_str());
}

bool MacroActionRun::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_procConfig.Save(obj);
	_timeout.Save(obj);
	obs_data_set_bool(obj, "wait", _wait);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionRun::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_procConfig.Load(obj);
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "version")) {
		return true;
	}
	_timeout.Load(obj);
	_wait = obs_data_get_bool(obj, "wait");
	return true;
}

std::string MacroActionRun::GetShortDesc() const
{
	return _procConfig.UnresolvedPath();
}

std::shared_ptr<MacroAction> MacroActionRun::Create(Macro *m)
{
	return std::make_shared<MacroActionRun>(m);
}

std::shared_ptr<MacroAction> MacroActionRun::Copy() const
{
	return std::make_shared<MacroActionRun>(*this);
}

void MacroActionRun::ResolveVariablesToFixedValues()
{
	_procConfig.ResolveVariables();
	_timeout.ResolveVariables();
}

MacroActionRunEdit::MacroActionRunEdit(
	QWidget *parent, std::shared_ptr<MacroActionRun> entryData)
	: QWidget(parent),
	  _procConfig(new ProcessConfigEdit(this)),
	  _waitLayout(new QHBoxLayout()),
	  _wait(new QCheckBox()),
	  _timeout(new DurationSelection(this, true, 0.1))
{
	QWidget::connect(_procConfig,
			 SIGNAL(ConfigChanged(const ProcessConfig &)), this,
			 SLOT(ProcessConfigChanged(const ProcessConfig &)));
	QWidget::connect(_procConfig, SIGNAL(AdvancedSettingsEnabled()), this,
			 SLOT(ProcessConfigAdvancedSettingsShown()));
	QWidget::connect(_wait, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitChanged(int)));
	QWidget::connect(_timeout, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(TimeoutChanged(const Duration &)));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.run.entry.wait"),
		     _waitLayout,
		     {{"{{wait}}", _wait}, {"{{timeout}}", _timeout}});
	SetLayoutVisible(_waitLayout, false);

	auto layout = new QVBoxLayout;
	layout->addWidget(_procConfig);
	layout->addLayout(_waitLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionRunEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_procConfig->SetProcessConfig(_entryData->_procConfig);
	_wait->setChecked(_entryData->_wait);
	_timeout->SetDuration(_entryData->_timeout);
}

void MacroActionRunEdit::ProcessConfigAdvancedSettingsShown()
{
	SetLayoutVisible(_waitLayout, true);
}

void MacroActionRunEdit::WaitChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}
	auto lock = LockContext();
	_entryData->_wait = value;
}

void MacroActionRunEdit::TimeoutChanged(const Duration &timeout)
{
	if (_loading || !_entryData) {
		return;
	}
	auto lock = LockContext();
	_entryData->_timeout = timeout;
}

void MacroActionRunEdit::ProcessConfigChanged(const ProcessConfig &conf)
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
