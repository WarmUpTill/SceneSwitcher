#pragma once
#include <QWidget>
#include <QSlider>
#include <QLabel>

class ThresholdSlider : public QWidget {
	Q_OBJECT

public:
	ThresholdSlider(double min = 0., double max = 1.,
			const QString &label = "threshold",
			const QString &description = "", QWidget *parent = 0);
	void SetDoubleValue(double);
public slots:
	void NotifyValueChanged(int value);
signals:
	void DoubleValueChanged(double value);

private:
	void SetDoubleValueText(double);
	QLabel *_value;
	QSlider *_slider;
	double _scale = 100.0;
	int _precision = 2;
};
