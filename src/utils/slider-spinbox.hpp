#pragma once
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QDoubleSpinBox>

class SliderSpinBox : public QWidget {
	Q_OBJECT

public:
	SliderSpinBox(double min = 0., double max = 1.,
		      const QString &label = "threshold",
		      const QString &description = "", QWidget *parent = 0);
	void SetDoubleValue(double);
public slots:
	void SliderValueChanged(int value);
	void SpinBoxValueChanged(double value);
signals:
	void DoubleValueChanged(double value);

private:
	QDoubleSpinBox *_spinBox;
	QSlider *_slider;
	double _scale = 100.0;
};
