#include "macro-action-file.hpp"
#include "layout-helpers.hpp"

#include <QFileDialog>
#include <QFile>
#include <QTextStream>

namespace advss {

const std::string MacroActionFile::id = "file";

bool MacroActionFile::_registered = MacroActionFactory::Register(
	MacroActionFile::id,
	{MacroActionFile::Create, MacroActionFileEdit::Create,
	 "AdvSceneSwitcher.action.file"});

static const std::map<MacroActionFile::Action, std::string> actionTypes = {
	{MacroActionFile::Action::WRITE,
	 "AdvSceneSwitcher.action.file.type.write"},
	{MacroActionFile::Action::APPEND,
	 "AdvSceneSwitcher.action.file.type.append"},
};

bool MacroActionFile::PerformAction()
{
	QString path = QString::fromStdString(_file);
	QFile file(path);
	bool open = false;
	switch (_action) {
	case Action::WRITE:
		open = file.open(QIODevice::WriteOnly);
		break;
	case Action::APPEND:
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

void MacroActionFile::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\" for file \"%s\"",
		      it->second.c_str(), _file.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown file action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionFile::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_file.Save(obj, "file");
	_text.Save(obj, "text");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionFile::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_file.Load(obj, "file");
	_text.Load(obj, "text");
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionFile::GetShortDesc() const
{
	return _file.UnresolvedValue();
}

std::shared_ptr<MacroAction> MacroActionFile::Create(Macro *m)
{
	return std::make_shared<MacroActionFile>(m);
}

std::shared_ptr<MacroAction> MacroActionFile::Copy() const
{
	return std::make_shared<MacroActionFile>(*this);
}

void MacroActionFile::ResolveVariablesToFixedValues()
{
	_file.ResolveVariables();
	_text.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionFileEdit::MacroActionFileEdit(
	QWidget *parent, std::shared_ptr<MacroActionFile> entryData)
	: QWidget(parent),
	  _filePath(new FileSelection(FileSelection::Type::WRITE)),
	  _text(new VariableTextEdit(this)),
	  _actions(new QComboBox())
{
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
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.file.entry"),
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
	_text->setPlainText(_entryData->_text);

	adjustSize();
	updateGeometry();
}

void MacroActionFileEdit::PathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_file = text.toUtf8().constData();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionFileEdit::TextChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_text = _text->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionFileEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionFile::Action>(value);
}

} // namespace advss
