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
		RunActions(macro.get());
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
		ablog(LOG_INFO, "paused \"%s\"", macro->Name().c_str());
		break;
	case Action::UNPAUSE:
		ablog(LOG_INFO, "unpaused \"%s\"", macro->Name().c_str());
		break;
	case Action::RESET_COUNTER:
		ablog(LOG_INFO, "reset counter for \"%s\"",
		      macro->Name().c_str());
		break;
	case Action::RUN:
		ablog(LOG_INFO, "run nested macro \"%s\"",
		      macro->Name().c_str());
		break;
	case Action::STOP:
		ablog(LOG_INFO, "stopped macro \"%s\"", macro->Name().c_str());
		break;
	case Action::DISABLE_ACTION:
		ablog(LOG_INFO, "disabled action %d of macro \"%s\"",
		      _actionIndex.GetValue(), macro->Name().c_str());
		break;
	case Action::ENABLE_ACTION:
		ablog(LOG_INFO, "enabled action %d of macro \"%s\"",
		      _actionIndex.GetValue(), macro->Name().c_str());
		break;
	case Action::TOGGLE_ACTION:
		ablog(LOG_INFO, "toggled action %d of macro \"%s\"",
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
	_runOptions.Save(obj);
	return true;
}

bool MacroActionMacro::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro.Load(obj);
	_actionIndex.Load(obj, "actionIndex");
	_action = static_cast<MacroActionMacro::Action>(
		obs_data_get_int(obj, "action"));
	_runOptions.Load(obj);
	return true;
}

bool MacroActionMacro::PostLoad()
{
	MacroRefAction::PostLoad();
	MacroAction::PostLoad();
	_runOptions.macro.PostLoad();
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

void MacroActionMacro::RunActions(Macro *actionMacro) const
{
	if (_runOptions.skipWhenPaused && actionMacro->Paused()) {
		return;
	}

	if (_runOptions.logic == RunOptions::Logic::IGNORE_CONDITIONS) {
		actionMacro->PerformActions(!_runOptions.runElseActions, false,
					    true);
		return;
	}

	auto conditionMacro = _runOptions.macro.GetMacro();
	if (!conditionMacro) {
		return;
	}

	if ((_runOptions.logic == RunOptions::Logic::CONDITIONS &&
	     conditionMacro->Matched()) ||
	    (_runOptions.logic == RunOptions::Logic::INVERT_CONDITIONS &&
	     !conditionMacro->Matched())) {
		actionMacro->PerformActions(!_runOptions.runElseActions, false,
					    true);
	}
}

static void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static void populateConditionBehaviorSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.conditions.ignore"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.conditions.true"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.conditions.false"));
}

static void populateActionTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.actionType.regular"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.actionType.else"));
}

MacroActionMacroEdit::MacroActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionMacro> entryData)
	: QWidget(parent),
	  _macros(new MacroSelection(parent)),
	  _actionIndex(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::ACTION)),
	  _actions(new QComboBox()),
	  _conditionMacros(new MacroSelection(parent)),
	  _conditionBehaviors(new QComboBox()),
	  _actionTypes(new QComboBox()),
	  _skipWhenPaused(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.macro.type.run.skipWhenPaused"))),
	  _entryLayout(new QHBoxLayout()),
	  _conditionLayout(new QHBoxLayout())
{
	populateActionSelection(_actions);
	populateConditionBehaviorSelection(_conditionBehaviors);
	populateActionTypeSelection(_actionTypes);

	_conditionMacros->HideSelectedMacro();

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_actionIndex,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(ActionIndexChanged(const IntVariable &)));
	QWidget::connect(_conditionMacros,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ConditionMacroChanged(const QString &)));
	QWidget::connect(_conditionBehaviors, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionBehaviorChanged(int)));
	QWidget::connect(_actionTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionTypeChanged(int)));
	QWidget::connect(_skipWhenPaused, SIGNAL(stateChanged(int)), this,
			 SLOT(SkipWhenPausedChanged(int)));

	auto layout = new QVBoxLayout();
	layout->addLayout(_entryLayout);
	layout->addLayout(_conditionLayout);
	layout->addWidget(_skipWhenPaused);
	setLayout(layout);
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
	_conditionMacros->SetCurrentMacro(_entryData->_runOptions.macro);
	_conditionBehaviors->setCurrentIndex(
		static_cast<int>(_entryData->_runOptions.logic));
	_actionTypes->setCurrentIndex(
		_entryData->_runOptions.runElseActions ? 1 : 0);
	_skipWhenPaused->setChecked(_entryData->_runOptions.skipWhenPaused);
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

void MacroActionMacroEdit::ConditionMacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_runOptions.macro = text;
}

void MacroActionMacroEdit::ConditionBehaviorChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_runOptions.logic =
		static_cast<MacroActionMacro::RunOptions::Logic>(value);
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_runOptions.runElseActions = value;
}

void MacroActionMacroEdit::SkipWhenPausedChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_runOptions.skipWhenPaused = value;
}

void MacroActionMacroEdit::SetWidgetVisibility()
{
	_entryLayout->removeWidget(_actions);
	_entryLayout->removeWidget(_actionIndex);
	_entryLayout->removeWidget(_macros);
	_entryLayout->removeWidget(_actionTypes);
	_conditionLayout->removeWidget(_conditionBehaviors);
	_conditionLayout->removeWidget(_conditionMacros);

	ClearLayout(_entryLayout);
	ClearLayout(_conditionLayout);

	const std::unordered_map<std::string, QWidget *> placeholders = {
		{"{{actions}}", _actions},
		{"{{actionIndex}}", _actionIndex},
		{"{{macros}}", _macros},
		{"{{actionTypes}}", _actionTypes},
		{"{{conditionBehaviors}}", _conditionBehaviors},
		{"{{conditionMacros}}", _conditionMacros},

	};

	PlaceWidgets(
		obs_module_text(
			_entryData->_action == MacroActionMacro::Action::RUN
				? "AdvSceneSwitcher.action.macro.entry.run"
				: "AdvSceneSwitcher.action.macro.entry.other"),
		_entryLayout, placeholders);

	if (_entryData->_runOptions.logic ==
	    MacroActionMacro::RunOptions::Logic::IGNORE_CONDITIONS) {
		_conditionLayout->addWidget(_conditionBehaviors);
		_conditionLayout->addStretch();
	} else {
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.macro.entry.run.condition"),
			_conditionLayout, placeholders);
	}

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

	SetLayoutVisible(_conditionLayout,
			 _entryData->_action == MacroActionMacro::Action::RUN);
	_conditionMacros->setVisible(
		_entryData->_action == MacroActionMacro::Action::RUN &&
		_entryData->_runOptions.logic !=
			MacroActionMacro::RunOptions::Logic::IGNORE_CONDITIONS);
	_actionTypes->setVisible(_entryData->_action ==
				 MacroActionMacro::Action::RUN);
	_skipWhenPaused->setVisible(_entryData->_action ==
				    MacroActionMacro::Action::RUN);

	adjustSize();
	updateGeometry();
}

void MacroActionMacro::RunOptions::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "logic", static_cast<int>(logic));
	obs_data_set_bool(data, "runElseActions", runElseActions);
	obs_data_set_bool(data, "skipWhenPaused", skipWhenPaused);
	macro.Save(data);
	obs_data_set_obj(obj, "runOptions", data);
}

void MacroActionMacro::RunOptions::Load(obs_data_t *obj)
{
	if (!obs_data_has_user_value(obj, "runOptions")) {
		return;
	}
	OBSDataAutoRelease data = obs_data_get_obj(obj, "runOptions");
	logic = static_cast<Logic>(obs_data_get_int(data, "logic"));
	runElseActions = obs_data_get_bool(data, "runElseActions");
	skipWhenPaused = obs_data_get_bool(data, "skipWhenPaused");
	macro.Load(data);
}

} // namespace advss
