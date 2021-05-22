#include "headers/macro-action-pause.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionPause::id = "pause";

bool MacroActionPause::_registered = MacroActionFactory::Register(
	MacroActionPause::id,
	{MacroActionPause::Create, MacroActionPauseEdit::Create,
	 "AdvSceneSwitcher.action.pause"});

const static std::map<PauseAction, std::string> actionTypes = {
	{PauseAction::PAUSE, "AdvSceneSwitcher.action.pause.type.pause"},
	{PauseAction::UNPAUSE, "AdvSceneSwitcher.action.pause.type.unpause"},
};

bool MacroActionPause::PerformAction()
{
	if (!_macro) {
		return true;
	}

	switch (_action) {
	case PauseAction::PAUSE:
		_macro->SetPaused();
		break;
	case PauseAction::UNPAUSE:
		_macro->SetPaused(false);
		break;
	default:
		break;
	}
	return true;
}

void MacroActionPause::LogAction()
{
	if (!_macro) {
		return;
	}
	switch (_action) {
	case PauseAction::PAUSE:
		vblog(LOG_INFO, "paused \"%s\"", _macro->Name().c_str());
		break;
	case PauseAction::UNPAUSE:
		vblog(LOG_INFO, "unpaused \"%s\"", _macro->Name().c_str());
		break;
	default:
		break;
	}
}

bool MacroActionPause::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionPause::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<PauseAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateMacroSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.selectMacro"));
	for (auto &m : switcher->macros) {
		list->addItem(QString::fromStdString(m.Name()));
	}
}

MacroActionPauseEdit::MacroActionPauseEdit(
	QWidget *parent, std::shared_ptr<MacroActionPause> entryData)
	: QWidget(parent)
{
	_macros = new QComboBox();
	_actions = new QComboBox();

	populateActionSelection(_actions);
	populateMacroSelection(_macros);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(parent, SIGNAL(MacroAdded(const QString &)), this,
			 SLOT(MacroAdd(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(parent,
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{macros}}", _macros},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.pause.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionPauseEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	if (_entryData->_macro) {
		_macros->setCurrentText(
			QString::fromStdString(_entryData->_macro->Name()));
	} else {
		_macros->setCurrentIndex(0);
	}
}

void MacroActionPauseEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macro = GetMacroByQString(text);
}

void MacroActionPauseEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<PauseAction>(value);
}

void MacroActionPauseEdit::MacroAdd(const QString &name)
{
	_macros->addItem(name);
}

void MacroActionPauseEdit::MacroRemove(const QString &name)
{
	int idx = _macros->findText(name);
	if (idx == -1) {
		return;
	}
	_macros->removeItem(idx);
	if (_entryData && _entryData->_macro == GetMacroByQString(name)) {
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_macro = nullptr;
	}
	_macros->setCurrentIndex(0);
}

void MacroActionPauseEdit::MacroRename(const QString &oldName,
				       const QString &newName)
{
	bool renameSelected = _macros->currentText() == oldName;
	int idx = _macros->findText(oldName);
	if (idx == -1) {
		return;
	}
	_macros->removeItem(idx);
	_macros->insertItem(idx, newName);
	if (renameSelected) {
		_macros->setCurrentIndex(_macros->findText(newName));
	}
}
