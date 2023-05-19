#pragma once
#include "variable-spinbox.hpp"

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QDoubleSpinBox>

namespace advss {

class SliderSpinBox : public QWidget {
	Q_OBJECT

public:
	SliderSpinBox(double min = 0., double max = 1.,
		      const QString &label = "threshold",
		      const QString &description = "",
		      bool descriptionAsTooltip = false,
		      QWidget *parent = nullptr);
	void SetDoubleValue(double);
	void SetDoubleValue(const NumberVariable<double> &);
public slots:
	void SliderValueChanged(int value);
	void SpinBoxValueChanged(const NumberVariable<double> &value);
signals:
	void DoubleValueChanged(const NumberVariable<double> &value);

private:
	void SetVisibility(const NumberVariable<double> &);

	VariableDoubleSpinBox *_spinBox;
	QSlider *_slider;
	double _scale = 100.0;
};

} // namespace advss
