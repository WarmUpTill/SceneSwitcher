#include "process-config.hpp"
#include "name-dialog.hpp"
#include "utility.hpp"

#include <QProcess>
#include <QFileDialog>

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

QStringList ProcessConfig::Args()
{
	QStringList result;
	for (auto &arg : _args) {
		result << QString::fromStdString(arg);
	}
	return result;
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
			  "AdvSceneSwitcher.process.addArgumentDescription"))),
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
	placeWidgets(obs_module_text("AdvSceneSwitcher.process.entry"),
		     entryLayout, widgetPlaceholders, false);

	auto workingDirectoryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.process.entry.workingDirectory"),
		     workingDirectoryLayout, widgetPlaceholders, false);

	_advancedSettingsLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.process.arguments")));
	_advancedSettingsLayout->addWidget(_argList);
	_advancedSettingsLayout->addLayout(workingDirectoryLayout);

	auto *mainLayout = new QVBoxLayout;
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
	setLayoutVisible(_advancedSettingsLayout, showAdvancedSettings);
	_showAdvancedSettings->setVisible(!showAdvancedSettings);
	adjustSize();
	updateGeometry();
}
