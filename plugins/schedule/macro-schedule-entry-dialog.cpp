#include "macro-schedule-entry-dialog.hpp"

#include "help-icon.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace advss {

bool MacroScheduleEntryDialog::AskForSettings(QWidget *parent,
					      MacroScheduleEntry &entry,
					      bool isNew)
{
	MacroScheduleEntryDialog dialog(parent, entry, isNew);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	dialog.ApplyToEntry(entry);
	return true;
}

MacroScheduleEntryDialog::MacroScheduleEntryDialog(
	QWidget *parent, const MacroScheduleEntry &entry, bool isNew)
	: QDialog(parent),
	  _name(new QLineEdit(this)),
	  _macroSel(new MacroSelection(this)),
	  _startDateTime(new QDateTimeEdit(this)),
	  _hasEndDate(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.macroScheduleEntry.endDate"),
		  this)),
	  _endDate(new QDateTimeEdit(this)),
	  _endDateAction(new QComboBox(this)),
	  _repeatType(new QComboBox(this)),
	  _intervalRow(new QWidget(this)),
	  _repeatInterval(new QSpinBox(_intervalRow)),
	  _intervalUnitLabel(new QLabel("", _intervalRow)),
	  _repeatEndGroup(new QGroupBox(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.repeatEnd"),
		  this)),
	  _endNever(new QRadioButton(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.repeatEnd.never"),
		  _repeatEndGroup)),
	  _endAfterN(new QRadioButton(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.repeatEnd.afterNTimes"),
		  _repeatEndGroup)),
	  _endCount(new QSpinBox(_repeatEndGroup)),
	  _endUntil(new QRadioButton(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.repeatEnd.untilDate"),
		  _repeatEndGroup)),
	  _endUntilDate(new QDateEdit(_repeatEndGroup)),
	  _colorBtn(new QPushButton(this)),
	  _checkConditions(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.checkConditions"),
		  this)),
	  _runElseActionsOnConditionFailure(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.macroScheduleEntry.runElseActionsOnConditionFailure"),
		  this)),
	  _enabled(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.macroScheduleEntry.enabled"),
		  this))
{
	setWindowTitle(obs_module_text(
		isNew ? "AdvSceneSwitcher.macroScheduleEntry.dialog.title.add"
		      : "AdvSceneSwitcher.macroScheduleEntry.dialog.title.edit"));
	setMinimumWidth(420);

	auto form = new QFormLayout();
	form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	// --- Name ---
	_name->setPlaceholderText(obs_module_text(
		"AdvSceneSwitcher.macroScheduleEntry.name.placeholder"));
	form->addRow(
		obs_module_text("AdvSceneSwitcher.macroScheduleEntry.name"),
		_name);

	// --- Macro ---
	form->addRow(
		obs_module_text("AdvSceneSwitcher.macroScheduleEntry.macro"),
		_macroSel);

	// --- Start date/time ---
	_startDateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
	_startDateTime->setCalendarPopup(true);
	_startDateTime->setDateTime(QDateTime::currentDateTime());
	form->addRow(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.startDateTime"),
		_startDateTime);

	// --- Optional end date ---

	_endDate->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
	_endDate->setCalendarPopup(true);
	_endDate->setDateTime(QDateTime::currentDateTime().addSecs(60 * 60));
	_endDate->setEnabled(false);
	auto endDateHelp = new HelpIcon(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.endDate.help"),
		this);
	_endDateAction->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.endDate.action.none"),
		static_cast<int>(MacroScheduleEntry::EndDateAction::NONE));
	_endDateAction->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.endDate.action.disableEntry"),
		static_cast<int>(
			MacroScheduleEntry::EndDateAction::DISABLE_ENTRY));
	_endDateAction->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.endDate.action.pauseMacro"),
		static_cast<int>(
			MacroScheduleEntry::EndDateAction::PAUSE_MACRO));
	_endDateAction->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.endDate.action.stopMacro"),
		static_cast<int>(
			MacroScheduleEntry::EndDateAction::STOP_MACRO));
	_endDateAction->setEnabled(false);

	auto endDateRow = new QHBoxLayout();
	endDateRow->addWidget(_hasEndDate);
	endDateRow->addWidget(_endDate);
	endDateRow->addWidget(_endDateAction);
	endDateRow->addWidget(endDateHelp);
	endDateRow->addStretch();
	form->addRow(QString(), endDateRow);

	connect(_hasEndDate, &QCheckBox::toggled, _endDate,
		&QDateTimeEdit::setEnabled);
	connect(_hasEndDate, &QCheckBox::toggled, _endDateAction,
		&QComboBox::setEnabled);

	// Keep end date minimum in sync with the start date.
	_endDate->setMinimumDateTime(_startDateTime->dateTime());
	connect(_startDateTime, &QDateTimeEdit::dateTimeChanged, this,
		[this](const QDateTime &dt) {
			_endDate->setMinimumDateTime(dt);
			if (_endDate->dateTime() < dt) {
				_endDate->setDateTime(dt);
			}
		});

	// --- Repeat type ---
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.none"),
		static_cast<int>(MacroScheduleEntry::RepeatType::NONE));
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.minutely"),
		static_cast<int>(MacroScheduleEntry::RepeatType::MINUTELY));
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.hourly"),
		static_cast<int>(MacroScheduleEntry::RepeatType::HOURLY));
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.daily"),
		static_cast<int>(MacroScheduleEntry::RepeatType::DAILY));
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.weekly"),
		static_cast<int>(MacroScheduleEntry::RepeatType::WEEKLY));
	_repeatType->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.monthly"),
		static_cast<int>(MacroScheduleEntry::RepeatType::MONTHLY));
	form->addRow(
		obs_module_text("AdvSceneSwitcher.macroScheduleEntry.repeat"),
		_repeatType);

	// --- Repeat interval row (hidden when NONE) ---
	auto intervalLayout = new QHBoxLayout(_intervalRow);
	intervalLayout->setContentsMargins(0, 0, 0, 0);
	auto everyLabel = new QLabel(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.prefix"),
		_intervalRow);
	_repeatInterval->setMinimum(1);
	_repeatInterval->setMaximum(999);
	_repeatInterval->setValue(1);
	intervalLayout->addWidget(everyLabel);
	intervalLayout->addWidget(_repeatInterval);
	intervalLayout->addWidget(_intervalUnitLabel);
	intervalLayout->addStretch();
	form->addRow(QString(), _intervalRow);

	// --- Repeat end group (hidden when NONE) ---

	auto repeatEndLayout = new QVBoxLayout(_repeatEndGroup);

	_endNever->setChecked(true);
	repeatEndLayout->addWidget(_endNever);

	auto afterNRow = new QHBoxLayout();
	_endCount->setMinimum(1);
	_endCount->setMaximum(99999);
	_endCount->setValue(1);
	_endCount->setEnabled(false);
	auto afterNSuffix = new QLabel(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeatEnd.afterNTimes.suffix"),
		_repeatEndGroup);
	afterNRow->addWidget(_endAfterN);
	afterNRow->addWidget(_endCount);
	afterNRow->addWidget(afterNSuffix);
	afterNRow->addStretch();
	repeatEndLayout->addLayout(afterNRow);

	auto untilRow = new QHBoxLayout();
	_endUntilDate->setCalendarPopup(true);
	_endUntilDate->setDate(QDate::currentDate().addMonths(1));
	_endUntilDate->setEnabled(false);
	untilRow->addWidget(_endUntil);
	untilRow->addWidget(_endUntilDate);
	untilRow->addStretch();
	repeatEndLayout->addLayout(untilRow);

	form->addRow(_repeatEndGroup);

	// Enable/disable child widgets based on radio selection
	connect(_endAfterN, &QRadioButton::toggled, _endCount,
		&QSpinBox::setEnabled);
	connect(_endUntil, &QRadioButton::toggled, _endUntilDate,
		&QDateEdit::setEnabled);

	// --- Color ---
	_colorBtn->setFixedSize(48, 22);
	_colorBtn->setFlat(false);
	connect(_colorBtn, &QPushButton::clicked, this, [this]() {
		const QColor picked = QColorDialog::getColor(
			_color, this,
			obs_module_text(
				"AdvSceneSwitcher.macroScheduleEntry.color.dialog"));
		if (picked.isValid()) {
			_color = picked;
			UpdateColorButton();
		}
	});
	form->addRow(
		obs_module_text("AdvSceneSwitcher.macroScheduleEntry.color"),
		_colorBtn);

	// --- Options ---
	form->addRow(QString(), _checkConditions);

	_runElseActionsOnConditionFailure->setVisible(false);
	form->addRow(QString(), _runElseActionsOnConditionFailure);
	connect(_checkConditions, &QCheckBox::toggled,
		_runElseActionsOnConditionFailure, &QCheckBox::setVisible);

	_enabled->setChecked(true);
	form->addRow(QString(), _enabled);

	// --- Buttons ---
	auto buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// --- Main layout ---
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(form);
	mainLayout->addWidget(buttons);
	setLayout(mainLayout);

	// Connect repeat type change
	connect(_repeatType,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int) { UpdateRepeatVisibility(); });

	PopulateFromEntry(entry);
	UpdateRepeatVisibility();
}

void MacroScheduleEntryDialog::PopulateFromEntry(const MacroScheduleEntry &entry)
{
	_name->setText(QString::fromStdString(entry.name));

	MacroRef ref = entry.macro;
	_macroSel->SetCurrentMacro(ref);

	if (entry.startDateTime.isValid()) {
		_startDateTime->setDateTime(entry.startDateTime);
	}

	_hasEndDate->setChecked(entry.hasEndDate);
	if (entry.hasEndDate && entry.endDate.isValid()) {
		_endDate->setDateTime(entry.endDate);
	}
	{
		const int idx = _endDateAction->findData(
			static_cast<int>(entry.endDateAction));
		if (idx >= 0) {
			_endDateAction->setCurrentIndex(idx);
		}
	}

	const int repeatIdx =
		_repeatType->findData(static_cast<int>(entry.repeatType));
	if (repeatIdx >= 0) {
		_repeatType->setCurrentIndex(repeatIdx);
	}
	_repeatInterval->setValue(entry.repeatInterval);

	switch (entry.repeatEndType) {
	case MacroScheduleEntry::RepeatEndType::NEVER:
		_endNever->setChecked(true);
		break;
	case MacroScheduleEntry::RepeatEndType::AFTER_N_TIMES:
		_endAfterN->setChecked(true);
		_endCount->setValue(entry.repeatMaxCount);
		break;
	case MacroScheduleEntry::RepeatEndType::UNTIL_DATE:
		_endUntil->setChecked(true);
		if (entry.repeatUntilDate.isValid()) {
			_endUntilDate->setDate(entry.repeatUntilDate.date());
		}
		break;
	}

	_color = entry.color.isValid() ? entry.color : QColor(70, 130, 180);
	UpdateColorButton();
	_checkConditions->setChecked(entry.checkConditions);
	_runElseActionsOnConditionFailure->setChecked(
		entry.runElseActionsOnConditionFailure);
	_runElseActionsOnConditionFailure->setVisible(entry.checkConditions);
	_enabled->setChecked(entry.enabled);
}

void MacroScheduleEntryDialog::ApplyToEntry(MacroScheduleEntry &entry) const
{
	entry.name = _name->text().toStdString();

	const QString macroName = _macroSel->currentText();
	entry.macro = macroName;

	const QDateTime newStartDateTime = _startDateTime->dateTime();
	const bool startChanged = (newStartDateTime != entry.startDateTime);
	entry.startDateTime = newStartDateTime;
	if (startChanged) {
		entry.FastForwardTo(QDateTime::currentDateTime());
	}

	entry.hasEndDate = _hasEndDate->isChecked();
	if (entry.hasEndDate) {
		entry.endDate = _endDate->dateTime();
		entry.endDateAction =
			static_cast<MacroScheduleEntry::EndDateAction>(
				_endDateAction->currentData().toInt());
	} else {
		entry.endDate = QDateTime();
		entry.endDateAction = MacroScheduleEntry::EndDateAction::NONE;
	}
	entry.endDateActionApplied = false;

	const int repeatData = _repeatType->currentData().toInt();
	entry.repeatType =
		static_cast<MacroScheduleEntry::RepeatType>(repeatData);
	entry.repeatInterval = _repeatInterval->value();

	if (_endNever->isChecked()) {
		entry.repeatEndType = MacroScheduleEntry::RepeatEndType::NEVER;
		entry.repeatUntilDate = QDateTime();
	} else if (_endAfterN->isChecked()) {
		entry.repeatEndType =
			MacroScheduleEntry::RepeatEndType::AFTER_N_TIMES;
		entry.repeatMaxCount = _endCount->value();
		entry.repeatUntilDate = QDateTime();
	} else if (_endUntil->isChecked()) {
		entry.repeatEndType =
			MacroScheduleEntry::RepeatEndType::UNTIL_DATE;
		entry.repeatUntilDate =
			QDateTime(_endUntilDate->date(), QTime(23, 59, 59));
	}

	entry.color = _color;
	entry.checkConditions = _checkConditions->isChecked();
	entry.runElseActionsOnConditionFailure =
		_runElseActionsOnConditionFailure->isChecked();
	entry.enabled = _enabled->isChecked();
}

static QString repeatTypeUnitLabel(MacroScheduleEntry::RepeatType type)
{
	switch (type) {
	case MacroScheduleEntry::RepeatType::MINUTELY:
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.unit.minute");
	case MacroScheduleEntry::RepeatType::HOURLY:
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.unit.hour");
	case MacroScheduleEntry::RepeatType::DAILY:
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.unit.day");
	case MacroScheduleEntry::RepeatType::WEEKLY:
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.unit.week");
	case MacroScheduleEntry::RepeatType::MONTHLY:
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval.unit.month");
	default:
		return "";
	}
}

void MacroScheduleEntryDialog::UpdateRepeatVisibility()
{
	const int data = _repeatType->currentData().toInt();
	const auto type = static_cast<MacroScheduleEntry::RepeatType>(data);
	const bool repeating = (type != MacroScheduleEntry::RepeatType::NONE);

	_intervalRow->setVisible(repeating);
	_repeatEndGroup->setVisible(repeating);

	if (repeating) {
		_intervalUnitLabel->setText(repeatTypeUnitLabel(type));
	}
}

void MacroScheduleEntryDialog::UpdateColorButton()
{
	// Fill the button with the chosen color so it acts as a swatch
	const QString style =
		QString("background-color: %1; border: 1px solid #888;")
			.arg(_color.name());
	_colorBtn->setStyleSheet(style);
}

} // namespace advss
