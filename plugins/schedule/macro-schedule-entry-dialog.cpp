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
	  _doesRepeat(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.macroScheduleEntry.repeat"),
		  this)),
	  _repeatInterval(new DurationSelection(this, true, 0.1)),
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

	// --- Repeat ---
	form->addRow(QString(), _doesRepeat);

	_repeatInterval->setVisible(false);
	form->addRow(
		obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.interval"),
		_repeatInterval);
	if (auto *label = form->labelForField(_repeatInterval)) {
		label->setVisible(false);
	}

	connect(_doesRepeat, &QCheckBox::toggled, this,
		[form, this](bool checked) {
			_repeatInterval->setVisible(checked);
			if (auto *label =
				    form->labelForField(_repeatInterval)) {
				label->setVisible(checked);
			}
			_repeatEndGroup->setVisible(checked);
		});

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

	_repeatEndGroup->setVisible(false);
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

	PopulateFromEntry(entry);
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

	_doesRepeat->setChecked(entry.doesRepeat);
	_repeatInterval->SetDuration(entry.repeatInterval);
	_doesRepeat->toggled(entry.doesRepeat);

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

	entry.doesRepeat = _doesRepeat->isChecked();
	entry.repeatInterval = _repeatInterval->GetDuration();

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

void MacroScheduleEntryDialog::UpdateColorButton()
{
	// Fill the button with the chosen color so it acts as a swatch
	const QString style =
		QString("background-color: %1; border: 1px solid #888;")
			.arg(_color.name());
	_colorBtn->setStyleSheet(style);
}

} // namespace advss
