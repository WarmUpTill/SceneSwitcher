#include "process-config.hpp"
#include "name-dialog.hpp"
#include "utility.hpp"

#include <QProcess>
#include <QFileDialog>

bool ProcessConfig::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "path", _path.c_str());
	obs_data_set_string(data, "workingDirectory",
			    _workingDirectory.c_str());
	obs_data_array_t *args = obs_data_array_create();
	for (auto &arg : _args) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "arg",
				    arg.toStdString().c_str());
		obs_data_array_push_back(args, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(data, "args", args);
	obs_data_array_release(args);

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
		obs_data_array_t *args = obs_data_get_array(obj, "args");
		size_t count = obs_data_array_count(args);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(args, i);
			_args << QString::fromStdString(
				obs_data_get_string(array_obj, "arg"));
			obs_data_release(array_obj);
		}
		obs_data_array_release(args);
		return true;
	}

	auto data = obs_data_get_obj(obj, "processConfig");
	_path = obs_data_get_string(data, "path");
	_workingDirectory = obs_data_get_string(data, "workingDirectory");
	obs_data_array_t *args = obs_data_get_array(data, "args");
	size_t count = obs_data_array_count(args);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(args, i);
		_args << QString::fromStdString(
			obs_data_get_string(array_obj, "arg"));
		obs_data_release(array_obj);
	}
	obs_data_array_release(args);
	obs_data_release(data);
	return true;
}

ProcessConfigEdit::ProcessConfigEdit(QWidget *parent)
	: QWidget(parent),
	  _filePath(new FileSelection()),
	  _showAdvancedSettings(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.process.showAdvanced"))),
	  _advancedSettingsLayout(new QVBoxLayout()),
	  _argList(new QListWidget()),
	  _addArg(new QPushButton()),
	  _removeArg(new QPushButton()),
	  _argUp(new QPushButton()),
	  _argDown(new QPushButton()),
	  _workingDirectory(new FileSelection(FileSelection::Type::FOLDER))
{
	_advancedSettingsLayout->setContentsMargins(0, 0, 0, 0);

	_addArg->setMaximumWidth(22);
	_addArg->setProperty("themeID",
			     QVariant(QString::fromUtf8("addIconSmall")));
	_addArg->setFlat(true);
	_removeArg->setMaximumWidth(22);
	_removeArg->setProperty("themeID",
				QVariant(QString::fromUtf8("removeIconSmall")));
	_removeArg->setFlat(true);
	_argUp->setMaximumWidth(22);
	_argUp->setProperty("themeID",
			    QVariant(QString::fromUtf8("upArrowIconSmall")));
	_argUp->setFlat(true);
	_argDown->setMaximumWidth(22);
	_argDown->setProperty(
		"themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	_argDown->setFlat(true);

	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_showAdvancedSettings, SIGNAL(clicked()), this,
			 SLOT(ShowAdvancedSettingsClicked()));
	QWidget::connect(_addArg, SIGNAL(clicked()), this, SLOT(AddArg()));
	QWidget::connect(_removeArg, SIGNAL(clicked()), this,
			 SLOT(RemoveArg()));
	QWidget::connect(_argUp, SIGNAL(clicked()), this, SLOT(ArgUp()));
	QWidget::connect(_argDown, SIGNAL(clicked()), this, SLOT(ArgDown()));
	QWidget::connect(_argList, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
			 this, SLOT(ArgItemClicked(QListWidgetItem *)));
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

	auto argButtonLayout = new QHBoxLayout;
	argButtonLayout->addWidget(_addArg);
	argButtonLayout->addWidget(_removeArg);
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	argButtonLayout->addWidget(line);
	argButtonLayout->addWidget(_argUp);
	argButtonLayout->addWidget(_argDown);
	argButtonLayout->addStretch();

	auto workingDirectoryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.process.entry.workingDirectory"),
		     workingDirectoryLayout, widgetPlaceholders, false);

	_advancedSettingsLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.process.arguments")));
	_advancedSettingsLayout->addWidget(_argList);
	_advancedSettingsLayout->addLayout(argButtonLayout);
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
	_filePath->SetPath(QString::fromStdString(conf._path));
	for (const auto &arg : conf._args) {
		QListWidgetItem *item = new QListWidgetItem(arg, _argList);
		item->setData(Qt::UserRole, arg);
	}
	_workingDirectory->SetPath(
		QString::fromStdString(conf._workingDirectory));
	SetArgListSize();
	ShowAdvancedSettings(!_conf._args.empty() ||
			     !_conf._workingDirectory.empty());
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

void ProcessConfigEdit::AddArg()
{
	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.process.addArgument"),
		obs_module_text(
			"AdvSceneSwitcher.process.addArgumentDescription"),
		name, "", 170, false);

	if (!accepted || name.empty()) {
		return;
	}
	auto arg = QString::fromStdString(name);
	QVariant v = QVariant::fromValue(arg);
	QListWidgetItem *item = new QListWidgetItem(arg, _argList);
	item->setData(Qt::UserRole, arg);

	_conf._args << arg;
	SetArgListSize();

	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::RemoveArg()
{
	int idx = _argList->currentRow();
	if (idx == -1) {
		return;
	}
	_conf._args.removeAt(idx);

	QListWidgetItem *item = _argList->currentItem();
	if (!item) {
		return;
	}
	delete item;
	SetArgListSize();

	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ArgUp()
{
	int idx = _argList->currentRow();
	if (idx != -1 && idx != 0) {
		_argList->insertItem(idx - 1, _argList->takeItem(idx));
		_argList->setCurrentRow(idx - 1);

		_conf._args.move(idx, idx - 1);
	}
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ArgDown()
{
	int idx = _argList->currentRow();
	if (idx != -1 && idx != _argList->count() - 1) {
		_argList->insertItem(idx + 1, _argList->takeItem(idx));
		_argList->setCurrentRow(idx + 1);

		_conf._args.move(idx, idx + 1);
	}
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ArgItemClicked(QListWidgetItem *item)
{
	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.process.addArgument"),
		obs_module_text(
			"AdvSceneSwitcher.process.addArgumentDescription"),
		name, item->text(), 170, false);

	if (!accepted || name.empty()) {
		return;
	}

	auto arg = QString::fromStdString(name);
	QVariant v = QVariant::fromValue(arg);
	item->setText(arg);
	item->setData(Qt::UserRole, arg);
	int idx = _argList->currentRow();
	_conf._args[idx] = arg;

	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::WorkingDirectoryChanged(const QString &path)
{
	_conf._workingDirectory = path.toStdString();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::SetArgListSize()
{
	setHeightToContentHeight(_argList);
	adjustSize();
	updateGeometry();
}

void ProcessConfigEdit::ShowAdvancedSettings(bool showAdvancedSettings)
{
	setLayoutVisible(_advancedSettingsLayout, showAdvancedSettings);
	_showAdvancedSettings->setVisible(!showAdvancedSettings);
	adjustSize();
	updateGeometry();
}
