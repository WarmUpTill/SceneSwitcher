#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-date.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionDate::id = "date";

bool MacroConditionDate::_registered = MacroConditionFactory::Register(
	MacroConditionDate::id,
	{MacroConditionDate::Create, MacroConditionDateEdit::Create,
	 "AdvSceneSwitcher.condition.date", false});

bool MacroConditionDate::CheckCondition()
{
	bool match = false;
	QDateTime cur = QDateTime::currentDateTime();
	if (_ignoreDate) {
		_dateTime.setDate(cur.date());
	}
	if (_ignoreTime) {
		_dateTime.setTime(cur.time());
	}

	match = _dateTime >= cur &&
		_dateTime <= cur.addMSecs(switcher->interval);
	if (match && _repeat) {
		_dateTime = _dateTime.addSecs(_duration.seconds);
	}

	return match;
}

bool MacroConditionDate::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "dateTime",
			    _dateTime.toString().toStdString().c_str());
	obs_data_set_bool(obj, "ignoreDate", _ignoreDate);
	obs_data_set_bool(obj, "ignoreTime", _ignoreTime);
	obs_data_set_bool(obj, "repeat", _repeat);
	_duration.Save(obj);

	return true;
}

bool MacroConditionDate::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_dateTime = QDateTime::fromString(
		QString::fromStdString(obs_data_get_string(obj, "dateTime")));
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

MacroConditionDateEdit::MacroConditionDateEdit(
	QWidget *parent, std::shared_ptr<MacroConditionDate> entryData)
	: QWidget(parent)
{
	_dateTime = new QDateTimeEdit();
	_dateTime->setCalendarPopup(true);
	_dateTime->setDisplayFormat("yyyy.MM.dd hh:mm:ss");
	_ignoreDate = new QCheckBox();
	_ignoreTime = new QCheckBox();
	_repeat = new QCheckBox();
	_duration = new DurationSelection();

	QWidget::connect(_dateTime, SIGNAL(dateTimeChanged(const QDateTime &)),
			 this, SLOT(DateTimeChanged(const QDateTime &)));
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

	auto line1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{dateTime}}", _dateTime},
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
	_dateTime->setDateTime(_entryData->_dateTime);
	_ignoreDate->setChecked(_entryData->_ignoreDate);
	_ignoreTime->setChecked(_entryData->_ignoreTime);
	_repeat->setChecked(_entryData->_repeat);
	_duration->SetDuration(_entryData->_duration);
	_duration->setDisabled(!_entryData->_repeat);
}
