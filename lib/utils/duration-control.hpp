#pragma once
#include "duration.hpp"
#include "variable-spinbox.hpp"

#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>

namespace advss {

class DurationSelection : public QWidget {
	Q_OBJECT
public:
	EXPORT DurationSelection(QWidget *parent = nullptr,
				 bool showUnitSelection = true,
				 double minValue = 0.0);
	EXPORT void SetDuration(const Duration &);
	Duration GetDuration() const { return _current; }
	EXPORT QDoubleSpinBox *SpinBox() { return _duration->SpinBox(); }

private slots:
	void _DurationChanged(const NumberVariable<double> &value);
	void _UnitChanged(int idx);
signals:
	void DurationChanged(const Duration &value);

private:
	VariableDoubleSpinBox *_duration;
	QComboBox *_unitSelection;

	Duration _current;
};

} // namespace advss
