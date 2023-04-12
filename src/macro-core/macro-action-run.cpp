#include "macro-action-run.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <QProcess>
#include <QDesktopServices>

namespace advss {

const std::string MacroActionRun::id = "run";

bool MacroActionRun::_registered = MacroActionFactory::Register(
	MacroActionRun::id, {MacroActionRun::Create, MacroActionRunEdit::Create,
			     "AdvSceneSwitcher.action.run"});

bool MacroActionRun::PerformAction()
{
	bool procStarted = QProcess::startDetached(
		QString::fromStdString(_procConfig.Path()), _procConfig.Args(),
		QString::fromStdString(_procConfig.WorkingDir()));
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
	return true;
}

bool MacroActionRun::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_procConfig.Load(obj);
	return true;
}

std::string MacroActionRun::GetShortDesc() const
{
	return _procConfig.UnresolvedPath();
}

MacroActionRunEdit::MacroActionRunEdit(
	QWidget *parent, std::shared_ptr<MacroActionRun> entryData)
	: QWidget(parent), _procConfig(new ProcessConfigEdit(this))
{
	QWidget::connect(_procConfig,
			 SIGNAL(ConfigChanged(const ProcessConfig &)), this,
			 SLOT(ProcessConfigChanged(const ProcessConfig &)));

	auto *layout = new QVBoxLayout;
	layout->addWidget(_procConfig);
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
}

void MacroActionRunEdit::ProcessConfigChanged(const ProcessConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_procConfig = conf;
	adjustSize();
	updateGeometry();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
