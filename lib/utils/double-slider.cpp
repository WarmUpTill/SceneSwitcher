#include "double-slider.hpp"
#include <QWheelEvent>

#include <cmath>

namespace advss {

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation,
				       QWidget *parent)
	: QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus()) {
		event->ignore();
	} else {
		QSlider::wheelEvent(event);
	}
}

DoubleSlider::DoubleSlider(QWidget *parent) : SliderIgnoreScroll(parent)
{
	connect(this, &DoubleSlider::valueChanged, [this](int val) {
		emit DoubleValChanged((minVal / minStep + val) * minStep);
	});
}

void DoubleSlider::SetDoubleConstraints(double newMin, double newMax,
					double newStep, double val)
{
	minVal = newMin;
	maxVal = newMax;
	minStep = newStep;

	double total = maxVal - minVal;
	int intMax = int(total / minStep);

	setMinimum(0);
	setMaximum(intMax);
	setSingleStep(1);
	SetDoubleVal(val);
}

double DoubleSlider::DoubleValue()
{
	return (minVal / minStep + value()) * minStep;
}

void DoubleSlider::SetDoubleVal(double val)
{
	setValue(lround((val - minVal) / minStep));
}

} // namespace advss
