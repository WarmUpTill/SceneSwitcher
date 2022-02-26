#include "headers/macro-action-file.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <QFile>
#include <QTextStream>
#include <QFileDialog>

const std::string MacroActionFile::id = "file";

bool MacroActionFile::_registered = MacroActionFactory::Register(
	MacroActionFile::id,
	{MacroActionFile::Create, MacroActionFileEdit::Create,
	 "AdvSceneSwitcher.action.file"});

const static std::map<FileAction, std::string> actionTypes = {
	{FileAction::WRITE, "AdvSceneSwitcher.action.file.type.write"},
	{FileAction::APPEND, "AdvSceneSwitcher.action.file.type.append"},
};

bool MacroActionFile::PerformAction()
{
	QString path = QString::fromStdString(_file);
	QFile file(path);
	bool open = false;
	switch (_action) {
	case FileAction::WRITE:
		open = file.open(QIODevice::WriteOnly);
		break;
	case FileAction::APPEND:
		open = file.open(QIODevice::WriteOnly | QIODevice::Append);
		break;
	default:
		break;
	}
	if (open) {
		QTextStream out(&file);
		out << QString::fromStdString(_text);
	}
	return true;
}

void MacroActionFile::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" for file \"%s\"",
		      it->second.c_str(), _file.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown file action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionFile::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "file", _file.c_str());
	obs_data_set_string(obj, "text", _text.c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionFile::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_file = obs_data_get_string(obj, "file");
	_text = obs_data_get_string(obj, "text");
	_action = static_cast<FileAction>(obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionFile::GetShortDesc()
{
	return _file;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionFileEdit::MacroActionFileEdit(
	QWidget *parent, std::shared_ptr<MacroActionFile> entryData)
	: QWidget(parent)
{
	_filePath = new FileSelection(FileSelection::Type::WRITE);
	_text = new ResizingPlainTextEdit(this);
	_actions = new QComboBox();

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_text, SIGNAL(textChanged()), this,
			 SLOT(TextChanged()));
	;

	QHBoxLayout *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{filePath}}", _filePath},
		{"{{matchText}}", _text},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.file.entry"),
		     entryLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_text);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionFileEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_filePath->SetPath(QString::fromStdString(_entryData->_file));
	_text->setPlainText(QString::fromStdString(_entryData->_text));

	adjustSize();
	updateGeometry();
}

void MacroActionFileEdit::PathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_file = text.toUtf8().constData();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionFileEdit::TextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_text = _text->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFileEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<FileAction>(value);
}
