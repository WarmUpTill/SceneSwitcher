#include "process-config.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "name-dialog.hpp"

#include <QFileDialog>

namespace advss {

bool ProcessConfig::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	_path.Save(data, "path");
	_workingDirectory.Save(data, "workingDirectory");
	_args.Save(data, "args", "arg");
	obs_data_set_obj(obj, "processConfig", data);
	obs_data_release(data);

	return true;
}

bool ProcessConfig::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "processConfig")) {
		_path = obs_data_get_string(obj, "path");
		_workingDirectory =
			obs_data_get_string(obj, "workingDirectory");
		_args.Load(obj, "args", "arg");

		return true;
	}

	auto data = obs_data_get_obj(obj, "processConfig");
	_path.Load(data, "path");
	_workingDirectory.Load(data, "workingDirectory");
	_args.Load(data, "args", "arg");
	obs_data_release(data);

	return true;
}

QStringList ProcessConfig::Args() const
{
	QStringList result;
	for (auto &arg : _args) {
		result << QString::fromStdString(arg);
	}

	return result;
}

bool ProcessConfig::StartProcessDetached() const
{
	return QProcess::startDetached(QString::fromStdString(Path()), Args(),
				       QString::fromStdString(WorkingDir()));
}

void ProcessConfig::ResolveVariables()
{
	_path.ResolveVariables();
	_workingDirectory.ResolveVariables();
	_args.ResolveVariables();
}

std::variant<int, ProcessConfig::ProcStartError>
ProcessConfig::StartProcessAndWait(int timeout)
{
	ResetFinishedProcessData();

	QProcess process;
	process.setWorkingDirectory(QString::fromStdString(WorkingDir()));
	process.start(QString::fromStdString(Path()), Args());
	SetProcessId(QString::number(process.processId()).toStdString());
	vblog(LOG_INFO, "run \"%s\" with a timeout of %d ms", Path().c_str(),
	      timeout);

	if (!process.waitForFinished(timeout)) {
		if (process.error() == QProcess::FailedToStart) {
			vblog(LOG_INFO, "failed to start \"%s\"!",
			      Path().c_str());
			return ProcStartError::FAILED_TO_START;
		}

		SetFinishedProcessData(process);
		vblog(LOG_INFO,
		      "timeout while running \"%s\"\nAttempting to kill process!",
		      Path().c_str());
		process.kill();
		process.waitForFinished();

		return ProcStartError::TIMEOUT;
	}

	SetFinishedProcessData(process);

	if (process.exitStatus() == QProcess::NormalExit) {
		return process.exitCode();
	}

	vblog(LOG_INFO, "process \"%s\" crashed!", Path().c_str());
	return ProcStartError::CRASH;
}

void ProcessConfig::SetFinishedProcessData(QProcess &process)
{
	static const QRegularExpression regex("(\\r\\n|\\r|\\n)$");
	_processExitCode = QString::number(process.exitCode()).toStdString();
	// Qt reads extra newline, at least on Windows, hence the workaround
	_processOutputStream = QString(process.readAllStandardOutput())
				       .remove(regex)
				       .toStdString();
	_processErrorStream = QString(process.readAllStandardError())
				      .remove(regex)
				      .toStdString();
}

void ProcessConfig::ResetFinishedProcessData()
{
	_processExitCode = "";
	_processOutputStream = "";
	_processErrorStream = "";
}

ProcessConfigEdit::ProcessConfigEdit(QWidget *parent)
	: QWidget(parent),
	  _filePath(new FileSelection()),
	  _showAdvancedSettings(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.process.showAdvanced"))),
	  _advancedSettingsLayout(new QVBoxLayout()),
	  _argList(new StringListEdit(
		  this, obs_module_text("AdvSceneSwitcher.process.addArgument"),
		  obs_module_text(
			  "AdvSceneSwitcher.process.addArgumentDescription"),
		  4096, true)),
	  _workingDirectory(new FileSelection(FileSelection::Type::FOLDER))
{
	_advancedSettingsLayout->setContentsMargins(0, 0, 0, 0);

	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_showAdvancedSettings, SIGNAL(clicked()), this,
			 SLOT(ShowAdvancedSettingsClicked()));
	QWidget::connect(_argList,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(ArgsChanged(const StringList &)));
	QWidget::connect(_workingDirectory,
			 SIGNAL(PathChanged(const QString &)), this,
			 SLOT(WorkingDirectoryChanged(const QString &)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{filePath}}", _filePath},
		{"{{workingDirectory}}", _workingDirectory},
		{"{{advancedSettings}}", _showAdvancedSettings},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.process.entry"),
		     entryLayout, widgetPlaceholders, false);

	auto workingDirectoryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.process.entry.workingDirectory"),
		     workingDirectoryLayout, widgetPlaceholders, false);

	_advancedSettingsLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.process.arguments")));
	_advancedSettingsLayout->addWidget(_argList);
	_advancedSettingsLayout->addLayout(workingDirectoryLayout);

	auto mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_advancedSettingsLayout);
	setLayout(mainLayout);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void ProcessConfigEdit::SetProcessConfig(const ProcessConfig &conf)
{
	_conf = conf;
	_filePath->SetPath(conf._path);
	_argList->SetStringList(conf._args);
	_workingDirectory->SetPath(conf._workingDirectory);
	ShowAdvancedSettings(
		!_conf._args.empty() ||
		!_conf._workingDirectory.UnresolvedValue().empty());
}

void ProcessConfigEdit::PathChanged(const QString &text)
{
	_conf._path = text.toStdString();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ShowAdvancedSettingsClicked()
{
	ShowAdvancedSettings(true);
	emit ConfigChanged(_conf); // Just to make sure resizing is handled
}

void ProcessConfigEdit::WorkingDirectoryChanged(const QString &path)
{
	_conf._workingDirectory = path.toStdString();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ArgsChanged(const StringList &args)
{
	_conf._args = args;
	adjustSize();
	updateGeometry();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ShowAdvancedSettings(bool showAdvancedSettings)
{
	SetLayoutVisible(_advancedSettingsLayout, showAdvancedSettings);
	_showAdvancedSettings->setVisible(!showAdvancedSettings);
	adjustSize();
	updateGeometry();
	if (showAdvancedSettings) {
		emit AdvancedSettingsEnabled();
	}
}

} // namespace advss
