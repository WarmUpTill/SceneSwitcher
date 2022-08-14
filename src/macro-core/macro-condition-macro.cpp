#include "macro-condition-edit.hpp"
#include "macro-condition-macro.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

const std::string MacroConditionMacro::id = "macro";

bool MacroConditionMacro::_registered = MacroConditionFactory::Register(
	MacroConditionMacro::id,
	{MacroConditionMacro::Create, MacroConditionMacroEdit::Create,
	 "AdvSceneSwitcher.condition.macro"});

static std::map<MacroConditionMacroType, std::string> macroConditionTypes = {
	{MacroConditionMacroType::COUNT,
	 "AdvSceneSwitcher.condition.macro.type.count"},
	{MacroConditionMacroType::STATE,
	 "AdvSceneSwitcher.condition.macro.type.state"},
};

static std::map<CounterCondition, std::string> counterConditionTypes = {
	{CounterCondition::BELOW,
	 "AdvSceneSwitcher.condition.macro.count.type.below"},
	{CounterCondition::ABOVE,
	 "AdvSceneSwitcher.condition.macro.count.type.above"},
	{CounterCondition::EQUAL,
	 "AdvSceneSwitcher.condition.macro.count.type.equal"},
};

bool MacroConditionMacro::CheckStateCondition()
{
	// Note:
	// Depending on the order the macro conditions are checked Matched() might
	// still return the state of the previous interval
	return _macro->Matched();
}

bool MacroConditionMacro::CheckCountCondition()
{
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
	if (!_macro.get()) {
		return false;
	}

	switch (_type) {
	case MacroConditionMacroType::STATE:
		return CheckStateCondition();
		break;
	case MacroConditionMacroType::COUNT:
		return CheckCountCondition();
		break;
	default:
		break;
	}

	return false;
}

bool MacroConditionMacro::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_macro.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_int(obj, "condition", static_cast<int>(_counterCondition));
	obs_data_set_int(obj, "count", _count);
	return true;
}

bool MacroConditionMacro::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_macro.Load(obj);
	_type = static_cast<MacroConditionMacroType>(
		obs_data_get_int(obj, "type"));
	_counterCondition = static_cast<CounterCondition>(
		obs_data_get_int(obj, "condition"));
	_count = obs_data_get_int(obj, "count");
	return true;
}

std::string MacroConditionMacro::GetShortDesc()
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

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : counterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionMacroEdit::MacroConditionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMacro> entryData)
	: QWidget(parent)
{
	_macros = new MacroSelection(parent);
	_types = new QComboBox(parent);
	_counterConditions = new QComboBox(parent);
	_count = new QSpinBox(parent);
	_currentCount = new QLabel(parent);
	_resetCount = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.condition.macro.count.reset"));
	_pausedWarning = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.macro.pausedWarning"));

	_count->setMaximum(10000000);
	populateTypeSelection(_types);
	populateConditionSelection(_counterConditions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_types, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_counterConditions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionChanged(int)));
	QWidget::connect(_count, SIGNAL(valueChanged(int)), this,
			 SLOT(CountChanged(int)));
	QWidget::connect(_resetCount, SIGNAL(clicked()), this,
			 SLOT(ResetClicked()));

	auto typesLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{types}}", _types},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.macro.type.selection"),
		     typesLayout, widgetPlaceholders);
	_settingsLine1 = new QHBoxLayout;
	_settingsLine2 = new QHBoxLayout;
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(typesLayout);
	mainLayout->addLayout(_settingsLine1);
	mainLayout->addLayout(_settingsLine2);
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
	clearLayout(_settingsLine1);
	clearLayout(_settingsLine2);
}

void MacroConditionMacroEdit::SetupStateWidgets()
{
	ClearLayouts();

	_counterConditions->hide();
	_count->hide();
	_currentCount->hide();
	_resetCount->hide();

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.macro.state.entry"),
		_settingsLine1, widgetPlaceholders);
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

	_counterConditions->show();
	_count->show();
	_currentCount->show();
	_resetCount->show();

	adjustSize();
}

void MacroConditionMacroEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	if (_entryData->_type == MacroConditionMacroType::STATE) {
		SetupStateWidgets();
	} else {
		SetupCountWidgets();
	}

	_macros->SetCurrentMacro(_entryData->_macro.get());
	_types->setCurrentIndex(static_cast<int>(_entryData->_type));
	_counterConditions->setCurrentIndex(
		static_cast<int>(_entryData->_counterCondition));
	_count->setValue(_entryData->_count);
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

void MacroConditionMacroEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_counterCondition = static_cast<CounterCondition>(cond);
}

void MacroConditionMacroEdit::MacroRemove(const QString &)
{
	if (_entryData) {
		_entryData->_macro.UpdateRef();
	}
}

void MacroConditionMacroEdit::TypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroConditionMacroType>(type);
	if (_entryData->_type == MacroConditionMacroType::STATE) {
		SetupStateWidgets();
	} else {
		SetupCountWidgets();
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
	_pausedWarning->setVisible(_entryData && _entryData->_macro.get() &&
				   _entryData->_macro->Paused());
	adjustSize();
}
