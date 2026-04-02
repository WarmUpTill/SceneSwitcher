#include "source-interaction-step-edit.hpp"
#include "obs-module-helper.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"

#include <map>
#include <string>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace advss {

static const std::map<SourceInteractionStep::Type, std::string> stepTypeNames = {
	{SourceInteractionStep::Type::MOUSE_MOVE,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.mouseMove"},
	{SourceInteractionStep::Type::MOUSE_CLICK,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.mouseClick"},
	{SourceInteractionStep::Type::MOUSE_WHEEL,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.mouseWheel"},
	{SourceInteractionStep::Type::KEY_PRESS,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.keyPress"},
	{SourceInteractionStep::Type::TYPE_TEXT,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.typeText"},
	{SourceInteractionStep::Type::WAIT,
	 "AdvSceneSwitcher.action.sourceInteraction.step.edit.wait"},
};

static const std::map<obs_mouse_button_type, std::string> mouseButtonNames = {
	{MOUSE_LEFT, "AdvSceneSwitcher.action.sourceInteraction.button.left"},
	{MOUSE_MIDDLE,
	 "AdvSceneSwitcher.action.sourceInteraction.button.middle"},
	{MOUSE_RIGHT, "AdvSceneSwitcher.action.sourceInteraction.button.right"},
};

static void populateTypeCombo(QComboBox *combo)
{
	for (const auto &[type, name] : stepTypeNames) {
		combo->addItem(obs_module_text(name.c_str()),
			       static_cast<int>(type));
	}
}

static void populateButtonCombo(QComboBox *combo)
{
	for (const auto &[btn, name] : mouseButtonNames) {
		combo->addItem(obs_module_text(name.c_str()),
			       static_cast<int>(btn));
	}
}

static QHBoxLayout *labelledRow(const char *labelKey, QWidget *w)
{
	auto row = new QHBoxLayout;
	row->addWidget(new QLabel(obs_module_text(labelKey)));
	row->addWidget(w);
	row->addStretch();
	return row;
}

SourceInteractionStepEdit::SourceInteractionStepEdit(
	QWidget *parent, const SourceInteractionStep &step)
	: QWidget(parent),
	  _typeCombo(new QComboBox(this)),
	  _fields(new QStackedWidget(this)),
	  _step(step)
{
	populateTypeCombo(_typeCombo);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	auto typeRow = labelledRow(
		"AdvSceneSwitcher.action.sourceInteraction.step.edit.type",
		_typeCombo);
	mainLayout->addLayout(typeRow);
	mainLayout->addWidget(_fields);
	setLayout(mainLayout);

	connect(_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &SourceInteractionStepEdit::TypeChanged);

	RebuildFields();

	int idx = _typeCombo->findData(static_cast<int>(_step.type));
	if (idx >= 0) {
		_typeCombo->setCurrentIndex(idx);
	}
}

void SourceInteractionStepEdit::TypeChanged(int)
{
	_step.type = static_cast<SourceInteractionStep::Type>(
		_typeCombo->currentData().toInt());
	RebuildFields();
	emit StepChanged(_step);
}

void SourceInteractionStepEdit::UpdateStep()
{
	emit StepChanged(_step);
}

void SourceInteractionStepEdit::RebuildFields()
{
	while (_fields->count() > 0) {
		auto w = _fields->widget(0);
		_fields->removeWidget(w);
		delete w;
	}

	auto makeVarSpin = [](int min, int max,
			      const NumberVariable<int> &val) {
		auto sb = new VariableSpinBox;
		sb->setMinimum(min);
		sb->setMaximum(max);
		sb->SetValue(val);
		return sb;
	};

	switch (_step.type) {
	case SourceInteractionStep::Type::MOUSE_MOVE:
	case SourceInteractionStep::Type::MOUSE_CLICK:
	case SourceInteractionStep::Type::MOUSE_WHEEL: {
		auto page = new QWidget;
		auto layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, 0, 0, 0);

		auto xSpin = makeVarSpin(-32768, 32767, _step.x);
		auto ySpin = makeVarSpin(-32768, 32767, _step.y);
		layout->addLayout(labelledRow(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.x",
			xSpin));
		layout->addLayout(labelledRow(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.y",
			ySpin));

		connect(xSpin,
			QOverload<const NumberVariable<int> &>::of(
				&GenericVariableSpinbox::NumberVariableChanged),
			this, [this, xSpin](const NumberVariable<int> &) {
				_step.x = xSpin->Value();
				UpdateStep();
			});
		connect(ySpin,
			QOverload<const NumberVariable<int> &>::of(
				&GenericVariableSpinbox::NumberVariableChanged),
			this, [this, ySpin](const NumberVariable<int> &) {
				_step.y = ySpin->Value();
				UpdateStep();
			});

		if (_step.type == SourceInteractionStep::Type::MOUSE_CLICK) {
			auto btnCombo = new QComboBox;
			populateButtonCombo(btnCombo);
			btnCombo->setCurrentIndex(btnCombo->findData(
				static_cast<int>(_step.button)));

			auto upCheck = new QCheckBox(obs_module_text(
				"AdvSceneSwitcher.action.sourceInteraction.step.edit.mouseUp"));
			upCheck->setChecked(_step.mouseUp);

			auto countSpin = makeVarSpin(1, 3, _step.clickCount);

			layout->addLayout(labelledRow(
				"AdvSceneSwitcher.action.sourceInteraction.step.edit.button",
				btnCombo));
			layout->addWidget(upCheck);
			layout->addLayout(labelledRow(
				"AdvSceneSwitcher.action.sourceInteraction.step.edit.clickCount",
				countSpin));

			connect(btnCombo,
				QOverload<int>::of(
					&QComboBox::currentIndexChanged),
				this, [this, btnCombo]() {
					_step.button = static_cast<
						obs_mouse_button_type>(
						btnCombo->currentData().toInt());
					UpdateStep();
				});
			connect(upCheck, &QCheckBox::stateChanged, this,
				[this, upCheck]() {
					_step.mouseUp = upCheck->isChecked();
					UpdateStep();
				});
			connect(countSpin,
				QOverload<const NumberVariable<int> &>::of(
					&GenericVariableSpinbox::
						NumberVariableChanged),
				this,
				[this, countSpin](const NumberVariable<int> &) {
					_step.clickCount = countSpin->Value();
					UpdateStep();
				});
		} else if (_step.type ==
			   SourceInteractionStep::Type::MOUSE_WHEEL) {
			auto dxSpin =
				makeVarSpin(-1200, 1200, _step.wheelDeltaX);
			auto dySpin =
				makeVarSpin(-1200, 1200, _step.wheelDeltaY);
			layout->addLayout(labelledRow(
				"AdvSceneSwitcher.action.sourceInteraction.step.edit.wheelDx",
				dxSpin));
			layout->addLayout(labelledRow(
				"AdvSceneSwitcher.action.sourceInteraction.step.edit.wheelDy",
				dySpin));
			connect(dxSpin,
				QOverload<const NumberVariable<int> &>::of(
					&GenericVariableSpinbox::
						NumberVariableChanged),
				this,
				[this, dxSpin](const NumberVariable<int> &) {
					_step.wheelDeltaX = dxSpin->Value();
					UpdateStep();
				});
			connect(dySpin,
				QOverload<const NumberVariable<int> &>::of(
					&GenericVariableSpinbox::
						NumberVariableChanged),
				this,
				[this, dySpin](const NumberVariable<int> &) {
					_step.wheelDeltaY = dySpin->Value();
					UpdateStep();
				});
		}

		_fields->addWidget(page);
		_fields->setCurrentWidget(page);
		break;
	}

	case SourceInteractionStep::Type::KEY_PRESS: {
		auto page = new QWidget;
		auto layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, 0, 0, 0);

		auto vkeySpin = makeVarSpin(0, 0xFFFF, _step.nativeVkey);
		auto upCheck = new QCheckBox(obs_module_text(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.keyUp"));
		upCheck->setChecked(_step.keyUp);

		layout->addLayout(labelledRow(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.vkey",
			vkeySpin));
		layout->addWidget(upCheck);

		connect(vkeySpin,
			QOverload<const NumberVariable<int> &>::of(
				&GenericVariableSpinbox::NumberVariableChanged),
			this, [this, vkeySpin](const NumberVariable<int> &) {
				_step.nativeVkey = vkeySpin->Value();
				UpdateStep();
			});
		connect(upCheck, &QCheckBox::stateChanged, this,
			[this, upCheck]() {
				_step.keyUp = upCheck->isChecked();
				UpdateStep();
			});

		_fields->addWidget(page);
		_fields->setCurrentWidget(page);
		break;
	}

	case SourceInteractionStep::Type::TYPE_TEXT: {
		auto page = new QWidget;
		auto layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, 0, 0, 0);

		auto textEdit = new VariableLineEdit(page);
		textEdit->setText(_step.text);
		layout->addLayout(labelledRow(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.text",
			textEdit));

		connect(textEdit, &QLineEdit::editingFinished, this,
			[this, textEdit]() {
				_step.text = textEdit->text().toStdString();
				UpdateStep();
			});

		_fields->addWidget(page);
		_fields->setCurrentWidget(page);
		break;
	}

	case SourceInteractionStep::Type::WAIT: {
		auto page = new QWidget;
		auto layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, 0, 0, 0);

		auto msSpin = makeVarSpin(0, 60000, _step.waitMs);
		layout->addLayout(labelledRow(
			"AdvSceneSwitcher.action.sourceInteraction.step.edit.waitMs",
			msSpin));

		connect(msSpin,
			QOverload<const NumberVariable<int> &>::of(
				&GenericVariableSpinbox::NumberVariableChanged),
			this, [this, msSpin](const NumberVariable<int> &) {
				_step.waitMs = msSpin->Value();
				UpdateStep();
			});

		_fields->addWidget(page);
		_fields->setCurrentWidget(page);
		break;
	}
	}
}

} // namespace advss
