#include "variable-spinbox.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <QLayout>

GenericVaraiableSpinbox::GenericVaraiableSpinbox(QWidget *parent,
						 bool wholeNumber)
	: QWidget(parent),
	  _toggleType(new QPushButton()),
	  _fixedValueInt(new QSpinBox()),
	  _fixedValueDouble(new QDoubleSpinBox()),
	  _variable(new VariableSelection(this)),
	  _wholeNumber(wholeNumber)
{
	_toggleType->setCheckable(true);
	_toggleType->setMaximumWidth(11);
	setButtonIcon(_toggleType, ":/res/images/dots-vert.svg");

	QWidget::connect(_fixedValueInt, SIGNAL(valueChanged(int)), this,
			 SLOT(SetFixedValue(int)));
	QWidget::connect(_fixedValueDouble, SIGNAL(valueChanged(double)), this,
			 SLOT(SetFixedValue(double)));
	QWidget::connect(_toggleType, SIGNAL(toggled(bool)), this,
			 SLOT(ToggleTypeClicked(bool)));
	QWidget::connect(_variable, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));

	// Forward signals for variable selection
	if (window() != this) {
		QWidget::connect(window(),
				 SIGNAL(VariableRenamed(const QString &,
							const QString &)),
				 this,
				 SIGNAL(VariableRenamed(const QString &,
							const QString &)));
		QWidget::connect(window(),
				 SIGNAL(VariableAdded(const QString &)), this,
				 SIGNAL(VariableAdded(const QString &)));
		QWidget::connect(window(),
				 SIGNAL(VariableRemoved(const QString &)), this,
				 SIGNAL(VariableRemoved(const QString &)));
	}

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_fixedValueInt);
	layout->addWidget(_fixedValueDouble);
	layout->addWidget(_variable);
	layout->addWidget(_toggleType);
	setLayout(layout);
	SetVisibility();
}

void GenericVaraiableSpinbox::SetValue(const NumberVariable<int> &number)
{
	_numberInt = number;
	const QSignalBlocker b1(_toggleType);
	const QSignalBlocker b2(_variable);
	_toggleType->setChecked(!number.IsFixedType());
	_fixedValueInt->setValue(number._value);
	_variable->SetVariable(number._variable);
	SetVisibility();
}

void GenericVaraiableSpinbox::SetValue(const NumberVariable<double> &number)
{
	_numberDouble = number;
	const QSignalBlocker b1(_toggleType);
	const QSignalBlocker b2(_variable);
	_toggleType->setChecked(!number.IsFixedType());
	_fixedValueDouble->setValue(number._value);
	_variable->SetVariable(number._variable);
	SetVisibility();
}

void GenericVaraiableSpinbox::setMinimum(double value)
{
	_fixedValueInt->setMinimum(value);
	_fixedValueDouble->setMinimum(value);
}

void GenericVaraiableSpinbox::setMaximum(double value)
{
	_fixedValueInt->setMaximum(value);
	_fixedValueDouble->setMaximum(value);
}

void GenericVaraiableSpinbox::setPrefix(const QString &prefix)
{
	_fixedValueInt->setPrefix(prefix);
	_fixedValueDouble->setPrefix(prefix);
}

void GenericVaraiableSpinbox::setSuffix(const QString &suffix)
{
	_fixedValueInt->setSuffix(suffix);
	_fixedValueDouble->setSuffix(suffix);
}

void GenericVaraiableSpinbox::SetFixedValue(int value)
{
	_numberInt._value = value;
	const QSignalBlocker b(_fixedValueInt);
	_fixedValueInt->setValue(value);
	EmitSignals();
}

void GenericVaraiableSpinbox::SetFixedValue(double value)
{
	_numberDouble._value = value;
	const QSignalBlocker b(_fixedValueDouble);
	_fixedValueDouble->setValue(value);
	EmitSignals();
}

void GenericVaraiableSpinbox::VariableChanged(const QString &name)
{
	_numberInt._variable = GetWeakVariableByQString(name);
	_numberDouble._variable = GetWeakVariableByQString(name);
	EmitSignals();
}

void GenericVaraiableSpinbox::ToggleTypeClicked(bool useVariable)
{
	_numberInt._type = useVariable ? NumberVariable<int>::Type::VARIABLE
				       : NumberVariable<int>::Type::FIXED_VALUE;
	_numberDouble._type =
		useVariable ? NumberVariable<double>::Type::VARIABLE
			    : NumberVariable<double>::Type::FIXED_VALUE;

	SetVisibility();
	EmitSignals();
}

void GenericVaraiableSpinbox::EmitSignals()
{
	if (_wholeNumber) {
		emit FixedValueChanged(_numberInt);
		emit NumberVariableChanged(_numberInt);
	} else {
		emit FixedValueChanged(_numberDouble);
		emit NumberVariableChanged(_numberDouble);
	}
}

void GenericVaraiableSpinbox::SetVisibility()
{
	if (_wholeNumber) {
		_fixedValueDouble->hide();
		SetVisibilityInt();
	} else {
		_fixedValueInt->hide();
		SetVisibilityDouble();
	}
}

void GenericVaraiableSpinbox::SetVisibilityInt()
{
	if (_numberInt.IsFixedType()) {
		_fixedValueInt->show();
		_variable->hide();
		_toggleType->setVisible(!switcher->variables.empty());
	} else {
		_fixedValueInt->hide();
		_variable->show();
		_toggleType->show();
	}
	adjustSize();
	updateGeometry();
}

void GenericVaraiableSpinbox::SetVisibilityDouble()
{
	if (_numberDouble.IsFixedType()) {
		_fixedValueDouble->show();
		_variable->hide();
		_toggleType->setVisible(!switcher->variables.empty());
	} else {
		_fixedValueDouble->hide();
		_variable->show();
		_toggleType->show();
	}
	adjustSize();
	updateGeometry();
}

VariableSpinBox::VariableSpinBox(QWidget *parent)
	: GenericVaraiableSpinbox(parent, true)
{
}

VariableDoubleSpinBox::VariableDoubleSpinBox(QWidget *parent)
	: GenericVaraiableSpinbox(parent, false)
{
}
