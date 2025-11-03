#include "macro-action-macro.hpp"
#include "help-icon.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "macro-action-factory.hpp"

namespace advss {

const std::string MacroActionMacro::id = "macro";

bool MacroActionMacro::_registered = MacroActionFactory::Register(
	MacroActionMacro::id,
	{MacroActionMacro::Create, MacroActionMacroEdit::Create,
	 "AdvSceneSwitcher.action.macro"});

void MacroActionMacro::AdjustActionState(Macro *macro) const
{
	const auto &macroActions = _useElseSection ? macro->ElseActions()
						   : macro->Actions();

	std::vector<std::shared_ptr<MacroAction>> actionsToModify;
	switch (_actionSelectionType) {
	case SelectionType::INDEX: {
		const bool isValidAction =
			(_useElseSection &&
			 IsValidElseActionIndex(macro, _actionIndex - 1)) ||
			(!_useElseSection &&
			 IsValidActionIndex(macro, _actionIndex - 1));

		if (isValidAction) {
			actionsToModify.emplace_back(
				macroActions.at(_actionIndex - 1));
		}
		break;
	}
	case SelectionType::LABEL:
		for (const auto &action : macroActions) {
			if (!action->GetUseCustomLabel()) {
				continue;
			}

			const auto label = action->GetCustomLabel();

			if (_regex.Enabled()) {
				if (_regex.Matches(label, _label)) {
					actionsToModify.emplace_back(action);
				}
				continue;
			}

			if (label == std::string(_label)) {
				actionsToModify.emplace_back(action);
			}
		}
		break;
	case SelectionType::ID:
		for (const auto &action : macroActions) {
			if (action->GetId() == _actionId) {
				actionsToModify.emplace_back(action);
			}
		}
		break;
	default:
		break;
	}

	for (const auto &action : actionsToModify) {
		switch (_action) {
		case Action::DISABLE_ACTION:
			action->SetEnabled(false);
			break;
		case Action::ENABLE_ACTION:
			action->SetEnabled(true);
			break;
		case Action::TOGGLE_ACTION:
			action->SetEnabled(!action->Enabled());
			break;
		default:
			break;
		}
	}
}

bool MacroActionMacro::PerformAction()
{
	if (_action == Action::NESTED_MACRO) {
		const bool conditionsMatched = _nestedMacro->CheckConditions();
		return _nestedMacro->PerformActions(conditionsMatched);
	}

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
	case Action::TOGGLE_PAUSE:
		macro->SetPaused(!macro->Paused());
		break;
	case Action::RESET_COUNTER:
		macro->ResetRunCount();
		break;
	case Action::RUN_ACTIONS:
		RunActions(macro.get());
		break;
	case Action::STOP:
		macro->Stop();
		break;
	case Action::DISABLE_ACTION:
	case Action::ENABLE_ACTION:
	case Action::TOGGLE_ACTION:
		AdjustActionState(macro.get());
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
	case Action::RUN_ACTIONS:
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
	case Action::NESTED_MACRO:
		ablog(LOG_INFO, "run nested macro");
		break;
	default:
		break;
	}
}

bool MacroActionMacro::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_macro.Save(obj);
	obs_data_set_int(obj, "actionSelectionType",
			 static_cast<int>(_actionSelectionType));
	_actionIndex.Save(obj, "actionIndex");
	_label.Save(obj, "label");
	obs_data_set_string(obj, "actionId", _actionId.c_str());
	_regex.Save(obj);
	_runOptions.Save(obj);
	OBSDataAutoRelease nestedMacroData = obs_data_create();
	_nestedMacro->Save(nestedMacroData);
	obs_data_set_obj(obj, "nestedMacro", nestedMacroData);
	obs_data_set_int(obj, "customWidgetHeight", _customWidgetHeight);
	return true;
}

bool MacroActionMacro::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<MacroActionMacro::Action>(
		obs_data_get_int(obj, "action"));
	_macro.Load(obj);
	_actionSelectionType = static_cast<SelectionType>(
		obs_data_get_int(obj, "actionSelectionType"));
	_actionIndex.Load(obj, "actionIndex");
	_label.Load(obj, "label");
	_actionId = obs_data_get_string(obj, "actionId");
	_regex.Load(obj);
	_runOptions.Load(obj);

	if (obs_data_has_user_value(obj, "nestedMacro")) {
		OBSDataAutoRelease nestedMacroData =
			obs_data_get_obj(obj, "nestedMacro");
		_nestedMacro->Load(nestedMacroData);
	}

	_customWidgetHeight = obs_data_get_int(obj, "customWidgetHeight");

	return true;
}

bool MacroActionMacro::PostLoad()
{
	MacroRefAction::PostLoad();
	MacroAction::PostLoad();
	_runOptions.macro.PostLoad();
	_nestedMacro->PostLoad();
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
	auto copy = std::make_shared<MacroActionMacro>(*this);

	// Create a new nested macro
	OBSDataAutoRelease data = obs_data_create();
	_nestedMacro->Save(data);

	copy->_nestedMacro = std::make_shared<Macro>();
	copy->_nestedMacro->Load(data);
	copy->_nestedMacro->PostLoad();

	return copy;
}

void MacroActionMacro::ResolveVariablesToFixedValues()
{
	_actionIndex.ResolveVariables();
	_label.ResolveVariables();
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
		conditionMacro->CheckConditions(true);
	}

	if ((_runOptions.logic == RunOptions::Logic::CONDITIONS &&
	     conditionMacro->ConditionsMatched()) ||
	    (_runOptions.logic == RunOptions::Logic::INVERT_CONDITIONS &&
	     !conditionMacro->ConditionsMatched())) {
		runActionsHelper(actionMacro, _runOptions.runElseActions,
				 _runOptions.setInputs, _runOptions.inputs);
	}
}

static void populateActionSelection(QComboBox *list)
{
	static const std::vector<std::pair<MacroActionMacro::Action, std::string>>
		actions = {
			{MacroActionMacro::Action::PAUSE,
			 "AdvSceneSwitcher.action.macro.type.pause"},
			{MacroActionMacro::Action::UNPAUSE,
			 "AdvSceneSwitcher.action.macro.type.unpause"},
			{MacroActionMacro::Action::TOGGLE_PAUSE,
			 "AdvSceneSwitcher.action.macro.type.togglePause"},
			{MacroActionMacro::Action::RESET_COUNTER,
			 "AdvSceneSwitcher.action.macro.type.resetCounter"},
			{MacroActionMacro::Action::NESTED_MACRO,
			 "AdvSceneSwitcher.action.macro.type.nestedMacro"},
			{MacroActionMacro::Action::RUN_ACTIONS,
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

	for (const auto &[value, name] : actions) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
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

static void populateActionSectionSelection(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.actionType.regular"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.action.macro.type.run.actionType.else"));
}

static void populateActionTypes(QComboBox *list)
{
	for (const auto &[id, info] : MacroActionFactory::GetActionTypes()) {
		list->addItem(obs_module_text(info._name.c_str()),
			      QString::fromStdString(id));
	}
}

static void populateActionSelectionTypes(QComboBox *list)
{
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.macro.actionSelectionType.index"),
		static_cast<int>(MacroActionMacro::SelectionType::INDEX));
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.macro.actionSelectionType.label"),
		static_cast<int>(MacroActionMacro::SelectionType::LABEL));
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.macro.actionSelectionType.id"),
		static_cast<int>(MacroActionMacro::SelectionType::ID));
}

MacroActionMacroEdit::MacroActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionMacro> entryData)
	: ResizableWidget(parent),
	  _actions(new QComboBox()),
	  _macros(new MacroSelection(parent)),
	  _actionSelectionType(new QComboBox(this)),
	  _actionIndex(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::ACTION)),
	  _label(new VariableLineEdit(this)),
	  _actionTypes(new FilterComboBox(this)),
	  _regex(new RegexConfigWidget(this)),
	  _conditionMacros(new MacroSelection(parent)),
	  _conditionBehaviors(new QComboBox()),
	  _reevaluateConditionState(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.macro.type.run."
				  "updateConditionMatchState"))),
	  _actionSections(new QComboBox(this)),
	  _skipWhenPaused(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.macro.type.run.skipWhenPaused"))),
	  _setInputs(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.macro.type.run.setInputs"))),
	  _inputs(new MacroInputEdit()),
	  _entryLayout(new QHBoxLayout()),
	  _conditionLayout(new QHBoxLayout()),
	  _reevaluateConditionStateLayout(new QHBoxLayout()),
	  _setInputsLayout(new QHBoxLayout()),
	  _nestedMacro(new MacroEdit(
		  this,
		  QStringList()
			  << "AdvSceneSwitcher.action.macro.type.nestedMacro.conditionHelp"
			  << "AdvSceneSwitcher.action.macro.type.nestedMacro.actionHelp"
			  << "AdvSceneSwitcher.action.macro.type.nestedMacro.elseActionHelp"))
{
	populateActionSelection(_actions);
	populateConditionBehaviorSelection(_conditionBehaviors);
	populateActionSectionSelection(_actionSections);
	populateActionSelectionTypes(_actionSelectionType);
	populateActionTypes(_actionTypes);

	_conditionMacros->HideSelectedMacro();

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_actionSelectionType, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ActionSelectionTypeChanged(int)));
	QWidget::connect(_actionIndex,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(ActionIndexChanged(const IntVariable &)));
	QWidget::connect(_label, SIGNAL(editingFinished()), this,
			 SLOT(LabelChanged()));
	QWidget::connect(_actionTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionTypeChanged(int)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_conditionMacros,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ConditionMacroChanged(const QString &)));
	QWidget::connect(_conditionBehaviors, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionBehaviorChanged(int)));
	QWidget::connect(_actionSections, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ActionSectionChanged(int)));
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
	layout->addWidget(_nestedMacro);
	setLayout(layout);
	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void HighlightMacroSettingsButton(bool enable);

MacroActionMacroEdit::~MacroActionMacroEdit()
{
	HighlightMacroSettingsButton(false);

	if (!_entryData) {
		return;
	}
	_entryData->_customWidgetHeight = GetCustomHeight();
	_nestedMacro->SetMacro({}); // Save splitter states
}

void MacroActionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_actionSelectionType->setCurrentIndex(_actionSelectionType->findData(
		static_cast<int>(_entryData->_actionSelectionType)));
	_actionIndex->SetValue(_entryData->_actionIndex);
	_actionIndex->SetMacro(_entryData->_macro.GetMacro());
	_label->setText(_entryData->_label);
	_actionTypes->setCurrentIndex(_actionTypes->findData(
		QString::fromStdString(_entryData->_actionId)));
	_regex->SetRegexConfig(_entryData->_regex);
	_macros->SetCurrentMacro(_entryData->_macro);
	_conditionMacros->SetCurrentMacro(_entryData->_runOptions.macro);
	_conditionBehaviors->setCurrentIndex(
		static_cast<int>(_entryData->_runOptions.logic));
	_reevaluateConditionState->setChecked(
		_entryData->_runOptions.reevaluateConditionState);
	_actionSections->setCurrentIndex(
		_entryData->_runOptions.runElseActions ? 1 : 0);
	_skipWhenPaused->setChecked(_entryData->_runOptions.skipWhenPaused);
	_setInputs->setChecked(_entryData->_runOptions.setInputs);
	SetupMacroInput(_entryData->_macro.GetMacro().get());

	const auto &macro = _entryData->_nestedMacro;
	_nestedMacro->SetMacro(macro);

	SetWidgetVisibility();
}

QWidget *MacroActionMacroEdit::Create(QWidget *parent,
				      std::shared_ptr<MacroAction> action)
{
	return new MacroActionMacroEdit(
		parent, std::dynamic_pointer_cast<MacroActionMacro>(action));
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

void MacroActionMacroEdit::ActionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionMacro::Action>(
		_actions->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionSelectionTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionSelectionType =
		static_cast<MacroActionMacro::SelectionType>(
			_actionSelectionType->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionMacroEdit::ActionIndexChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionIndex = value;
}

void MacroActionMacroEdit::LabelChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_label = _label->text().toStdString();
}

void MacroActionMacroEdit::ActionTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionId =
		_actionTypes->itemData(idx).toString().toStdString();
}

void MacroActionMacroEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = regex;
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

void MacroActionMacroEdit::ActionSectionChanged(int useElse)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_runOptions.runElseActions = useElse;
	_entryData->_useElseSection = useElse;
	_actionIndex->SetType(useElse ? MacroSegmentSelection::Type::ELSE_ACTION
				      : MacroSegmentSelection::Type::ACTION);
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
	_entryLayout->removeWidget(_actionSections);
	_entryLayout->removeWidget(_label);
	_entryLayout->removeWidget(_regex);
	_entryLayout->removeWidget(_actionTypes);
	_entryLayout->removeWidget(_actionSelectionType);
	_conditionLayout->removeWidget(_conditionBehaviors);
	_conditionLayout->removeWidget(_conditionMacros);

	ClearLayout(_entryLayout);
	ClearLayout(_conditionLayout);

	const std::unordered_map<std::string, QWidget *> placeholders = {
		{"{{actions}}", _actions},
		{"{{actionIndex}}", _actionIndex},
		{"{{macros}}", _macros},
		{"{{actionSections}}", _actionSections},
		{"{{conditionBehaviors}}", _conditionBehaviors},
		{"{{conditionMacros}}", _conditionMacros},
		{"{{actionSelectionType}}", _actionSelectionType},
		{"{{label}}", _label},
		{"{{regex}}", _regex},
		{"{{actionTypes}}", _actionTypes},

	};

	const auto action = _entryData->_action;
	const char *layoutText = "";
	switch (action) {
	case MacroActionMacro::Action::PAUSE:
	case MacroActionMacro::Action::UNPAUSE:
	case MacroActionMacro::Action::TOGGLE_PAUSE:
	case MacroActionMacro::Action::RESET_COUNTER:
	case MacroActionMacro::Action::STOP:
	case MacroActionMacro::Action::NESTED_MACRO:
		layoutText = "AdvSceneSwitcher.action.macro.layout.other";
		break;
	case MacroActionMacro::Action::RUN_ACTIONS:
		layoutText = "AdvSceneSwitcher.action.macro.layout.run";
		break;
	case MacroActionMacro::Action::DISABLE_ACTION:
	case MacroActionMacro::Action::ENABLE_ACTION:
	case MacroActionMacro::Action::TOGGLE_ACTION:
		layoutText = "AdvSceneSwitcher.action.macro.layout.actionState";
		break;
	default:
		break;
	}

	PlaceWidgets(obs_module_text(layoutText), _entryLayout, placeholders);

	if (_entryData->_runOptions.logic ==
	    MacroActionMacro::RunOptions::Logic::IGNORE_CONDITIONS) {
		_conditionLayout->addWidget(_conditionBehaviors);
		_conditionLayout->addStretch();
	} else {
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.macro.layout.run.condition"),
			_conditionLayout, placeholders);
	}

	if (action == MacroActionMacro::Action::RUN_ACTIONS ||
	    action == MacroActionMacro::Action::STOP) {
		_macros->HideSelectedMacro();
	} else {
		_macros->ShowAllMacros();
	}

	const auto actionSelectionType = _entryData->_actionSelectionType;
	const bool isModifyingActionState =
		action == MacroActionMacro::Action::DISABLE_ACTION ||
		action == MacroActionMacro::Action::ENABLE_ACTION ||
		action == MacroActionMacro::Action::TOGGLE_ACTION;
	_actionSelectionType->setVisible(isModifyingActionState);
	_actionIndex->setVisible(
		isModifyingActionState &&
		actionSelectionType == MacroActionMacro::SelectionType::INDEX);
	_label->setVisible(isModifyingActionState &&
			   actionSelectionType ==
				   MacroActionMacro::SelectionType::LABEL);
	_regex->setVisible(isModifyingActionState &&
			   actionSelectionType ==
				   MacroActionMacro::SelectionType::LABEL);
	_actionTypes->setVisible(isModifyingActionState &&
				 actionSelectionType ==
					 MacroActionMacro::SelectionType::ID);

	SetLayoutVisible(_conditionLayout,
			 action == MacroActionMacro::Action::RUN_ACTIONS);
	const bool needsAdditionalConditionWidgets =
		action == MacroActionMacro::Action::RUN_ACTIONS &&
		_entryData->_runOptions.logic !=
			MacroActionMacro::RunOptions::Logic::IGNORE_CONDITIONS;
	_conditionMacros->setVisible(needsAdditionalConditionWidgets);
	SetLayoutVisible(_reevaluateConditionStateLayout,
			 needsAdditionalConditionWidgets);
	SetLayoutVisible(_setInputsLayout,
			 action == MacroActionMacro::Action::RUN_ACTIONS);
	_inputs->setVisible(action == MacroActionMacro::Action::RUN_ACTIONS &&
			    _entryData->_runOptions.setInputs);
	HighlightMacroSettingsButton(
		action == MacroActionMacro::Action::RUN_ACTIONS &&
		_entryData->_runOptions.setInputs &&
		!_inputs->HasInputsToSet());
	_actionSections->setVisible(
		action == MacroActionMacro::Action::RUN_ACTIONS ||
		isModifyingActionState);
	_skipWhenPaused->setVisible(action ==
				    MacroActionMacro::Action::RUN_ACTIONS);

	_nestedMacro->setVisible(action ==
				 MacroActionMacro::Action::NESTED_MACRO);
	_macros->setVisible(action != MacroActionMacro::Action::NESTED_MACRO);
	SetResizingEnabled(action == MacroActionMacro::Action::NESTED_MACRO);

	if (_nestedMacro->IsEmpty()) {
		_nestedMacro->ShowAllMacroSections();
		// TODO: find a better solution than setting a fixed height
		_entryData->_customWidgetHeight = 600;
	}
	SetCustomHeight(_entryData->_customWidgetHeight);

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
