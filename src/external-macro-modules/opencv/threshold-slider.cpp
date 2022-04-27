#include "threshold-slider.hpp"
#include <QHBoxLayout>

ThresholdSlider::ThresholdSlider(double min, double max, const QString &label,
				 const QString &description, QWidget *parent)
	: QWidget(parent),
	  _spinBox(new QDoubleSpinBox()),
	  _slider(new QSlider())
{
	_slider->setOrientation(Qt::Horizontal);
	_slider->setRange(min * _scale, max * _scale);
	_spinBox->setMinimum(min);
	_spinBox->setMaximum(max);
	_spinBox->setDecimals(5);

	connect(_slider, SIGNAL(valueChanged(int)), this,
		SLOT(SliderValueChanged(int)));
	connect(_spinBox, SIGNAL(valueChanged(double)), this,
		SLOT(SpinBoxValueChanged(double)));
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

void ThresholdSlider::SetDoubleValue(double value)
{
	const QSignalBlocker b1(_slider);
	const QSignalBlocker b2(_spinBox);
	_slider->setValue(value * _scale);
	_spinBox->setValue(value);
}

void ThresholdSlider::SpinBoxValueChanged(double value)
{
	int sliderPos = value * _scale;
	_slider->setValue(sliderPos);
	emit DoubleValueChanged(value);
}

void ThresholdSlider::SliderValueChanged(int value)
{
	double doubleValue = value / _scale;
	_spinBox->setValue(doubleValue);
}
