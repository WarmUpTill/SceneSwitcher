#include "headers/macro-action-run.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <QProcess>
#include <QFileDialog>

const int MacroActionRun::id = 6;

bool MacroActionRun::_registered = MacroActionFactory::Register(
	MacroActionRun::id, {MacroActionRun::Create, MacroActionRunEdit::Create,
			     "AdvSceneSwitcher.action.run"});

bool MacroActionRun::PerformAction()
{
	QProcess::startDetached(QString::fromStdString(_path), QStringList());
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
	return true;
}

bool MacroActionRun::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_path = obs_data_get_string(obj, "path");
	return true;
}

MacroActionRunEdit::MacroActionRunEdit(
	QWidget *parent, std::shared_ptr<MacroActionRun> entryData)
	: QWidget(parent)
{
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	_browseButton->setStyleSheet("border:1px solid gray;");

	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{filePath}}", _filePath},
		{"{{browseButton}}", _browseButton},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.run.entry"),
		     mainLayout, widgetPlaceholders);
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
}

void MacroActionRunEdit::FilePathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_path = _filePath->text().toUtf8().constData();
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
