#include "headers/macro-action-run.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/name-dialog.hpp"
#include "headers/utility.hpp"

#include <QProcess>
#include <QFileDialog>

const std::string MacroActionRun::id = "run";

bool MacroActionRun::_registered = MacroActionFactory::Register(
	MacroActionRun::id, {MacroActionRun::Create, MacroActionRunEdit::Create,
			     "AdvSceneSwitcher.action.run"});

bool MacroActionRun::PerformAction()
{
	QProcess::startDetached(QString::fromStdString(_path), _args);
	return true;
}

void MacroActionRun::LogAction()
{
	vblog(LOG_INFO, "run \"%s\"", _path.c_str());
}

bool MacroActionRun::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "path", _path.c_str());
	obs_data_array_t *args = obs_data_array_create();
	for (auto &arg : _args) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "arg",
				    arg.toStdString().c_str());
		obs_data_array_push_back(args, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "args", args);
	obs_data_array_release(args);
	return true;
}

bool MacroActionRun::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_path = obs_data_get_string(obj, "path");
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

std::string MacroActionRun::GetShortDesc()
{
	return _path;
}

MacroActionRunEdit::MacroActionRunEdit(
	QWidget *parent, std::shared_ptr<MacroActionRun> entryData)
	: QWidget(parent)
{
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	_argList = new QListWidget();
	_addArg = new QPushButton();
	_addArg->setMaximumSize(QSize(22, 22));
	_addArg->setProperty("themeID",
			     QVariant(QString::fromUtf8("addIconSmall")));
	_addArg->setFlat(true);
	_removeArg = new QPushButton();
	_removeArg->setMaximumSize(QSize(22, 22));
	_removeArg->setProperty("themeID",
				QVariant(QString::fromUtf8("removeIconSmall")));
	_removeArg->setFlat(true);
	_argUp = new QPushButton();
	_argUp->setMaximumSize(QSize(22, 22));
	_argUp->setProperty("themeID",
			    QVariant(QString::fromUtf8("upArrowIconSmall")));
	_argUp->setFlat(true);
	_argDown = new QPushButton();
	_argDown->setMaximumSize(QSize(22, 22));
	_argDown->setProperty(
		"themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	_argDown->setFlat(true);

	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QWidget::connect(_addArg, SIGNAL(clicked()), this, SLOT(AddArg()));
	QWidget::connect(_removeArg, SIGNAL(clicked()), this,
			 SLOT(RemoveArg()));
	QWidget::connect(_argUp, SIGNAL(clicked()), this, SLOT(ArgUp()));
	QWidget::connect(_argDown, SIGNAL(clicked()), this, SLOT(ArgDown()));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{filePath}}", _filePath},
		{"{{browseButton}}", _browseButton},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.run.entry"),
		     entryLayout, widgetPlaceholders);

	auto *argButtonLayout = new QHBoxLayout;
	argButtonLayout->addWidget(_addArg);
	argButtonLayout->addWidget(_removeArg);
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	argButtonLayout->addWidget(line);
	argButtonLayout->addWidget(_argUp);
	argButtonLayout->addWidget(_argDown);
	argButtonLayout->addStretch();

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.run.arguments")));
	mainLayout->addWidget(_argList);
	mainLayout->addLayout(argButtonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionRunEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_filePath->setText(QString::fromStdString(_entryData->_path));
	for (auto &arg : _entryData->_args) {
		QListWidgetItem *item = new QListWidgetItem(arg, _argList);
		item->setData(Qt::UserRole, arg);
	}
}

void MacroActionRunEdit::FilePathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_path = _filePath->text().toUtf8().constData();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionRunEdit::BrowseButtonClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	QString path = QFileDialog::getOpenFileName(this);
	if (path.isEmpty()) {
		return;
	}

	_filePath->setText(path);
	FilePathChanged();
}

void MacroActionRunEdit::AddArg()
{
	if (_loading || !_entryData) {
		return;
	}

	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this,
		obs_module_text("AdvSceneSwitcher.action.run.addArgument"),
		obs_module_text(
			"AdvSceneSwitcher.action.run.addArgumentDescription"),
		name, "", 170, false);

	if (!accepted || name.empty()) {
		return;
	}
	auto arg = QString::fromStdString(name);
	QVariant v = QVariant::fromValue(arg);
	QListWidgetItem *item = new QListWidgetItem(arg, _argList);
	item->setData(Qt::UserRole, arg);

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_args << arg;
}

void MacroActionRunEdit::RemoveArg()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	int idx = _argList->currentRow();
	if (idx == -1) {
		return;
	}
	_entryData->_args.removeAt(idx);

	QListWidgetItem *item = _argList->currentItem();
	if (!item) {
		return;
	}
	delete item;
}

void MacroActionRunEdit::ArgUp()
{
	if (_loading || !_entryData) {
		return;
	}

	int idx = _argList->currentRow();
	if (idx != -1 && idx != 0) {
		_argList->insertItem(idx - 1, _argList->takeItem(idx));
		_argList->setCurrentRow(idx - 1);

		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_args.move(idx, idx - 1);
	}
}

void MacroActionRunEdit::ArgDown()
{
	int idx = _argList->currentRow();
	if (idx != -1 && idx != _argList->count() - 1) {
		_argList->insertItem(idx + 1, _argList->takeItem(idx));
		_argList->setCurrentRow(idx + 1);

		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_args.move(idx, idx + 1);
	}
}
