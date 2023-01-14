#include "macro-condition-edit.hpp"
#include "macro-condition-date.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <QCalendarWidget>

const std::string MacroConditionDate::id = "date";
constexpr const char dateFormat[] = "yyyy MM dd hh mm ss";

bool MacroConditionDate::_registered = MacroConditionFactory::Register(
	MacroConditionDate::id,
	{MacroConditionDate::Create, MacroConditionDateEdit::Create,
	 "AdvSceneSwitcher.condition.date", false});

static std::map<MacroConditionDate::Condition, std::string> dateConditionTypes =
	{
		{MacroConditionDate::Condition::AT,
		 "AdvSceneSwitcher.condition.date.state.at"},
		{MacroConditionDate::Condition::AFTER,
		 "AdvSceneSwitcher.condition.date.state.after"},
		{MacroConditionDate::Condition::BEFORE,
		 "AdvSceneSwitcher.condition.date.state.before"},
		{MacroConditionDate::Condition::BETWEEN,
		 "AdvSceneSwitcher.condition.date.state.between"},
		{MacroConditionDate::Condition::PATTERN,
		 "AdvSceneSwitcher.condition.date.state.pattern"},
};

static std::map<MacroConditionDate::Condition, std::string> weekConditionTypes =
	{
		{MacroConditionDate::Condition::AT,
		 "AdvSceneSwitcher.condition.date.state.at"},
		{MacroConditionDate::Condition::AFTER,
		 "AdvSceneSwitcher.condition.date.state.after"},
		{MacroConditionDate::Condition::BEFORE,
		 "AdvSceneSwitcher.condition.date.state.before"},
};

static std::map<MacroConditionDate::Day, std::string> dayOfWeekNames = {
	{MacroConditionDate::Day::ANY,
	 "AdvSceneSwitcher.condition.date.anyDay"},
	{MacroConditionDate::Day::MONDAY,
	 "AdvSceneSwitcher.condition.date.monday"},
	{MacroConditionDate::Day::TUESDAY,
	 "AdvSceneSwitcher.condition.date.tuesday"},
	{MacroConditionDate::Day::WEDNESDAY,
	 "AdvSceneSwitcher.condition.date.wednesday"},
	{MacroConditionDate::Day::THURSDAY,
	 "AdvSceneSwitcher.condition.date.thursday"},
	{MacroConditionDate::Day::FRIDAY,
	 "AdvSceneSwitcher.condition.date.friday"},
	{MacroConditionDate::Day::SATURDAY,
	 "AdvSceneSwitcher.condition.date.saturday"},
	{MacroConditionDate::Day::SUNDAY,
	 "AdvSceneSwitcher.condition.date.sunday"},
};

bool MacroConditionDate::CheckDayOfWeek(int64_t msSinceLastCheck)
{
	QDateTime cur = QDateTime::currentDateTime();
	SetVariableValue(cur.toString().toStdString());
	if (_dayOfWeek != Day::ANY &&
	    cur.date().dayOfWeek() != static_cast<int>(_dayOfWeek)) {
		return false;
	}
	if (_ignoreTime) {
		return true;
	}
	_dateTime.setDate(cur.date());

	switch (_condition) {
	case MacroConditionDate::Condition::AT:
		return _dateTime <= cur &&
		       _dateTime >= cur.addMSecs(-msSinceLastCheck);
	case MacroConditionDate::Condition::AFTER:
		return cur >= _dateTime;
	case MacroConditionDate::Condition::BEFORE:
		return cur <= _dateTime;
	default:
		break;
	}
	return false;
}

bool MacroConditionDate::CheckBetween(const QDateTime &now)
{
	// In the case of ignoring the date component treat the "left" value as
	// the start of the time range and the "right" value as the end
	if (_ignoreDate) {
		if (_dateTime2 >= _dateTime) {
			return now >= _dateTime && now <= _dateTime2;
		}
		// Assume that the end / start value should be shifted by 24h
		// as otherwise the condition would never be true
		return (now >= _dateTime && now <= _dateTime2.addDays(1)) ||
		       (now >= _dateTime.addDays(-1) && now <= _dateTime2);
	}

	if (_dateTime2 >= _dateTime) {
		return now >= _dateTime && now <= _dateTime2;
	}
	return now >= _dateTime2 && now <= _dateTime;
}

bool MacroConditionDate::CheckPattern(QDateTime now,
				      int64_t secondsSinceLastCheck)
{
	auto regex = QRegularExpression(QRegularExpression::anchoredPattern(
		QString::fromStdString(_pattern)));
	if (!regex.isValid()) {
		return false;
	}

	// Silently ignore cases in which the scene switcher was not able to
	// run for more than 60s
	if (secondsSinceLastCheck > 60) {
		secondsSinceLastCheck = 0;
	}

	for (int i = 0; i <= secondsSinceLastCheck; ++i) {
		auto match = regex.match(now.toString(dateFormat));
		if (match.hasMatch()) {
			return true;
		}
		now = now.addSecs(-1);
	}
	return false;
}

bool MacroConditionDate::CheckRegularDate(int64_t msSinceLastCheck)
{
	bool match = false;
	QDateTime cur = QDateTime::currentDateTime();
	SetVariableValue(cur.toString().toStdString());

	if (_ignoreDate) {
		_dateTime.setDate(cur.date());
		_dateTime2.setDate(cur.date());
	}
	if (_ignoreTime) {
		_dateTime.setTime(cur.time());
		_dateTime2.setTime(cur.time());
	}

	switch (_condition) {
	case MacroConditionDate::Condition::AT:
		match = _dateTime <= cur &&
			_dateTime >= cur.addMSecs(-msSinceLastCheck);
		break;
	case MacroConditionDate::Condition::AFTER:
		match = cur >= _dateTime;
		break;
	case MacroConditionDate::Condition::BEFORE:
		match = cur <= _dateTime;
		break;
	case MacroConditionDate::Condition::BETWEEN:
		match = CheckBetween(cur);
		break;
	case MacroConditionDate::Condition::PATTERN:
		match = CheckPattern(cur, msSinceLastCheck / 1000);
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

bool MacroConditionDate::CheckCondition()
{
	auto m = GetMacro();
	if (!m) {
		return false;
	}
	auto msSinceLastCheck = m->MsSinceLastCheck();
	if (_dayOfWeekCheck) {
		return CheckDayOfWeek(msSinceLastCheck);
	}
	return CheckRegularDate(msSinceLastCheck);
}

bool MacroConditionDate::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "dayOfWeek", static_cast<int>(_dayOfWeek));
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	const auto &dateToSave = _updateOnRepeat ? _dateTime : _origDateTime;
	const auto &dateToSave2 = _updateOnRepeat ? _dateTime2 : _origDateTime2;
	obs_data_set_string(obj, "dateTime",
			    dateToSave.toString().toStdString().c_str());
	obs_data_set_string(obj, "dateTime2",
			    dateToSave2.toString().toStdString().c_str());
	obs_data_set_bool(obj, "ignoreDate", _ignoreDate);
	obs_data_set_bool(obj, "ignoreTime", _ignoreTime);
	obs_data_set_bool(obj, "repeat", _repeat);
	obs_data_set_bool(obj, "updateOnRepeat", _updateOnRepeat);
	_duration.Save(obj);
	obs_data_set_bool(obj, "dayOfWeekCheck", _dayOfWeekCheck);
	obs_data_set_string(obj, "pattern", _pattern.c_str());
	return true;
}

bool MacroConditionDate::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_dayOfWeek = static_cast<Day>(obs_data_get_int(obj, "dayOfWeek"));
	_condition = static_cast<MacroConditionDate::Condition>(
		obs_data_get_int(obj, "condition"));
	_dateTime = QDateTime::fromString(
		QString::fromStdString(obs_data_get_string(obj, "dateTime")));
	_origDateTime = _dateTime;
	_dateTime2 = QDateTime::fromString(
		QString::fromStdString(obs_data_get_string(obj, "dateTime2")));
	_origDateTime2 = _dateTime2;
	_ignoreDate = obs_data_get_bool(obj, "ignoreDate");
	_ignoreTime = obs_data_get_bool(obj, "ignoreTime");
	_repeat = obs_data_get_bool(obj, "repeat");
	_updateOnRepeat = obs_data_get_bool(obj, "updateOnRepeat");
	_duration.Load(obj);
	_dayOfWeekCheck = obs_data_get_bool(obj, "dayOfWeekCheck");
	_pattern = obs_data_get_string(obj, "pattern");

	// The following code is used to avoid issues with old save files in
	// which the simple date check did not support setting _condition
	if (_dayOfWeekCheck &&
	    _condition == MacroConditionDate::Condition::BETWEEN) {
		_condition = MacroConditionDate::Condition::AT;
	}
	return true;
}

std::string MacroConditionDate::GetShortDesc() const
{
	if (_dayOfWeekCheck) {
		auto it = dayOfWeekNames.find(_dayOfWeek);
		if (it == dayOfWeekNames.end()) {
			return "";
		}
		std::string dayName = obs_module_text(it->second.c_str());
		if (_ignoreTime) {
			return dayName;
		}
		return dayName + " " +
		       _dateTime.time().toString().toStdString();
	} else {
		if (_condition == Condition::PATTERN) {
			return "";
		}

		if (!_ignoreDate && !_ignoreTime) {
			return GetDateTime1().toString().toStdString();
		} else if (_ignoreDate && !_ignoreTime) {
			return GetDateTime1().time().toString().toStdString();
		} else if (!_ignoreDate && _ignoreTime) {
			return GetDateTime1().date().toString().toStdString();
		}
	}
	return "";
}

void MacroConditionDate::SetDate1(const QDate &date)
{
	_dateTime.setDate(date);
	_origDateTime.setDate(date);
}

void MacroConditionDate::SetDate2(const QDate &date)
{
	_dateTime2.setDate(date);
	_origDateTime2.setDate(date);
}

void MacroConditionDate::SetTime1(const QTime &time)
{
	_dateTime.setTime(time);
	_origDateTime.setTime(time);
}

void MacroConditionDate::SetTime2(const QTime &time)
{
	_dateTime2.setTime(time);
	_origDateTime2.setTime(time);
}

QDateTime MacroConditionDate::GetDateTime1() const
{
	return _repeat && _updateOnRepeat ? _dateTime : _origDateTime;
}

QDateTime MacroConditionDate::GetDateTime2() const
{
	return _repeat && _updateOnRepeat ? _dateTime2 : _origDateTime2;
}

QDateTime MacroConditionDate::GetNextMatchDateTime() const
{
	return _dateTime;
}

static inline void populateDaySelection(QComboBox *list)
{
	for (auto entry : dayOfWeekNames) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : dateConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateWeekConditionSelection(QComboBox *list)
{
	for (auto entry : weekConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionDateEdit::MacroConditionDateEdit(
	QWidget *parent, std::shared_ptr<MacroConditionDate> entryData)
	: QWidget(parent),
	  _weekCondition(new QComboBox()),
	  _dayOfWeek(new QComboBox()),
	  _ignoreWeekTime(new QCheckBox()),
	  _weekTime(new QTimeEdit()),
	  _condition(new QComboBox()),
	  _date(new QDateEdit()),
	  _time(new QTimeEdit()),
	  _separator(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.date.separator"))),
	  _date2(new QDateEdit()),
	  _time2(new QTimeEdit()),
	  _ignoreDate(new QCheckBox()),
	  _ignoreTime(new QCheckBox()),
	  _repeat(new QCheckBox()),
	  _nextMatchDate(new QLabel()),
	  _updateOnRepeat(new QCheckBox()),
	  _duration(new DurationSelection()),
	  _pattern(new QLineEdit()),
	  _currentDate(new QLabel()),
	  _advancedSettingsTooggle(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.date.showAdvancedSettings"))),
	  _simpleLayout(new QHBoxLayout()),
	  _advancedLayout(new QHBoxLayout()),
	  _repeatLayout(new QVBoxLayout()),
	  _repeatUpdateLayout(new QHBoxLayout()),
	  _patternLayout(new QHBoxLayout())
{
	_ignoreWeekTime->setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.date.ignoreTime"));
	_weekTime->setDisplayFormat("hh:mm:ss");
	_date->setDisplayFormat("yyyy.MM.dd ");
	_date->setCalendarPopup(true);
	auto cal = _date->calendarWidget();
	cal->showSelectedDate();
	_time->setDisplayFormat("hh:mm:ss");
	_date2->setDisplayFormat("yyyy.MM.dd ");
	_date2->setCalendarPopup(true);
	cal = _date2->calendarWidget();
	cal->showSelectedDate();
	_time2->setDisplayFormat("hh:mm:ss");
	_ignoreDate->setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.date.ignoreDate"));
	_ignoreTime->setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.date.ignoreTime"));

	QWidget::connect(_weekCondition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_dayOfWeek, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(DayOfWeekChanged(int)));
	QWidget::connect(_ignoreWeekTime, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreTimeChanged(int)));
	QWidget::connect(_weekTime, SIGNAL(timeChanged(const QTime &)), this,
			 SLOT(TimeChanged(const QTime &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_date, SIGNAL(dateChanged(const QDate &)), this,
			 SLOT(DateChanged(const QDate &)));
	QWidget::connect(_date2, SIGNAL(dateChanged(const QDate &)), this,
			 SLOT(Date2Changed(const QDate &)));
	QWidget::connect(_time, SIGNAL(timeChanged(const QTime &)), this,
			 SLOT(TimeChanged(const QTime &)));
	QWidget::connect(_time2, SIGNAL(timeChanged(const QTime &)), this,
			 SLOT(Time2Changed(const QTime &)));
	QWidget::connect(_ignoreDate, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreDateChanged(int)));
	QWidget::connect(_ignoreTime, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreTimeChanged(int)));
	QWidget::connect(_repeat, SIGNAL(stateChanged(int)), this,
			 SLOT(RepeatChanged(int)));
	QWidget::connect(_updateOnRepeat, SIGNAL(stateChanged(int)), this,
			 SLOT(UpdateOnRepeatChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_advancedSettingsTooggle, SIGNAL(clicked()), this,
			 SLOT(AdvancedSettingsToggleClicked()));
	QWidget::connect(_pattern, SIGNAL(editingFinished()), this,
			 SLOT(PatternChanged()));

	populateDaySelection(_dayOfWeek);
	populateConditionSelection(_condition);
	populateWeekConditionSelection(_weekCondition);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{weekCondition}}", _weekCondition},
		{"{{dayOfWeek}}", _dayOfWeek},
		{"{{ignoreWeekTime}}", _ignoreWeekTime},
		{"{{weekTime}}", _weekTime},
		{"{{condition}}", _condition},
		{"{{date}}", _date},
		{"{{time}}", _time},
		{"{{separator}}", _separator},
		{"{{date2}}", _date2},
		{"{{time2}}", _time2},
		{"{{ignoreDate}}", _ignoreDate},
		{"{{ignoreTime}}", _ignoreTime},
		{"{{repeat}}", _repeat},
		{"{{updateOnRepeat}}", _updateOnRepeat},
		{"{{duration}}", _duration},
		{"{{pattern}}", _pattern},
		{"{{currentDate}}", _currentDate},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.date.entry.simple"),
		_simpleLayout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.date.entry.advanced"),
		     _advancedLayout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.date.entry.updateOnRepeat"),
		_repeatUpdateLayout, widgetPlaceholders);
	auto repeatLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.date.entry.repeat"),
		repeatLayout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.date.entry.pattern"),
		     _patternLayout, widgetPlaceholders);
	_repeatLayout->addLayout(repeatLayout);
	_repeatLayout->addWidget(_nextMatchDate);
	_repeatLayout->addLayout(_repeatUpdateLayout);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_simpleLayout);
	mainLayout->addLayout(_advancedLayout);
	mainLayout->addLayout(_patternLayout);
	mainLayout->addLayout(_repeatLayout);
	auto *_advancedToggleLayout = new QHBoxLayout;
	_advancedToggleLayout->addWidget(_advancedSettingsTooggle);
	_advancedToggleLayout->addStretch();
	mainLayout->addLayout(_advancedToggleLayout);
	setLayout(mainLayout);

	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(ShowNextMatch()));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateCurrentTime()));
	_timer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionDateEdit::DayOfWeekChanged(int day)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_dayOfWeek = static_cast<MacroConditionDate::Day>(day);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition =
		static_cast<MacroConditionDate::Condition>(cond);
	SetWidgetStatus();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::DateChanged(const QDate &date)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->SetDate1(date);

	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::TimeChanged(const QTime &time)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->SetTime1(time);

	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::Date2Changed(const QDate &date)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->SetDate2(date);
}

void MacroConditionDateEdit::Time2Changed(const QTime &time)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->SetTime2(time);
}

void MacroConditionDateEdit::IgnoreDateChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_ignoreDate = !state;
	SetWidgetStatus();
}

void MacroConditionDateEdit::IgnoreTimeChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_ignoreTime = !state;
	SetWidgetStatus();
}

void MacroConditionDateEdit::RepeatChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_repeat = state;
	_duration->setDisabled(!state);
	SetWidgetStatus();
}

void MacroConditionDateEdit::UpdateOnRepeatChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_updateOnRepeat = state;
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

void MacroConditionDateEdit::AdvancedSettingsToggleClicked()
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_dayOfWeekCheck = !_entryData->_dayOfWeekCheck;
		_entryData->_condition = MacroConditionDate::Condition::AT;
	}
	_condition->setCurrentIndex(0);
	_weekCondition->setCurrentIndex(0);
	SetWidgetStatus();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionDateEdit::ShowNextMatch()
{
	if (!_entryData || _entryData->_dayOfWeekCheck ||
	    !_entryData->_repeat) {
		return;
	}
	QString format(obs_module_text(
		"AdvSceneSwitcher.condition.date.entry.nextMatchDate"));
	_nextMatchDate->setText(
		format.arg(_entryData->GetNextMatchDateTime().toString()));
}

void MacroConditionDateEdit::PatternChanged()
{
	if (_loading || !_entryData) {
		return;
	}
	_entryData->_pattern = _pattern->text().toStdString();
}

void MacroConditionDateEdit::UpdateCurrentTime()
{
	QDateTime dateTime = dateTime.currentDateTime();
	_currentDate->setText(dateTime.toString(dateFormat));
}

void MacroConditionDateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_weekCondition->setCurrentIndex(
		static_cast<int>(_entryData->_condition));
	_dayOfWeek->setCurrentIndex(
		static_cast<int>(static_cast<int>(_entryData->_dayOfWeek)));
	_ignoreWeekTime->setChecked(!_entryData->_ignoreTime);
	_weekTime->setTime(_entryData->GetDateTime1().time());
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_date->setDate(_entryData->GetDateTime1().date());
	_time->setTime(_entryData->GetDateTime1().time());
	_date2->setDate(_entryData->GetDateTime2().date());
	_time2->setTime(_entryData->GetDateTime2().time());
	_ignoreDate->setChecked(!_entryData->_ignoreDate);
	_ignoreTime->setChecked(!_entryData->_ignoreTime);
	_repeat->setChecked(_entryData->_repeat);
	_updateOnRepeat->setChecked(_entryData->_updateOnRepeat);
	_duration->SetDuration(_entryData->_duration);
	_duration->setDisabled(!_entryData->_repeat);
	_pattern->setText(QString::fromStdString(_entryData->_pattern));
	ShowNextMatch();
	SetWidgetStatus();
}

void MacroConditionDateEdit::SetupSimpleView()
{
	setLayoutVisible(_simpleLayout, true);
	setLayoutVisible(_advancedLayout, false);
	setLayoutVisible(_patternLayout, false);
	setLayoutVisible(_repeatLayout, false);
	setLayoutVisible(_repeatUpdateLayout, false);
	_weekTime->setDisabled(_entryData->_ignoreTime);
	_weekCondition->setDisabled(_entryData->_ignoreTime);
	const QSignalBlocker b(_weekTime);
	_weekTime->setTime(_entryData->GetDateTime1().time());
	_advancedSettingsTooggle->setText(obs_module_text(
		"AdvSceneSwitcher.condition.date.showAdvancedSettings"));
}

void MacroConditionDateEdit::SetupAdvancedView()
{
	setLayoutVisible(_simpleLayout, false);
	setLayoutVisible(_advancedLayout, true);
	setLayoutVisible(_patternLayout, false);
	setLayoutVisible(_repeatLayout, true);
	setLayoutVisible(_repeatUpdateLayout, _entryData->_repeat);
	_nextMatchDate->setVisible(_entryData->_repeat);
	_date->setDisabled(_entryData->_ignoreDate);
	_date2->setDisabled(_entryData->_ignoreDate);
	_time->setDisabled(_entryData->_ignoreTime);
	_time2->setDisabled(_entryData->_ignoreTime);
	ShowFirstDateSelection(true);
	ShowSecondDateSelection(_entryData->_condition ==
				MacroConditionDate::Condition::BETWEEN);
	_advancedSettingsTooggle->setText(obs_module_text(
		"AdvSceneSwitcher.condition.date.showSimpleSettings"));
}

void MacroConditionDateEdit::SetupPatternView()
{
	setLayoutVisible(_simpleLayout, false);
	setLayoutVisible(_advancedLayout, true);
	setLayoutVisible(_patternLayout, true);
	setLayoutVisible(_repeatLayout, false);
	setLayoutVisible(_repeatUpdateLayout, false);
	ShowFirstDateSelection(false);
	ShowSecondDateSelection(false);
}

void MacroConditionDateEdit::SetWidgetStatus()
{
	if (!_entryData) {
		return;
	}

	if (_entryData->_dayOfWeekCheck) {
		SetupSimpleView();
	} else if (_entryData->_condition ==
		   MacroConditionDate::Condition::PATTERN) {
		SetupPatternView();
	} else {
		SetupAdvancedView();
	}
	adjustSize();
}

void MacroConditionDateEdit::ShowFirstDateSelection(bool visible)
{
	_ignoreDate->setVisible(visible);
	_date->setVisible(visible);
	_ignoreTime->setVisible(visible);
	_time->setVisible(visible);
}

void MacroConditionDateEdit::ShowSecondDateSelection(bool visible)
{
	_separator->setVisible(visible);
	_date2->setVisible(visible);
	_time2->setVisible(visible);
}
