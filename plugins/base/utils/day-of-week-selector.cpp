#include "day-of-week-selector.hpp"
#include "obs-module-helper.hpp"

#include <QPushButton>
#include <QHBoxLayout>

namespace advss {

DayOfWeekSelector::DayOfWeekSelector(QWidget *parent) : QWidget(parent)
{
	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	const QVector<QPair<Day, QString>> days = {
		{Monday, obs_module_text("AdvSceneSwitcher.day.monday")},
		{Tuesday, obs_module_text("AdvSceneSwitcher.day.tuesday")},
		{Wednesday, obs_module_text("AdvSceneSwitcher.day.wednesday")},
		{Thursday, obs_module_text("AdvSceneSwitcher.day.thursday")},
		{Friday, obs_module_text("AdvSceneSwitcher.day.friday")},
		{Saturday, obs_module_text("AdvSceneSwitcher.day.saturday")},
		{Sunday, obs_module_text("AdvSceneSwitcher.day.sunday")}};

	for (const auto &[day, name] : days) {
		auto btn = new QPushButton(this);
		btn->setText(name);
		btn->setCheckable(true);

		connect(btn, &QPushButton::toggled, this,
			&DayOfWeekSelector::OnButtonToggled);

		layout->addWidget(btn);
		m_buttons.insert(day, btn);
	}

	setLayout(layout);
}

void DayOfWeekSelector::OnButtonToggled(bool)
{
	emit SelectionChanged(SelectedDays());
}

QSet<DayOfWeekSelector::Day> DayOfWeekSelector::SelectedDays() const
{
	QSet<Day> result;
	for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
		if (it.value()->isChecked()) {
			result.insert(it.key());
		}
	}
	return result;
}

void DayOfWeekSelector::SetSelectedDays(const QSet<Day> &days)
{
	for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
		it.value()->setChecked(days.contains(it.key()));
	}
}

QString DayOfWeekSelector::ToString(const QSet<Day> &days)
{
	const QVector<QPair<Day, QString>> dayNames = {
		{Monday, obs_module_text("AdvSceneSwitcher.day.monday")},
		{Tuesday, obs_module_text("AdvSceneSwitcher.day.tuesday")},
		{Wednesday, obs_module_text("AdvSceneSwitcher.day.wednesday")},
		{Thursday, obs_module_text("AdvSceneSwitcher.day.thursday")},
		{Friday, obs_module_text("AdvSceneSwitcher.day.friday")},
		{Saturday, obs_module_text("AdvSceneSwitcher.day.saturday")},
		{Sunday, obs_module_text("AdvSceneSwitcher.day.sunday")}};

	if (days.isEmpty()) {
		return obs_module_text("AdvSceneSwitcher.day.none");
	}

	if (days.size() == 7) {
		return obs_module_text("AdvSceneSwitcher.day.any");
	}

	QStringList parts;
	for (const auto &[day, name] : dayNames) {
		if (days.contains(day)) {
			parts << name;
		}
	}

	return parts.join(", ");
}

void SaveSelectedDays(obs_data_t *settings,
		      const QSet<DayOfWeekSelector::Day> &days)
{
	uint64_t mask = 0;

	for (DayOfWeekSelector::Day day : days) {
		mask |= (1ULL << static_cast<int>(day));
	}

	obs_data_set_int(settings, "selectedDays",
			 static_cast<long long>(mask));
}

QSet<DayOfWeekSelector::Day> LoadSelectedDays(obs_data_t *settings)
{
	QSet<DayOfWeekSelector::Day> result;

	uint64_t mask = static_cast<uint64_t>(
		obs_data_get_int(settings, "selectedDays"));

	for (int i = DayOfWeekSelector::Monday; i <= DayOfWeekSelector::Sunday;
	     ++i) {
		if (mask & (1ULL << i)) {
			result.insert(static_cast<DayOfWeekSelector::Day>(i));
		}
	}

	return result;
}

} // namespace advss
