#include "macro-action-macro.hpp"
#include "help-icon.hpp"
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

static void runActionsHelper(Macro *macro, bool runElseActions, bool setInputs,
			     const StringList &inputs)
{
	if (setInputs) {
		macro->GetInputVariables().SetValues(inputs);
	}
	macro->PerformActions(!runElseActions, false, true);
}

void MacroActionMacro::RunActions(Macro *actionMacro) const
{
	if (_runOptions.skipWhenPaused && actionMacro->Paused()) {
		return;
	}

	if (_runOptions.logic == RunOptions::Logic::IGNORE_CONDITIONS) {
		runActionsHelper(actionMacro, _runOptions.runElseActions,
				 _runOptions.setInputs, _runOptions.inputs);
		return;
	}

	auto conditionMacro = _runOptions.macro.GetMacro();
	if (!conditionMacro) {
		return;
	}

	if (_runOptions.reevaluateConditionState) {
		conditionMacro->CeckMatch(true);
	}

	if ((_runOptions.logic == RunOptions::Logic::CONDITIONS &&
	     conditionMacro->Matched()) ||
	    (_runOptions.logic == RunOptions::Logic::INVERT_CONDITIONS &&
	     !conditionMacro->Matched())) {
		runActionsHelper(actionMacro, _runOptions.runElseActions,
				 _runOptions.setInputs, _runOptions.inputs);
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
	  _reevaluateConditionState(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.macro.type.run."
				  "updateConditionMatchState"))),
	  _actionTypes(new QComboBox()),
	  _skipWhenPaused(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.macro.type.run.skipWhenPaused"))),
	  _setInputs(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.macro.type.run.setInputs"))),
	  _inputs(new MacroInputEdit()),
	  _entryLayout(new QHBoxLayout()),
	  _conditionLayout(new QHBoxLayout()),
	  _reevaluateConditionStateLayout(new QHBoxLayout()),
	  _setInputsLayout(new QHBoxLayout())
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
	QWidget::connect(_setInputs, SIGNAL(stateChanged(int)), this,
			 SLOT(SetInputsChanged(int)));
	QWidget::connect(_inputs,
			 SIGNAL(MacroInputValuesChanged(const StringList &)),
			 this, SLOT(InputsChanged(const StringList &)));
	QWidget::connect(_reevaluateConditionState, SIGNAL(stateChanged(int)),
			 this, SLOT(ReevaluateConditionStateChanged(int)));

	_setInputsLayout->addWidget(_setInputs);
	_setInputsLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.setInputs.description")));
	_setInputsLayout->addStretch();

	_reevaluateConditionStateLayout->addWidget(_reevaluateConditionState);
	_reevaluateConditionStateLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.updateConditionMatchState.help")));
	_reevaluateConditionStateLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->addLayout(_entryLayout);
	layout->addLayout(_conditionLayout);
	layout->addLayout(_reevaluateConditionStateLayout);
	layout->addLayout(_setInputsLayout);
	layout->addWidget(_inputs);
	layout->addWidget(_skipWhenPaused);
	setLayout(layout);
	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void HighligthMacroSettingsButton(bool enable);

MacroActionMacroEdit::~MacroActionMacroEdit()
{
	HighligthMacroSettingsButton(false);
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
	_reevaluateConditionState->setChecked(
		_entryData->_runOptions.reevaluateConditionState);
	_actionTypes->setCurrentIndex(
		_entryData->_runOptions.runElseActions ? 1 : 0);
	_skipWhenPaused->setChecked(_entryData->_runOptions.skipWhenPaused);
	_setInputs->setChecked(_entryData->_runOptions.setInputs);
	SetupMacroInput(_entryData->_macro.GetMacro().get());
	SetWidgetVisibility();
}

void MacroActionMacroEdit::MacroChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macro = text;
	const auto &macro = _entryData->_macro.GetMacro();
	_actionIndex->SetMacro(macro);
	SetupMacroInput(macro.get());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionMacro::Action>(value);
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionIndexChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionIndex = value;
}

void MacroActionMacroEdit::ConditionMacroChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.macro = text;
}

void MacroActionMacroEdit::ConditionBehaviorChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.logic =
		static_cast<MacroActionMacro::RunOptions::Logic>(value);
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ReevaluateConditionStateChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.reevaluateConditionState = value;
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionTypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.runElseActions = value;
}

void MacroActionMacroEdit::SkipWhenPausedChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.skipWhenPaused = value;
}

void MacroActionMacroEdit::SetInputsChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.setInputs = value;
	SetWidgetVisibility();
}

void MacroActionMacroEdit::InputsChanged(const StringList &inputs)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.inputs = inputs;
	adjustSize();
	updateGeometry();
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
	const bool needsAdditionalConditionWidgets =
		_entryData->_action == MacroActionMacro::Action::RUN &&
		_entryData->_runOptions.logic !=
			MacroActionMacro::RunOptions::Logic::IGNORE_CONDITIONS;
	_conditionMacros->setVisible(needsAdditionalConditionWidgets);
	SetLayoutVisible(_reevaluateConditionStateLayout,
			 needsAdditionalConditionWidgets);
	SetLayoutVisible(_setInputsLayout,
			 _entryData->_action == MacroActionMacro::Action::RUN);
	_inputs->setVisible(_entryData->_action ==
				    MacroActionMacro::Action::RUN &&
			    _entryData->_runOptions.setInputs);
	HighligthMacroSettingsButton(_entryData->_action ==
					     MacroActionMacro::Action::RUN &&
				     _entryData->_runOptions.setInputs &&
				     !_inputs->HasInputsToSet());
	_actionTypes->setVisible(_entryData->_action ==
				 MacroActionMacro::Action::RUN);
	_skipWhenPaused->setVisible(_entryData->_action ==
				    MacroActionMacro::Action::RUN);

	adjustSize();
	updateGeometry();
}

void MacroActionMacroEdit::SetupMacroInput(Macro *macro) const
{
	if (macro) {
		_inputs->SetInputVariablesAndValues(
			macro->GetInputVariables(),
			_entryData->_runOptions.inputs);
	} else {
		_inputs->SetInputVariablesAndValues({}, {});
	}
}

void MacroActionMacro::RunOptions::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "logic", static_cast<int>(logic));
	obs_data_set_bool(data, "reevaluateConditionState",
			  reevaluateConditionState);
	obs_data_set_bool(data, "runElseActions", runElseActions);
	obs_data_set_bool(data, "skipWhenPaused", skipWhenPaused);
	obs_data_set_bool(data, "setInputs", setInputs);
	inputs.Save(data, "inputs");
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
	reevaluateConditionState =
		obs_data_get_bool(data, "reevaluateConditionState");
	runElseActions = obs_data_get_bool(data, "runElseActions");
	skipWhenPaused = obs_data_get_bool(data, "skipWhenPaused");
	setInputs = obs_data_get_bool(data, "setInputs");
	inputs.Load(data, "inputs");
	macro.Load(data);
}

} // namespace advss
