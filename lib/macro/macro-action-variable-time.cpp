#include "macro-action-variable-time.hpp"
#include "scene-switch-helpers.hpp"

#include <QDateTime>

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{70,
	 {
		 {Action::CURRENT_DATE_TIME,
		  "AdvSceneSwitcher.action.variable.type.currentDateTime",
		  "AdvSceneSwitcher.action.variable.layout.currentDateTime",
		  true},
		 {Action::DATE_TIME_WITH_OFFSET,
		  "AdvSceneSwitcher.action.variable.type.dateTimeWithOffset",
		  "AdvSceneSwitcher.action.variable.layout.dateTimeWithOffset",
		  true},
		 {Action::UNIX_TIMESTAMP,
		  "AdvSceneSwitcher.action.variable.type.unixTimestamp",
		  "AdvSceneSwitcher.action.variable.layout.timeUnit"},
		 {Action::TIME_SINCE_SCENE_CHANGE,
		  "AdvSceneSwitcher.action.variable.type.timeSinceSceneChange",
		  "AdvSceneSwitcher.action.variable.layout.timeUnit"},
	 },
	 []() { return std::make_unique<VariableActionTime>(); },
	 VariableActionTimeEdit::Create});

std::unique_ptr<VariableActionGroup> VariableActionTime::Copy() const
{
	return std::make_unique<VariableActionTime>(*this);
}

void VariableActionTime::Save(obs_data_t *obj,
			      const MacroActionVariable &) const
{
	_dateFormat.Save(obj, "dateFormat");
	obs_data_set_int(obj, "timeUnit", static_cast<int>(_timeUnit));
	_dateOffset.Save(obj, "dateOffset");
	obs_data_set_int(obj, "dateOffsetUnit",
			 static_cast<int>(_dateOffsetUnit));
}

void VariableActionTime::Load(obs_data_t *obj, MacroActionVariable &)
{
	if (obs_data_has_user_value(obj, "dateFormat")) {
		_dateFormat.Load(obj, "dateFormat");
	}
	_timeUnit = static_cast<TimeUnit>(obs_data_get_int(obj, "timeUnit"));
	if (obs_data_has_user_value(obj, "dateOffset")) {
		_dateOffset.Load(obj, "dateOffset");
	}
	obs_data_set_default_int(obj, "dateOffsetUnit",
				 static_cast<int>(OffsetUnit::DAYS));
	_dateOffsetUnit = static_cast<OffsetUnit>(
		obs_data_get_int(obj, "dateOffsetUnit"));
}

void VariableActionTime::ResolveVariables()
{
	_dateFormat.ResolveVariables();
	_dateOffset.ResolveVariables();
}

void VariableActionTime::SetToTimeSinceSceneChange(Variable &var) const
{
	const auto lastChange = GetLastSceneChangeTime();
	// Default constructed until the first scene change was observed
	if (lastChange == std::chrono::high_resolution_clock::time_point{}) {
		return;
	}

	const auto elapsed =
		std::chrono::high_resolution_clock::now() - lastChange;
	if (_timeUnit == TimeUnit::MILLISECONDS) {
		var.SetValue(std::to_string(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				elapsed)
				.count()));
		return;
	}
	var.SetValue(std::to_string(
		std::chrono::duration_cast<std::chrono::seconds>(elapsed)
			.count()));
}

QDateTime VariableActionTime::GetDateTimeWithOffset() const
{
	const auto now = QDateTime::currentDateTime();
	const int offset = _dateOffset.GetValue();

	switch (_dateOffsetUnit) {
	case OffsetUnit::SECONDS:
		return now.addSecs(offset);
	case OffsetUnit::MINUTES:
		return now.addSecs(static_cast<qint64>(offset) * 60);
	case OffsetUnit::HOURS:
		return now.addSecs(static_cast<qint64>(offset) * 60 * 60);
	case OffsetUnit::DAYS:
		return now.addDays(offset);
	case OffsetUnit::WEEKS:
		return now.addDays(static_cast<qint64>(offset) * 7);
	case OffsetUnit::MONTHS:
		return now.addMonths(offset);
	case OffsetUnit::YEARS:
		return now.addYears(offset);
	}
	return now;
}

bool VariableActionTime::Perform(MacroActionVariable &,
				 MacroActionVariable::Action type,
				 Variable &var)
{
	const auto format = QString::fromStdString(_dateFormat);

	switch (type) {
	case Action::CURRENT_DATE_TIME:
		var.SetValue(QDateTime::currentDateTime()
				     .toString(format)
				     .toStdString());
		break;
	case Action::DATE_TIME_WITH_OFFSET:
		var.SetValue(
			GetDateTimeWithOffset().toString(format).toStdString());
		break;
	case Action::UNIX_TIMESTAMP: {
		const auto now = QDateTime::currentDateTime();
		const auto timestamp = _timeUnit == TimeUnit::MILLISECONDS
					       ? now.toMSecsSinceEpoch()
					       : now.toSecsSinceEpoch();
		var.SetValue(std::to_string(timestamp));
		break;
	}
	case Action::TIME_SINCE_SCENE_CHANGE:
		SetToTimeSinceSceneChange(var);
		break;
	default:
		break;
	}
	return true;
}

static void populateTimeUnitSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.seconds"),
		      static_cast<int>(VariableActionTime::TimeUnit::SECONDS));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.unit.milliseconds"),
		static_cast<int>(VariableActionTime::TimeUnit::MILLISECONDS));
}

static void populateOffsetUnitSelection(QComboBox *list)
{
	using Unit = VariableActionTime::OffsetUnit;
	const std::vector<std::pair<Unit, const char *>> units = {
		{Unit::SECONDS, "AdvSceneSwitcher.unit.seconds"},
		{Unit::MINUTES, "AdvSceneSwitcher.unit.minutes"},
		{Unit::HOURS, "AdvSceneSwitcher.unit.hours"},
		{Unit::DAYS, "AdvSceneSwitcher.unit.days"},
		{Unit::WEEKS, "AdvSceneSwitcher.unit.weeks"},
		{Unit::MONTHS, "AdvSceneSwitcher.unit.months"},
		{Unit::YEARS, "AdvSceneSwitcher.unit.years"},
	};
	for (const auto &[unit, name] : units) {
		list->addItem(obs_module_text(name), static_cast<int>(unit));
	}
}

VariableActionTimeEdit::VariableActionTimeEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionTime>()
			  : nullptr),
	  _dateFormat(new VariableLineEdit(this)),
	  _dateFormatHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.currentDateTime.help"),
		  this)),
	  _timeUnit(new QComboBox(this)),
	  _dateOffset(new VariableSpinBox(this)),
	  _dateOffsetUnit(new QComboBox(this))
{
	_dateOffset->setMinimum(-99999);
	_dateOffset->setMaximum(99999);
	populateTimeUnitSelection(_timeUnit);
	populateOffsetUnitSelection(_dateOffsetUnit);

	QWidget::connect(_dateFormat, SIGNAL(editingFinished()), this,
			 SLOT(DateFormatChanged()));
	QWidget::connect(_timeUnit, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TimeUnitChanged(int)));
	QWidget::connect(
		_dateOffset,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(DateOffsetChanged(const NumberVariable<int> &)));
	QWidget::connect(_dateOffsetUnit, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(DateOffsetUnitChanged(int)));
}

VariableActionGroupEdit *
VariableActionTimeEdit::Create(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionTimeEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionTimeEdit::GetInlineWidgets()
{
	return {
		{"{{dateFormat}}", _dateFormat},
		{"{{dateFormatHelp}}", _dateFormatHelp},
		{"{{timeUnit}}", _timeUnit},
		{"{{dateOffset}}", _dateOffset},
		{"{{dateOffsetUnit}}", _dateOffsetUnit},
	};
}

void VariableActionTimeEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_dateFormat->setText(_data->_dateFormat);
	_timeUnit->setCurrentIndex(
		_timeUnit->findData(static_cast<int>(_data->_timeUnit)));
	_dateOffset->SetValue(_data->_dateOffset);
	_dateOffsetUnit->setCurrentIndex(_dateOffsetUnit->findData(
		static_cast<int>(_data->_dateOffsetUnit)));
	_loading = false;
}

void VariableActionTimeEdit::SetWidgetVisibility(MacroActionVariable::Action)
{
	hide();
}

void VariableActionTimeEdit::DateFormatChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_dateFormat = _dateFormat->text().toStdString();
}

void VariableActionTimeEdit::TimeUnitChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_data->_timeUnit = static_cast<VariableActionTime::TimeUnit>(
		_timeUnit->currentData().toInt());
}

void VariableActionTimeEdit::DateOffsetChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_dateOffset = value;
}

void VariableActionTimeEdit::DateOffsetUnitChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_data->_dateOffsetUnit = static_cast<VariableActionTime::OffsetUnit>(
		_dateOffsetUnit->currentData().toInt());
}

} // namespace advss
