#include "variable-spinbox.hpp"
#include "ui-helpers.hpp"

#include <QLayout>

namespace advss {

GenericVariableSpinbox::GenericVariableSpinbox(QWidget *parent,
					       bool wholeNumber)
	: QWidget(parent),
	  _fixedValueInt(new QSpinBox()),
	  _fixedValueDouble(new QDoubleSpinBox()),
	  _toggleType(new QPushButton()),
	  _variable(new VariableSelection(this)),
	  _wholeNumber(wholeNumber)
{
	_toggleType->setCheckable(true);
	_toggleType->setMaximumWidth(11);
	SetButtonIcon(_toggleType, GetThemeTypeName() == "Light"
					   ? ":/res/images/dots-vert.svg"
					   : "theme:Dark/dots-vert.svg");

	QWidget::connect(_fixedValueInt, SIGNAL(valueChanged(int)), this,
			 SLOT(SetFixedValue(int)));
	QWidget::connect(_fixedValueDouble, SIGNAL(valueChanged(double)), this,
			 SLOT(SetFixedValue(double)));
	QWidget::connect(_toggleType, SIGNAL(toggled(bool)), this,
			 SLOT(ToggleTypeClicked(bool)));
	QWidget::connect(_variable, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));

	QWidget::connect(
		VariableSignalManager::Instance(),
		SIGNAL(Rename(const QString &, const QString &)), this,
		SIGNAL(VariableRenamed(const QString &, const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SIGNAL(VariableAdded(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SIGNAL(VariableRemoved(const QString &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_fixedValueInt);
	layout->addWidget(_fixedValueDouble);
	layout->addWidget(_variable);
	layout->addWidget(_toggleType);
	setLayout(layout);
	SetVisibility();
}

void GenericVariableSpinbox::SetValue(const NumberVariable<int> &number)
{
	_numberInt = number;
	const QSignalBlocker b1(_toggleType);
	const QSignalBlocker b2(_variable);
	_toggleType->setChecked(!number.IsFixedType());
	_fixedValueInt->setValue(number._value);
	_variable->SetVariable(number._variable);
	SetVisibility();
}

void GenericVariableSpinbox::SetValue(const NumberVariable<double> &number)
{
	_numberDouble = number;
	const QSignalBlocker b1(_toggleType);
	const QSignalBlocker b2(_variable);
	_toggleType->setChecked(!number.IsFixedType());
	_fixedValueDouble->setValue(number._value);
	_variable->SetVariable(number._variable);
	SetVisibility();
}

void GenericVariableSpinbox::DisableVariableSelection()
{
	_hideTypeToggle = true;
	_toggleType->hide();
	ToggleTypeClicked(false);
}

void GenericVariableSpinbox::setMinimum(double value)
{
	_fixedValueInt->setMinimum(value);
	_fixedValueDouble->setMinimum(value);
}

void GenericVariableSpinbox::setMaximum(double value)
{
	_fixedValueInt->setMaximum(value);
	_fixedValueDouble->setMaximum(value);
}

void GenericVariableSpinbox::setPrefix(const QString &prefix)
{
	_fixedValueInt->setPrefix(prefix);
	_fixedValueDouble->setPrefix(prefix);
}

void GenericVariableSpinbox::setSuffix(const QString &suffix)
{
	_fixedValueInt->setSuffix(suffix);
	_fixedValueDouble->setSuffix(suffix);
}

void GenericVariableSpinbox::setSpecialValueText(const QString &text)
{
	_fixedValueInt->setSpecialValueText(text);
	_fixedValueDouble->setSpecialValueText(text);
}

void GenericVariableSpinbox::SetFixedValue(int value)
{
	_numberInt._value = value;
	const QSignalBlocker b(_fixedValueInt);
	_fixedValueInt->setValue(value);
	EmitSignals();
}

void GenericVariableSpinbox::SetFixedValue(double value)
{
	_numberDouble._value = value;
	const QSignalBlocker b(_fixedValueDouble);
	_fixedValueDouble->setValue(value);
	EmitSignals();
}

void GenericVariableSpinbox::VariableChanged(const QString &name)
{
	_numberInt._variable = GetWeakVariableByQString(name);
	_numberDouble._variable = GetWeakVariableByQString(name);
	EmitSignals();
}

void GenericVariableSpinbox::ToggleTypeClicked(bool useVariable)
{
	_numberInt._type = useVariable ? NumberVariable<int>::Type::VARIABLE
				       : NumberVariable<int>::Type::FIXED_VALUE;
	_numberDouble._type =
		useVariable ? NumberVariable<double>::Type::VARIABLE
			    : NumberVariable<double>::Type::FIXED_VALUE;

	SetVisibility();
	EmitSignals();
}

void GenericVariableSpinbox::EmitSignals()
{
	if (_wholeNumber) {
		emit FixedValueChanged(_numberInt);
		emit NumberVariableChanged(_numberInt);
	} else {
		emit FixedValueChanged(_numberDouble);
		emit NumberVariableChanged(_numberDouble);
	}
}

void GenericVariableSpinbox::SetVisibility()
{
	if (_wholeNumber) {
		_fixedValueDouble->hide();
		SetVisibilityInt();
	} else {
		_fixedValueInt->hide();
		SetVisibilityDouble();
	}
}

void GenericVariableSpinbox::SetVisibilityInt()
{
	if (_numberInt.IsFixedType()) {
		_fixedValueInt->show();
		_variable->hide();
		_toggleType->setVisible(!GetVariables().empty() &&
					!_hideTypeToggle);
	} else {
		_fixedValueInt->hide();
		_variable->show();
		_toggleType->show();
	}
	adjustSize();
	updateGeometry();
}

void GenericVariableSpinbox::SetVisibilityDouble()
{
	if (_numberDouble.IsFixedType()) {
		_fixedValueDouble->show();
		_variable->hide();
		_toggleType->setVisible(!GetVariables().empty() &&
					!_hideTypeToggle);
	} else {
		_fixedValueDouble->hide();
		_variable->show();
		_toggleType->show();
	}
	adjustSize();
	updateGeometry();
}

VariableSpinBox::VariableSpinBox(QWidget *parent)
	: GenericVariableSpinbox(parent, true)
{
}

VariableDoubleSpinBox::VariableDoubleSpinBox(QWidget *parent)
	: GenericVariableSpinbox(parent, false)
{
}

void VariableDoubleSpinBox::setDecimals(int prec)
{
	_fixedValueDouble->setDecimals(prec);
}

} // namespace advss
