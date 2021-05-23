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
	if (_macro) {
		obs_data_set_string(obj, "macro", _macro->Name().c_str());
	}
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionPause::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro = GetMacroByName(obs_data_get_string(obj, "macro"));
	_action = static_cast<PauseAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionPauseEdit::MacroActionPauseEdit(
	QWidget *parent, std::shared_ptr<MacroActionPause> entryData)
	: QWidget(parent)
{
	_macros = new MacroSelection(parent);
	_actions = new QComboBox();

	populateActionSelection(_actions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

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
	_macros->SetCurrentMacro(_entryData->_macro);
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
