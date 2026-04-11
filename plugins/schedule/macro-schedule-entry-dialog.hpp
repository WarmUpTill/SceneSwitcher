#pragma once

#include "macro-schedule.hpp"
#include "macro-selection.hpp"

#include <QCheckBox>
#include <QColor>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>

namespace advss {

// Dialog for creating or editing a MacroScheduleEntry.
class MacroScheduleEntryDialog : public QDialog {
	Q_OBJECT

public:
	static bool AskForSettings(QWidget *parent, MacroScheduleEntry &entry,
				   bool isNew);

private:
	explicit MacroScheduleEntryDialog(QWidget *parent,
					  const MacroScheduleEntry &entry,
					  bool isNew);

	void PopulateFromEntry(const MacroScheduleEntry &entry);
	void ApplyToEntry(MacroScheduleEntry &entry) const;
	void UpdateRepeatVisibility();

	// Basic fields
	QLineEdit *_name;
	MacroSelection *_macroSel;
	QDateTimeEdit *_startDateTime;

	// Optional end date
	QCheckBox *_hasEndDate;
	QDateTimeEdit *_endDate;
	QComboBox *_endDateAction;

	// Repeat type
	QComboBox *_repeatType;

	// Repeat interval (hidden when type == NONE)
	QWidget *_intervalRow;
	QSpinBox *_repeatInterval;
	QLabel *_intervalUnitLabel;

	// Repeat end (hidden when type == NONE)
	QGroupBox *_repeatEndGroup;
	QRadioButton *_endNever;
	QRadioButton *_endAfterN;
	QSpinBox *_endCount;
	QRadioButton *_endUntil;
	QDateEdit *_endUntilDate;

	// Color
	QPushButton *_colorBtn;
	QColor _color;
	void UpdateColorButton();

	// Options
	QCheckBox *_checkConditions;
	QCheckBox *_runElseActionsOnConditionFailure;
	QCheckBox *_enabled;
};

} // namespace advss
