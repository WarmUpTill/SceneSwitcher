#include "macro-condition-macro.hpp"
#include "layout-helpers.hpp"
#include "macro-action-edit.hpp"
#include "macro-signals.hpp"
#include "macro.hpp"

namespace advss {

const std::string MacroConditionMacro::id = "macro";

bool MacroConditionMacro::_registered = MacroConditionFactory::Register(
	MacroConditionMacro::id,
	{MacroConditionMacro::Create, MacroConditionMacroEdit::Create,
	 "AdvSceneSwitcher.condition.macro"});

const static std::map<MacroConditionMacro::Type, std::string>
	macroConditionTypes = {
		{MacroConditionMacro::Type::COUNT,
		 "AdvSceneSwitcher.condition.macro.type.count"},
		{MacroConditionMacro::Type::STATE,
		 "AdvSceneSwitcher.condition.macro.type.state"},
		{MacroConditionMacro::Type::MULTI_STATE,
		 "AdvSceneSwitcher.condition.macro.type.multiState"},
		{MacroConditionMacro::Type::ACTION_DISABLED,
		 "AdvSceneSwitcher.condition.macro.type.actionDisabled"},
		{MacroConditionMacro::Type::ACTION_ENABLED,
		 "AdvSceneSwitcher.condition.macro.type.actionEnabled"},
		{MacroConditionMacro::Type::PAUSED,
		 "AdvSceneSwitcher.condition.macro.type.paused"},
		{MacroConditionMacro::Type::ACTIONS_PERFORMED,
		 "AdvSceneSwitcher.condition.macro.type.actionsPerformed"},
};

const static std::map<MacroConditionMacro::CounterCondition, std::string>
	counterConditionTypes = {
		{MacroConditionMacro::CounterCondition::BELOW,
		 "AdvSceneSwitcher.condition.macro.count.type.below"},
		{MacroConditionMacro::CounterCondition::ABOVE,
		 "AdvSceneSwitcher.condition.macro.count.type.above"},
		{MacroConditionMacro::CounterCondition::EQUAL,
		 "AdvSceneSwitcher.condition.macro.count.type.equal"},
};

const static std::map<MacroConditionMacro::MultiStateCondition, std::string>
	multiStateConditionTypes = {
		{MacroConditionMacro::MultiStateCondition::BELOW,
		 "AdvSceneSwitcher.condition.macro.state.type.below"},
		{MacroConditionMacro::MultiStateCondition::EQUAL,
		 "AdvSceneSwitcher.condition.macro.state.type.equal"},
		{MacroConditionMacro::MultiStateCondition::ABOVE,
		 "AdvSceneSwitcher.condition.macro.state.type.above"},
};

bool MacroConditionMacro::CheckStateCondition()
{
	// Note:
	// Depending on the order the macro conditions are checked Matched() might
	// still return the state of the previous interval
	auto macro = _macro.GetMacro();
	if (!macro) {
		return false;
	}

	return macro->ConditionsMatched();
}

bool MacroConditionMacro::CheckMultiStateCondition()
{
	// Note:
	// Depending on the order the macro conditions are checked Matched() might
	// still return the state of the previous interval
	int matchedCount = 0;
	for (const auto &m : _macros) {
		auto macro = m.GetMacro();
		if (!macro) {
			continue;
		}
		if (macro->ConditionsMatched()) {
			matchedCount++;
		}
	}

	SetTempVarValue("matchedCount", std::to_string(matchedCount));

	switch (_multiSateCondition) {
	case MacroConditionMacro::MultiStateCondition::BELOW:
		return matchedCount < _multiSateCount;
	case MacroConditionMacro::MultiStateCondition::EQUAL:
		return matchedCount == _multiSateCount;
	case MacroConditionMacro::MultiStateCondition::ABOVE:
		return matchedCount > _multiSateCount;
	default:
		break;
	}

	return false;
}

bool MacroConditionMacro::CheckActionStateCondition()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return false;
	}
	if (!IsValidMacroSegmentIndex(macro.get(), _actionIndex - 1, false)) {
		return false;
	}
	if (_type == Type::ACTION_DISABLED) {
		return !macro->Actions().at(_actionIndex - 1)->Enabled();
	} else if (_type == Type::ACTION_ENABLED) {
		return macro->Actions().at(_actionIndex - 1)->Enabled();
	}

	return false;
}

bool MacroConditionMacro::CheckPauseState()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return false;
	}

	return macro->Paused();
}

bool MacroConditionMacro::CheckActionsPerformed()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return false;
	}

	return macro->WasExecutedSince(macro->LastConditionCheckTime());
}

bool MacroConditionMacro::CheckCountCondition()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return false;
	}

	SetTempVarValue("runCount", std::to_string(macro->RunCount()));

	switch (_counterCondition) {
	case CounterCondition::BELOW:
		return macro->RunCount() < _count;
	case CounterCondition::ABOVE:
		return macro->RunCount() > _count;
	case CounterCondition::EQUAL:
		return macro->RunCount() == _count;
	default:
		break;
	}

	return false;
}

bool MacroConditionMacro::CheckCondition()
{
	switch (_type) {
	case Type::STATE:
		return CheckStateCondition();
	case Type::MULTI_STATE:
		return CheckMultiStateCondition();
	case Type::COUNT:
		return CheckCountCondition();
	case Type::ACTION_DISABLED:
	case Type::ACTION_ENABLED:
		return CheckActionStateCondition();
	case Type::PAUSED:
		return CheckPauseState();
	case Type::ACTIONS_PERFORMED:
		return CheckActionsPerformed();
	default:
		break;
	}

	return false;
}

bool MacroConditionMacro::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	SaveMacroList(obj, _macros);
	_macro.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_int(obj, "condition", static_cast<int>(_counterCondition));
	_count.Save(obj, "count");
	_multiSateCount.Save(obj, "multiStateCount");
	obs_data_set_int(obj, "multiStateCondition",
			 static_cast<int>(_multiSateCondition));
	_actionIndex.Save(obj, "actionIndex");
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionMacro::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	LoadMacroList(obj, _macros);
	_macro.Load(obj);
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_counterCondition = static_cast<CounterCondition>(
		obs_data_get_int(obj, "condition"));
	_actionIndex.Load(obj, "actionIndex");
	// TODO: Remove this fallback in future version
	if (!obs_data_has_user_value(obj, "multiStateCondition")) {
		_multiSateCondition = MultiStateCondition::ABOVE;
	} else {
		_multiSateCondition = static_cast<MultiStateCondition>(
			obs_data_get_int(obj, "multiStateCondition"));
	}

	if (!obs_data_has_user_value(obj, "version")) {
		_count = obs_data_get_int(obj, "count");
		_multiSateCount = obs_data_get_int(obj, "multiStateCount");
	} else {
		_count.Load(obj, "count");
		_multiSateCount.Load(obj, "multiStateCount");
	}

	return true;
}

bool MacroConditionMacro::PostLoad()
{
	return MacroCondition::PostLoad() && MacroRefCondition::PostLoad() &&
	       MultiMacroRefCondition::PostLoad();
}

std::string MacroConditionMacro::GetShortDesc() const
{
	return _macro.Name();
}

void MacroConditionMacro::SetType(Type type)
{
	_type = type;
	SetupTempVars();
}

void MacroConditionMacro::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_type) {
	case Type::COUNT:
		AddTempvar(
			"runCount",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.macro.runCount"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.macro.runCount.description"));
		break;
	case Type::STATE:
		break;
	case Type::MULTI_STATE:
		AddTempvar(
			"matchedCount",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.macro.matchedCount"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.macro.matchedCount.description"));
		break;
	case Type::ACTION_DISABLED:
		break;
	case Type::ACTION_ENABLED:
		break;
	case Type::PAUSED:
		break;
	default:
		break;
	}
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : macroConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateCounterConditionSelection(QComboBox *list)
{
	for (auto entry : counterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateMultiStateConditionSelection(QComboBox *list)
{
	for (auto entry : multiStateConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionMacroEdit::MacroConditionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMacro> entryData)
	: QWidget(parent),
	  _macros(new MacroSelection(parent)),
	  _types(new QComboBox()),
	  _counterConditions(new QComboBox()),
	  _count(new VariableSpinBox()),
	  _currentCount(new QLabel()),
	  _pausedWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.macro.pausedWarning"))),
	  _resetCount(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.macro.count.reset"))),
	  _settingsLine1(new QHBoxLayout()),
	  _settingsLine2(new QHBoxLayout()),
	  _macroList(new MacroList(this, false, false)),
	  _multiStateConditions(new QComboBox()),
	  _multiStateCount(new VariableSpinBox()),
	  _actionIndex(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::ACTION))
{
	_count->setMaximum(10000000);
	populateTypeSelection(_types);
	populateCounterConditionSelection(_counterConditions);
	populateMultiStateConditionSelection(_multiStateConditions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(MacroSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_types, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_counterConditions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(CountConditionChanged(int)));
	QWidget::connect(
		_count,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(CountChanged(const NumberVariable<int> &)));
	QWidget::connect(_resetCount, SIGNAL(clicked()), this,
			 SLOT(ResetClicked()));
	QWidget::connect(_macroList, SIGNAL(Added(const std::string &)), this,
			 SLOT(Add(const std::string &)));
	QWidget::connect(_macroList, SIGNAL(Removed(int)), this,
			 SLOT(Remove(int)));
	QWidget::connect(_macroList, SIGNAL(Replaced(int, const std::string &)),
			 this, SLOT(Replace(int, const std::string &)));
	QWidget::connect(_multiStateConditions,
			 SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MultiStateConditionChanged(int)));
	QWidget::connect(
		_multiStateCount,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(MultiStateCountChanged(const NumberVariable<int> &)));
	QWidget::connect(_actionIndex,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(ActionIndexChanged(const IntVariable &)));

	auto typesLayout = new QHBoxLayout();
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{types}}", _types},
	};
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.macro.type.selection"),
		     typesLayout, widgetPlaceholders);

	_settingsLine1->addWidget(_macros);
	_settingsLine1->addWidget(_counterConditions);
	_settingsLine1->addWidget(_count);
	_settingsLine2->addWidget(_currentCount);
	_settingsLine2->addWidget(_resetCount);
	_settingsLine1->addWidget(_multiStateConditions);
	_settingsLine1->addWidget(_multiStateCount);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(typesLayout);
	mainLayout->addLayout(_settingsLine1);
	mainLayout->addLayout(_settingsLine2);
	mainLayout->addWidget(_macroList);
	mainLayout->addWidget(_pausedWarning);
	setLayout(mainLayout);

	_entryData = entryData;

	QWidget::connect(&_countTimer, SIGNAL(timeout()), this,
			 SLOT(UpdateCount()));
	_countTimer.start(1000);

	_pausedWarning->setVisible(false);
	QWidget::connect(&_pausedTimer, SIGNAL(timeout()), this,
			 SLOT(UpdatePaused()));
	_pausedTimer.start(1000);

	UpdateEntryData();
	_loading = false;
}

void MacroConditionMacroEdit::ClearLayouts()
{
	_settingsLine1->removeWidget(_macros);
	_settingsLine1->removeWidget(_counterConditions);
	_settingsLine1->removeWidget(_count);
	_settingsLine2->removeWidget(_currentCount);
	_settingsLine2->removeWidget(_resetCount);
	_settingsLine1->removeWidget(_multiStateConditions);
	_settingsLine1->removeWidget(_multiStateCount);
	_settingsLine1->removeWidget(_actionIndex);
	ClearLayout(_settingsLine1);
	ClearLayout(_settingsLine2);
}

void MacroConditionMacroEdit::SetupWidgets()
{
	SetWidgetVisibility();
	ClearLayouts();

	switch (_entryData->GetType()) {
	case MacroConditionMacro::Type::COUNT:
		SetupCountWidgets();
		break;
	case MacroConditionMacro::Type::STATE:
		SetupStateWidgets();
		break;
	case MacroConditionMacro::Type::MULTI_STATE:
		SetupMultiStateWidgets();
		break;
	case MacroConditionMacro::Type::ACTION_DISABLED:
		SetupActionStateWidgets(false);
		break;
	case MacroConditionMacro::Type::ACTION_ENABLED:
		SetupActionStateWidgets(true);
		break;
	case MacroConditionMacro::Type::PAUSED:
		SetupPauseWidgets();
		break;
	case MacroConditionMacro::Type::ACTIONS_PERFORMED:
		SetupActionsPerformedWidgets();
		break;
	default:
		break;
	}
}

void MacroConditionMacroEdit::SetupStateWidgets()
{
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.macro.state.entry"),
		_settingsLine1, {{"{{macros}}", _macros}});
}

void MacroConditionMacroEdit::SetupMultiStateWidgets()
{
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.multistate.entry"),
		_settingsLine1,
		{{"{{multiStateConditions}}", _multiStateConditions},
		 {"{{multiStateCount}}", _multiStateCount}});
}

void MacroConditionMacroEdit::SetupCountWidgets()
{
	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
		{"{{conditions}}", _counterConditions},
		{"{{count}}", _count},
		{"{{currentCount}}", _currentCount},
		{"{{resetCount}}", _resetCount},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.count.entry.line1"),
		_settingsLine1, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.count.entry.line2"),
		_settingsLine2, widgetPlaceholders);
}

void MacroConditionMacroEdit::SetupActionStateWidgets(bool enable)
{
	PlaceWidgets(
		obs_module_text(
			enable ? "AdvSceneSwitcher.condition.macro.actionState.enabled.entry"
			       : "AdvSceneSwitcher.condition.macro.actionState.disabled.entry"),
		_settingsLine1,
		{{"{{macros}}", _macros}, {"{{actionIndex}}", _actionIndex}});
}

void MacroConditionMacroEdit::SetupPauseWidgets()
{
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.macro.paused.entry"),
		     _settingsLine1, {{"{{macros}}", _macros}});
}

void MacroConditionMacroEdit::SetupActionsPerformedWidgets()
{
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.actionsPerformed.entry"),
		_settingsLine1, {{"{{macros}}", _macros}});
}

void MacroConditionMacroEdit::SetWidgetVisibility()
{
	_macros->setVisible(
		_entryData->GetType() == MacroConditionMacro::Type::COUNT ||
		_entryData->GetType() == MacroConditionMacro::Type::STATE ||
		_entryData->GetType() ==
			MacroConditionMacro::Type::ACTION_DISABLED ||
		_entryData->GetType() ==
			MacroConditionMacro::Type::ACTION_ENABLED ||
		_entryData->GetType() == MacroConditionMacro::Type::PAUSED ||
		_entryData->GetType() ==
			MacroConditionMacro::Type::ACTIONS_PERFORMED);
	_counterConditions->setVisible(_entryData->GetType() ==
				       MacroConditionMacro::Type::COUNT);
	_count->setVisible(_entryData->GetType() ==
			   MacroConditionMacro::Type::COUNT);
	_currentCount->setVisible(_entryData->GetType() ==
				  MacroConditionMacro::Type::COUNT);
	_resetCount->setVisible(_entryData->GetType() ==
				MacroConditionMacro::Type::COUNT);
	_macroList->setVisible(_entryData->GetType() ==
			       MacroConditionMacro::Type::MULTI_STATE);
	_multiStateConditions->setVisible(
		_entryData->GetType() ==
		MacroConditionMacro::Type::MULTI_STATE);
	_multiStateCount->setVisible(_entryData->GetType() ==
				     MacroConditionMacro::Type::MULTI_STATE);
	_actionIndex->setVisible(
		_entryData->GetType() ==
			MacroConditionMacro::Type::ACTION_DISABLED ||
		_entryData->GetType() ==
			MacroConditionMacro::Type::ACTION_ENABLED);
	if (_entryData->GetType() == MacroConditionMacro::Type::MULTI_STATE ||
	    _entryData->GetType() ==
		    MacroConditionMacro::Type::ACTION_DISABLED ||
	    _entryData->GetType() ==
		    MacroConditionMacro::Type::ACTION_ENABLED ||
	    _entryData->GetType() == MacroConditionMacro::Type::PAUSED) {
		_pausedWarning->hide();
	}

	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	SetupWidgets();
	_macros->SetCurrentMacro(_entryData->_macro);
	_types->setCurrentIndex(static_cast<int>(_entryData->GetType()));
	_counterConditions->setCurrentIndex(
		static_cast<int>(_entryData->_counterCondition));
	_count->SetValue(_entryData->_count);
	_macroList->SetContent(_entryData->_macros);
	_multiStateConditions->setCurrentIndex(
		static_cast<int>(_entryData->_multiSateCondition));
	_multiStateCount->SetValue(_entryData->_multiSateCount);
	_actionIndex->SetValue(_entryData->_actionIndex);
	_actionIndex->SetMacro(_entryData->_macro.GetMacro());
	SetWidgetVisibility();
}

void MacroConditionMacroEdit::MacroChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macro = text;
	_actionIndex->SetMacro(_entryData->_macro.GetMacro());
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionMacroEdit::CountChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_count = value;
}

void MacroConditionMacroEdit::CountConditionChanged(int cond)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_counterCondition =
		static_cast<MacroConditionMacro::CounterCondition>(cond);
}

void MacroConditionMacroEdit::MacroRemove(const QString &)
{
	if (!_entryData) {
		return;
	}

	auto it = _entryData->_macros.begin();
	while (it != _entryData->_macros.end()) {
		if (!it->GetMacro()) {
			it = _entryData->_macros.erase(it);
		} else {
			++it;
		}
	}
	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::TypeChanged(int type)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetType(static_cast<MacroConditionMacro::Type>(type));
	SetupWidgets();
}

void MacroConditionMacroEdit::ResetClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	auto macro = _entryData->_macro.GetMacro();
	if (!macro) {
		return;
	}
	macro->ResetRunCount();
}

void MacroConditionMacroEdit::UpdateCount()
{
	if (!_entryData) {
		return;
	}

	auto macro = _entryData->_macro.GetMacro();
	if (macro) {
		_currentCount->setText(QString::number(macro->RunCount()));
	} else {
		_currentCount->setText("-");
	}
}

void MacroConditionMacroEdit::UpdatePaused()
{
	auto macro = _entryData->_macro.GetMacro();
	_pausedWarning->setVisible(
		_entryData &&
		_entryData->GetType() !=
			MacroConditionMacro::Type::MULTI_STATE &&
		macro && macro->Paused());
	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::MultiStateConditionChanged(int cond)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_multiSateCondition =
		static_cast<MacroConditionMacro::MultiStateCondition>(cond);
}

void MacroConditionMacroEdit::MultiStateCountChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_multiSateCount = value;
}

void MacroConditionMacroEdit::Add(const std::string &name)
{
	GUARD_LOADING_AND_LOCK();
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::Remove(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::Replace(int idx, const std::string &name)
{
	GUARD_LOADING_AND_LOCK();
	MacroRef macro(name);
	_entryData->_macros[idx] = macro;
	adjustSize();
	updateGeometry();
}

void MacroConditionMacroEdit::ActionIndexChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionIndex = value;
}

} // namespace advss
