#pragma once
#include <QWidget>
#include <QSet>

#include <obs-data.h>

class QPushButton;

namespace advss {

class DayOfWeekSelector : public QWidget {
	Q_OBJECT

public:
	enum Day {
		Monday = 1,
		Tuesday,
		Wednesday,
		Thursday,
		Friday,
		Saturday,
		Sunday
	};
	Q_ENUM(Day)

	explicit DayOfWeekSelector(QWidget *parent = nullptr);

	QSet<Day> SelectedDays() const;
	void SetSelectedDays(const QSet<Day> &days);
	static QString ToString(const QSet<Day> &days);

signals:
	void SelectionChanged(const QSet<Day> &days);

private:
	void OnButtonToggled(bool checked);

	QMap<Day, QPushButton *> m_buttons;
};

void SaveSelectedDays(obs_data_t *settings,
		      const QSet<DayOfWeekSelector::Day> &days);
QSet<DayOfWeekSelector::Day> LoadSelectedDays(obs_data_t *settings);

} // namespace advss
