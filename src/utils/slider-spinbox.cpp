#include "slider-spinbox.hpp"
#include <QHBoxLayout>

namespace advss {

SliderSpinBox::SliderSpinBox(double min, double max, const QString &label,
			     const QString &description, QWidget *parent)
	: QWidget(parent),
	  _spinBox(new VariableDoubleSpinBox()),
	  _slider(new QSlider())
{
	_slider->setOrientation(Qt::Horizontal);
	_slider->setRange(min * _scale, max * _scale);
	_spinBox->setMinimum(min);
	_spinBox->setMaximum(max);
	_spinBox->setDecimals(5);

	connect(_slider, SIGNAL(valueChanged(int)), this,
		SLOT(SliderValueChanged(int)));
	QWidget::connect(
		_spinBox,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this,
		SLOT(SpinBoxValueChanged(const NumberVariable<double> &)));

	QVBoxLayout *mainLayout = new QVBoxLayout();
	QHBoxLayout *sliderLayout = new QHBoxLayout();
	if (!label.isEmpty()) {
		sliderLayout->addWidget(new QLabel(label));
	}
	sliderLayout->addWidget(_spinBox);
	sliderLayout->addWidget(_slider);
	mainLayout->addLayout(sliderLayout);
	if (!description.isEmpty()) {
		mainLayout->addWidget(new QLabel(description));
	}
	mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);
}

void SliderSpinBox::SetDoubleValue(double value)
{
	NumberVariable<double> temp = value;
	SetDoubleValue(temp);
}

void SliderSpinBox::SetDoubleValue(const NumberVariable<double> &value)
{
	const QSignalBlocker b1(_slider);
	const QSignalBlocker b2(_spinBox);
	_slider->setValue(value * _scale);
	_spinBox->SetValue(value);
	SetVisibility(value);
}

void SliderSpinBox::SpinBoxValueChanged(const NumberVariable<double> &value)
{
	if (value.IsFixedType()) {
		int sliderPos = value * _scale;
		_slider->setValue(sliderPos);
	}
	SetVisibility(value);
	emit DoubleValueChanged(value);
}

void SliderSpinBox::SliderValueChanged(int value)
{
	NumberVariable<double> doubleValue = value / _scale;
	_spinBox->SetValue(doubleValue);
}

void SliderSpinBox::SetVisibility(const NumberVariable<double> &value)
{
	_slider->setVisible(value.IsFixedType());
}

} // namespace advss
