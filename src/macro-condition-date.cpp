#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-date.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionDate::id = "date";

bool MacroConditionDate::_registered = MacroConditionFactory::Register(
	MacroConditionDate::id,
	{MacroConditionDate::Create, MacroConditionDateEdit::Create,
	 "AdvSceneSwitcher.condition.date", false});

static std::map<DateCondition, std::string> dateConditionTypes = {
	{DateCondition::AT, "AdvSceneSwitcher.condition.date.state.at"},
	{DateCondition::AFTER, "AdvSceneSwitcher.condition.date.state.after"},
	{DateCondition::BEFORE, "AdvSceneSwitcher.condition.date.state.before"},
	{DateCondition::BETWEEN,
	 "AdvSceneSwitcher.condition.date.state.between"},
};

bool MacroConditionDate::CheckCondition()
{
	bool match = false;
	QDateTime cur = QDateTime::currentDateTime();
	if (_ignoreDate) {
		_dateTime.setDate(cur.date());
		_dateTime2.setDate(cur.date());
	}
	if (_ignoreTime) {
		_dateTime.setTime(cur.time());
		_dateTime2.setTime(cur.time());
	}

	switch (_condition) {
	case DateCondition::AT:
		match = _dateTime >= cur &&
			_dateTime <= cur.addMSecs(switcher->interval);
		break;
	case DateCondition::AFTER:
		match = cur >= _dateTime;
		break;
	case DateCondition::BEFORE:
		match = cur <= _dateTime;
		break;
	case DateCondition::BETWEEN:
		if (_dateTime2 > _dateTime) {
			match = cur >= _dateTime && cur <= _dateTime2;
		} else {
			match = cur >= _dateTime2 && cur <= _dateTime;
		}
		break;
	default:
		break;
	}

	if (match && _repeat) {
		_dateTime = _dateTime.addSecs(_duration.seconds);
		_dateTime2 = _dateTime2.addSecs(_duration.seconds);
	}

	return match;
}

bool MacroConditionDate::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "dateTime",
			    _dateTime.toString().toStdString().c_str());
	obs_data_set_string(obj, "dateTime2",
			    _dateTime2.toString().toStdString().c_str());
	obs_data_set_bool(obj, "ignoreDate", _ignoreDate);
	obs_data_set_bool(obj, "ignoreTime", _ignoreTime);
	obs_data_set_bool(obj, "repeat", _repeat);
	_duration.Save(obj);

	return true;
}

bool MacroConditionDate::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition =
		static_cast<DateCondition>(obs_data_get_int(obj, "condition"));
	_dateTime = QDateTime::fromString(
		QString::fromStdString(obs_data_get_string(obj, "dateTime")));
	_dateTime2 = QDateTime::fromString(
		QString::fromStdString(obs_data_get_string(obj, "dateTime2")));
	_ignoreDate = obs_data_get_bool(obj, "ignoreDate");
	_ignoreTime = obs_data_get_bool(obj, "ignoreTime");
	_repeat = obs_data_get_bool(obj, "repeat");
	_duration.Load(obj);
	return true;
}

std::string MacroConditionDate::GetShortDesc()
{
	return _dateTime.toString().toStdString();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : dateConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionDateEdit::MacroConditionDateEdit(
	QWidget *parent, std::shared_ptr<MacroConditionDate> entryData)
	: QWidget(parent)
{
	_condition = new QComboBox();
	_dateTime = new QDateTimeEdit();
	_dateTime->setCalendarPopup(true);
	_dateTime->setDisplayFormat("yyyy.MM.dd hh:mm:ss");
	_dateTime2 = new QDateTimeEdit();
	_dateTime2->setCalendarPopup(true);
	_dateTime2->setDisplayFormat("yyyy.MM.dd hh:mm:ss");
	_ignoreDate = new QCheckBox();
	_ignoreTime = new QCheckBox();
	_repeat = new QCheckBox();
	_duration = new DurationSelection();

	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_dateTime, SIGNAL(dateTimeChanged(const QDateTime &)),
			 this, SLOT(DateTimeChanged(const QDateTime &)));
	QWidget::connect(_dateTime2, SIGNAL(dateTimeChanged(const QDateTime &)),
			 this, SLOT(DateTime2Changed(const QDateTime &)));
	QWidget::connect(_ignoreDate, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreDateChanged(int)));
	QWidget::connect(_ignoreTime, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreTimeChanged(int)));
	QWidget::connect(_repeat, SIGNAL(stateChanged(int)), this,
			 SLOT(RepeatChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));

	populateConditionSelection(_condition);

	auto line1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{condition}}", _condition},
		{"{{dateTime}}", _dateTime},
		{"{{dateTime2}}", _dateTime2},
		{"{{ignoreDate}}", _ignoreDate},
		{"{{ignoreTime}}", _ignoreTime},
		{"{{repeat}}", _repeat},
		{"{{duration}}", _duration},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.date.entry.line1"),
		line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.date.entry.line2"),
		line2Layout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionDateEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<DateCondition>(cond);
	ShowSecondDateSelection(_entryData->_condition ==
				DateCondition::BETWEEN);
}

void MacroConditionDateEdit::DateTimeChanged(const QDateTime &datetime)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_dateTime = datetime;

	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::DateTime2Changed(const QDateTime &datetime)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_dateTime2 = datetime;
}

void MacroConditionDateEdit::IgnoreDateChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_ignoreDate = state;
}

void MacroConditionDateEdit::IgnoreTimeChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_ignoreTime = state;
}

void MacroConditionDateEdit::RepeatChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_repeat = state;
	_duration->setDisabled(!state);
}

void MacroConditionDateEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionDateEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionDateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_dateTime->setDateTime(_entryData->_dateTime);
	_dateTime2->setDateTime(_entryData->_dateTime2);
	_ignoreDate->setChecked(_entryData->_ignoreDate);
	_ignoreTime->setChecked(_entryData->_ignoreTime);
	_repeat->setChecked(_entryData->_repeat);
	_duration->SetDuration(_entryData->_duration);
	_duration->setDisabled(!_entryData->_repeat);
	ShowSecondDateSelection(_entryData->_condition ==
				DateCondition::BETWEEN);
}

void MacroConditionDateEdit::ShowSecondDateSelection(bool visible)
{
	_dateTime2->setVisible(visible);
	adjustSize();
}
