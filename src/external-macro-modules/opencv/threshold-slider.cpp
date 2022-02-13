#include "threshold-slider.hpp"
#include <QHBoxLayout>

ThresholdSlider::ThresholdSlider(double min, double max, const QString &label,
				 const QString &description, QWidget *parent)
	: QWidget(parent)
{
	_slider = new QSlider();
	_slider->setOrientation(Qt::Horizontal);
	_slider->setRange(min * _scale, max * _scale);
	_value = new QLabel();
	QString labelText = label + QString("0.");
	for (int i = 0; i < _precision; i++) {
		labelText.append(QString("0"));
	}
	_value->setText(labelText);
	connect(_slider, SIGNAL(valueChanged(int)), this,
		SLOT(NotifyValueChanged(int)));
	QVBoxLayout *mainLayout = new QVBoxLayout();
	QHBoxLayout *sliderLayout = new QHBoxLayout();
	sliderLayout->addWidget(_value);
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
	_slider->setValue(value * _scale);
	SetDoubleValueText(value);
}

void ThresholdSlider::NotifyValueChanged(int value)
{
	double doubleValue = value / _scale;
	SetDoubleValueText(doubleValue);
	emit DoubleValueChanged(doubleValue);
}

void ThresholdSlider::SetDoubleValueText(double value)
{
	QString labelText = _value->text();
	labelText.chop(_precision + 2); // 2 for the part left of the "."
	labelText.append(QString::number(value, 'f', _precision));
	_value->setText(labelText);
}
