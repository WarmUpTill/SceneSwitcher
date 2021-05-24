#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-counter.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionCounter::id = "counter";

bool MacroConditionCounter::_registered = MacroConditionFactory::Register(
	MacroConditionCounter::id,
	{MacroConditionCounter::Create, MacroConditionCounterEdit::Create,
	 "AdvSceneSwitcher.condition.counter"});

static std::map<CounterCondition, std::string> counterConditionTypes = {
	{CounterCondition::BELOW,
	 "AdvSceneSwitcher.condition.counter.type.below"},
	{CounterCondition::ABOVE,
	 "AdvSceneSwitcher.condition.counter.type.above"},
	{CounterCondition::EQUAL,
	 "AdvSceneSwitcher.condition.counter.type.equal"},
};

bool MacroConditionCounter::CheckCondition()
{
	if (!_macro.get()) {
		return false;
	}

	switch (_condition) {
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

bool MacroConditionCounter::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_macro.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "count", _count);
	return true;
}

bool MacroConditionCounter::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_macro.Load(obj);
	_condition = static_cast<CounterCondition>(
		obs_data_get_int(obj, "condition"));
	_count = obs_data_get_int(obj, "count");
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : counterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionCounterEdit::MacroConditionCounterEdit(
	QWidget *parent, std::shared_ptr<MacroConditionCounter> entryData)
	: QWidget(parent)
{
	_macros = new MacroSelection(parent);
	_conditions = new QComboBox();
	_count = new QSpinBox();
	_currentCount = new QLabel();
	_resetCount = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.counter.reset"));

	_count->setMaximum(10000000);
	populateConditionSelection(_conditions);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_count, SIGNAL(valueChanged(int)), this,
			 SLOT(CountChanged(int)));
	QWidget::connect(_resetCount, SIGNAL(clicked()), this,
			 SLOT(ResetClicked()));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	QHBoxLayout *line1Layout = new QHBoxLayout;
	QHBoxLayout *line2Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
		{"{{conditions}}", _conditions},
		{"{{count}}", _count},
		{"{{currentCount}}", _currentCount},
		{"{{resetCount}}", _resetCount},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.counter.entry.line1"),
		     line1Layout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.counter.entry.line2"),
		     line2Layout, widgetPlaceholders);
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionCounterEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_macros->SetCurrentMacro(_entryData->_macro.get());
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_count->setValue(_entryData->_count);
	ResetTimer();
}

void MacroConditionCounterEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macro.UpdateRef(text);
	ResetTimer();
}

void MacroConditionCounterEdit::CountChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_count = value;
}

void MacroConditionCounterEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<CounterCondition>(cond);
}

void MacroConditionCounterEdit::MacroRemove(const QString &name)
{
	UNUSED_PARAMETER(name);
	if (_entryData) {
		_entryData->_macro.UpdateRef();
	}
}

void MacroConditionCounterEdit::ResetClicked()
{
	if (_loading || !_entryData || !_entryData->_macro.get()) {
		return;
	}

	_entryData->_macro->ResetCount();
	ResetTimer();
}

void MacroConditionCounterEdit::UpdateCount()
{
	if (_entryData && _entryData->_macro.get()) {
		_currentCount->setText(
			QString::number(_entryData->_macro->GetCount()));
	} else {
		_currentCount->setText("-");
	}
}

void MacroConditionCounterEdit::ResetTimer()
{
	_timer.reset(new QTimer(this));
	connect(_timer.get(), SIGNAL(timeout()), this, SLOT(UpdateCount()));
	_timer->start(1000);
}
