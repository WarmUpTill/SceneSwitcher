#include "macro-action-macro.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"

namespace advss {

const std::string MacroActionMacro::id = "macro";

bool MacroActionMacro::_registered = MacroActionFactory::Register(
	MacroActionMacro::id,
	{MacroActionMacro::Create, MacroActionMacroEdit::Create,
	 "AdvSceneSwitcher.action.macro"});

const static std::map<MacroActionMacro::Action, std::string> actionTypes = {
	{MacroActionMacro::Action::PAUSE,
	 "AdvSceneSwitcher.action.macro.type.pause"},
	{MacroActionMacro::Action::UNPAUSE,
	 "AdvSceneSwitcher.action.macro.type.unpause"},
	{MacroActionMacro::Action::RESET_COUNTER,
	 "AdvSceneSwitcher.action.macro.type.resetCounter"},
	{MacroActionMacro::Action::RUN,
	 "AdvSceneSwitcher.action.macro.type.run"},
	{MacroActionMacro::Action::STOP,
	 "AdvSceneSwitcher.action.macro.type.stop"},
	{MacroActionMacro::Action::DISABLE_ACTION,
	 "AdvSceneSwitcher.action.macro.type.disableAction"},
	{MacroActionMacro::Action::ENABLE_ACTION,
	 "AdvSceneSwitcher.action.macro.type.enableAction"},
	{MacroActionMacro::Action::TOGGLE_ACTION,
	 "AdvSceneSwitcher.action.macro.type.toggleAction"},
};

bool MacroActionMacro::PerformAction()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return true;
	}

	switch (_action) {
	case Action::PAUSE:
		macro->SetPaused();
		break;
	case Action::UNPAUSE:
		macro->SetPaused(false);
		break;
	case Action::RESET_COUNTER:
		macro->ResetRunCount();
		break;
	case Action::RUN:
		if (!macro->Paused()) {
			macro->PerformActions(true, false, true);
		}
		break;
	case Action::STOP:
		macro->Stop();
		break;
	case Action::DISABLE_ACTION:
		if (IsValidMacroSegmentIndex(macro.get(), _actionIndex - 1,
					     false)) {
			macro->Actions().at(_actionIndex - 1)->SetEnabled(false);
		}
		break;
	case Action::ENABLE_ACTION:
		if (IsValidMacroSegmentIndex(macro.get(), _actionIndex - 1,
					     false)) {
			macro->Actions().at(_actionIndex - 1)->SetEnabled(true);
		}
		break;
	case Action::TOGGLE_ACTION:
		if (IsValidMacroSegmentIndex(macro.get(), _actionIndex - 1,
					     false)) {
			auto action = macro->Actions().at(_actionIndex - 1);
			action->SetEnabled(!action->Enabled());
		}
		break;
	default:
		break;
	}
	return true;
}

void MacroActionMacro::LogAction() const
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return;
	}
	switch (_action) {
	case Action::PAUSE:
		vblog(LOG_INFO, "paused \"%s\"", macro->Name().c_str());
		break;
	case Action::UNPAUSE:
		vblog(LOG_INFO, "unpaused \"%s\"", macro->Name().c_str());
		break;
	case Action::RESET_COUNTER:
		vblog(LOG_INFO, "reset counter for \"%s\"",
		      macro->Name().c_str());
		break;
	case Action::RUN:
		vblog(LOG_INFO, "run nested macro \"%s\"",
		      macro->Name().c_str());
		break;
	case Action::STOP:
		vblog(LOG_INFO, "stopped macro \"%s\"", macro->Name().c_str());
		break;
	case Action::DISABLE_ACTION:
		vblog(LOG_INFO, "disabled action %d of macro \"%s\"",
		      _actionIndex.GetValue(), macro->Name().c_str());
		break;
	case Action::ENABLE_ACTION:
		vblog(LOG_INFO, "enabled action %d of macro \"%s\"",
		      _actionIndex.GetValue(), macro->Name().c_str());
		break;
	case Action::TOGGLE_ACTION:
		vblog(LOG_INFO, "toggled action %d of macro \"%s\"",
		      _actionIndex.GetValue(), macro->Name().c_str());
		break;
	default:
		break;
	}
}

bool MacroActionMacro::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_macro.Save(obj);
	_actionIndex.Save(obj, "actionIndex");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionMacro::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro.Load(obj);
	_actionIndex.Load(obj, "actionIndex");
	_action = static_cast<MacroActionMacro::Action>(
		obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionMacro::GetShortDesc() const
{
	return _macro.Name();
}

std::shared_ptr<MacroAction> MacroActionMacro::Create(Macro *m)
{
	return std::make_shared<MacroActionMacro>(m);
}

std::shared_ptr<MacroAction> MacroActionMacro::Copy() const
{
	return std::make_shared<MacroActionMacro>(*this);
}

void MacroActionMacro::ResolveVariablesToFixedValues()
{
	_actionIndex.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionMacroEdit::MacroActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionMacro> entryData)
	: QWidget(parent),
	  _macros(new MacroSelection(parent)),
	  _actionIndex(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::ACTION)),
	  _actions(new QComboBox())
{
	populateActionSelection(_actions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_actionIndex,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(ActionIndexChanged(const IntVariable &)));

	auto mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.macro.entry"),
		     mainLayout,
		     {{"{{actions}}", _actions},
		      {"{{actionIndex}}", _actionIndex},
		      {"{{macros}}", _macros}});
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_actionIndex->SetValue(_entryData->_actionIndex);
	_actionIndex->SetMacro(_entryData->_macro.GetMacro());
	_macros->SetCurrentMacro(_entryData->_macro);
	SetWidgetVisibility();
}

void MacroActionMacroEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_macro = text;
	_actionIndex->SetMacro(_entryData->_macro.GetMacro());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionMacroEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionMacro::Action>(value);
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionIndexChanged(const IntVariable &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_actionIndex = value;
}

void MacroActionMacroEdit::SetWidgetVisibility()
{
	if (_entryData->_action == MacroActionMacro::Action::RUN ||
	    _entryData->_action == MacroActionMacro::Action::STOP) {
		_macros->HideSelectedMacro();
	} else {
		_macros->ShowAllMacros();
	}

	const bool isModifyingActionState =
		_entryData->_action ==
			MacroActionMacro::Action::DISABLE_ACTION ||
		_entryData->_action ==
			MacroActionMacro::Action::ENABLE_ACTION ||
		_entryData->_action == MacroActionMacro::Action::TOGGLE_ACTION;
	_actionIndex->setVisible(isModifyingActionState);
}

} // namespace advss
