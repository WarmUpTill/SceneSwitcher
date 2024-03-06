// Based on OBS DoubleSlider and SliderIgnoreScroll implementation

#pragma once
#include "export-symbol-helper.hpp"

#include <QSlider>

namespace advss {

class SliderIgnoreScroll : public QSlider {
	Q_OBJECT

public:
	SliderIgnoreScroll(QWidget *parent = nullptr);
	SliderIgnoreScroll(Qt::Orientation orientation,
			   QWidget *parent = nullptr);

protected:
	virtual void wheelEvent(QWheelEvent *event) override;
};

class ADVSS_EXPORT DoubleSlider : public SliderIgnoreScroll {
	Q_OBJECT

	double minVal = 0, maxVal = 100, minStep = 1;

public:
	DoubleSlider(QWidget *parent = nullptr);

	void SetDoubleConstraints(double newMin, double newMax, double newStep,
				  double val);
	double DoubleValue();
signals:
	void DoubleValChanged(double val);

public slots:
	void SetDoubleVal(double val);
};

} // namespace advss
