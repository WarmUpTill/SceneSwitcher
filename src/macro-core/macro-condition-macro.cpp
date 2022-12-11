#include "macro-condition-edit.hpp"
#include "macro-condition-macro.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

const std::string MacroConditionMacro::id = "macro";

bool MacroConditionMacro::_registered = MacroConditionFactory::Register(
	MacroConditionMacro::id,
	{MacroConditionMacro::Create, MacroConditionMacroEdit::Create,
	 "AdvSceneSwitcher.condition.macro"});

static std::map<MacroConditionMacro::Type, std::string> macroConditionTypes = {
	{MacroConditionMacro::Type::COUNT,
	 "AdvSceneSwitcher.condition.macro.type.count"},
	{MacroConditionMacro::Type::STATE,
	 "AdvSceneSwitcher.condition.macro.type.state"},
	{MacroConditionMacro::Type::MULTI_STATE,
	 "AdvSceneSwitcher.condition.macro.type.multiState"},
};

static std::map<MacroConditionMacro::CounterCondition, std::string>
	counterConditionTypes = {
		{MacroConditionMacro::CounterCondition::BELOW,
		 "AdvSceneSwitcher.condition.macro.count.type.below"},
		{MacroConditionMacro::CounterCondition::ABOVE,
		 "AdvSceneSwitcher.condition.macro.count.type.above"},
		{MacroConditionMacro::CounterCondition::EQUAL,
		 "AdvSceneSwitcher.condition.macro.count.type.equal"},
};

static std::map<MacroConditionMacro::MultiStateCondition, std::string>
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
	if (!_macro.get()) {
		return false;
	}

	return _macro->Matched();
}

bool MacroConditionMacro::CheckMultiStateCondition()
{
	// Note:
	// Depending on the order the macro conditions are checked Matched() might
	// still return the state of the previous interval
	int matchedCount = 0;
	for (const auto &macro : _macros) {
		if (!macro.get()) {
			continue;
		}
		if (macro->Matched()) {
			matchedCount++;
		}
	}

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

bool MacroConditionMacro::CheckCountCondition()
{
	if (!_macro.get()) {
		return false;
	}

	switch (_counterCondition) {
	case CounterCondition::BELOW:
		return _macro->GetCount() < _count;
	case CounterCondition::ABOVE:
		return _macro->GetCount() > _count;
	case CounterCondition::EQUAL:
		return _macro->GetCount() == _count;
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
	obs_data_set_int(obj, "count", _count);
	obs_data_set_int(obj, "multiStateCount", _multiSateCount);
	obs_data_set_int(obj, "multiStateCondition",
			 static_cast<int>(_multiSateCondition));
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
	_count = obs_data_get_int(obj, "count");
	_multiSateCount = obs_data_get_int(obj, "multiStateCount");
	// TODO: Remove this fallback in future version
	if (!obs_data_has_user_value(obj, "multiStateCondition")) {
		_multiSateCondition = MultiStateCondition::ABOVE;
	} else {
		_multiSateCondition = static_cast<MultiStateCondition>(
			obs_data_get_int(obj, "multiStateCondition"));
	}
	return true;
}

std::string MacroConditionMacro::GetShortDesc() const
{
	if (_macro.get()) {
		return _macro->Name();
	}
	return "";
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
	  _types(new QComboBox(parent)),
	  _counterConditions(new QComboBox(parent)),
	  _count(new QSpinBox(parent)),
	  _currentCount(new QLabel(parent)),
	  _pausedWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.macro.pausedWarning"))),
	  _resetCount(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.macro.count.reset"))),
	  _settingsLine1(new QHBoxLayout),
	  _settingsLine2(new QHBoxLayout),
	  _macroList(new MacroList(this, false, false)),
	  _multiStateConditions(new QComboBox(parent)),
	  _multiStateCount(new QSpinBox(parent))
{
	_count->setMaximum(10000000);
	populateTypeSelection(_types);
	populateCounterConditionSelection(_counterConditions);
	populateMultiStateConditionSelection(_multiStateConditions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_types, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_counterConditions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(CountConditionChanged(int)));
	QWidget::connect(_count, SIGNAL(valueChanged(int)), this,
			 SLOT(CountChanged(int)));
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
	QWidget::connect(_multiStateCount, SIGNAL(valueChanged(int)), this,
			 SLOT(MultiStateCountChanged(int)));

	auto typesLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{types}}", _types},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.macro.type.selection"),
		     typesLayout, widgetPlaceholders);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(typesLayout);
	mainLayout->addLayout(_settingsLine1);
	mainLayout->addLayout(_settingsLine2);
	mainLayout->addWidget(_macroList);
	mainLayout->addWidget(_pausedWarning);
	setLayout(mainLayout);

	_entryData = entryData;

	connect(&_countTimer, SIGNAL(timeout()), this, SLOT(UpdateCount()));
	_countTimer.start(1000);

	_pausedWarning->setVisible(false);
	connect(&_pausedTimer, SIGNAL(timeout()), this, SLOT(UpdatePaused()));
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
	clearLayout(_settingsLine1);
	clearLayout(_settingsLine2);
}

void MacroConditionMacroEdit::SetupStateWidgets()
{
	ClearLayouts();

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.macro.state.entry"),
		_settingsLine1, widgetPlaceholders);
	SetWidgetVisibility();
	adjustSize();
}

void MacroConditionMacroEdit::SetupMultiStateWidgets()
{
	ClearLayouts();

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{multiStateConditions}}", _multiStateConditions},
		{"{{multiStateCount}}", _multiStateCount},
	};
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.multistate.entry"),
		_settingsLine1, widgetPlaceholders);
	SetWidgetVisibility();
	adjustSize();
}

void MacroConditionMacroEdit::SetupCountWidgets()
{
	ClearLayouts();

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
		{"{{conditions}}", _counterConditions},
		{"{{count}}", _count},
		{"{{currentCount}}", _currentCount},
		{"{{resetCount}}", _resetCount},
	};
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.count.entry.line1"),
		_settingsLine1, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.macro.count.entry.line2"),
		_settingsLine2, widgetPlaceholders);
	SetWidgetVisibility();
	adjustSize();
}

void MacroConditionMacroEdit::SetWidgetVisibility()
{
	switch (_entryData->_type) {
	case MacroConditionMacro::Type::COUNT:
		_macros->show();
		_counterConditions->show();
		_count->show();
		_currentCount->show();
		_pausedWarning->show();
		_resetCount->show();
		_macroList->hide();
		_multiStateConditions->hide();
		_multiStateCount->hide();
		break;
	case MacroConditionMacro::Type::STATE:
		_macros->show();
		_counterConditions->hide();
		_count->hide();
		_currentCount->hide();
		_pausedWarning->show();
		_resetCount->hide();
		_macroList->hide();
		_multiStateConditions->hide();
		_multiStateCount->hide();
		break;
	case MacroConditionMacro::Type::MULTI_STATE:
		_macros->hide();
		_counterConditions->hide();
		_count->hide();
		_currentCount->hide();
		_pausedWarning->hide();
		_resetCount->hide();
		_macroList->show();
		_multiStateConditions->show();
		_multiStateCount->show();
		break;
	default:
		break;
	}
}

void MacroConditionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	auto test = _entryData->_type;
	switch (test) {
	case MacroConditionMacro::Type::COUNT:
		SetupCountWidgets();
		break;
	case MacroConditionMacro::Type::STATE:
		SetupStateWidgets();
		break;
	case MacroConditionMacro::Type::MULTI_STATE:
		SetupMultiStateWidgets();
		break;
	default:
		break;
	}

	_macros->SetCurrentMacro(_entryData->_macro.get());
	_types->setCurrentIndex(static_cast<int>(_entryData->_type));
	_counterConditions->setCurrentIndex(
		static_cast<int>(_entryData->_counterCondition));
	_count->setValue(_entryData->_count);
	_macroList->SetContent(_entryData->_macros);
	_multiStateConditions->setCurrentIndex(
		static_cast<int>(_entryData->_multiSateCondition));
	_multiStateCount->setValue(_entryData->_multiSateCount);
}

void MacroConditionMacroEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macro.UpdateRef(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionMacroEdit::CountChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_count = value;
}

void MacroConditionMacroEdit::CountConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_counterCondition =
		static_cast<MacroConditionMacro::CounterCondition>(cond);
}

void MacroConditionMacroEdit::MacroRemove(const QString &)
{
	if (!_entryData) {
		return;
	}

	_entryData->_macro.UpdateRef();

	auto it = _entryData->_macros.begin();
	while (it != _entryData->_macros.end()) {
		it->UpdateRef();
		if (it->get() == nullptr) {
			it = _entryData->_macros.erase(it);
		} else {
			++it;
		}
	}
	adjustSize();
}

void MacroConditionMacroEdit::TypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroConditionMacro::Type>(type);

	switch (_entryData->_type) {
	case MacroConditionMacro::Type::COUNT:
		SetupCountWidgets();
		break;
	case MacroConditionMacro::Type::STATE:
		SetupStateWidgets();
		break;
	case MacroConditionMacro::Type::MULTI_STATE:
		SetupMultiStateWidgets();
		break;
	default:
		break;
	}
}

void MacroConditionMacroEdit::ResetClicked()
{
	if (_loading || !_entryData || !_entryData->_macro.get()) {
		return;
	}

	_entryData->_macro->ResetCount();
}

void MacroConditionMacroEdit::UpdateCount()
{
	if (_entryData && _entryData->_macro.get()) {
		_currentCount->setText(
			QString::number(_entryData->_macro->GetCount()));
	} else {
		_currentCount->setText("-");
	}
}

void MacroConditionMacroEdit::UpdatePaused()
{
	_pausedWarning->setVisible(
		_entryData &&
		_entryData->_type != MacroConditionMacro::Type::MULTI_STATE &&
		_entryData->_macro.get() && _entryData->_macro->Paused());
	adjustSize();
}

void MacroConditionMacroEdit::MultiStateConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_multiSateCondition =
		static_cast<MacroConditionMacro::MultiStateCondition>(cond);
}

void MacroConditionMacroEdit::MultiStateCountChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_multiSateCount = value;
}

void MacroConditionMacroEdit::Add(const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	adjustSize();
}

void MacroConditionMacroEdit::Remove(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	adjustSize();
}

void MacroConditionMacroEdit::Replace(int idx, const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	MacroRef macro(name);
	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros[idx] = macro;
	adjustSize();
}
