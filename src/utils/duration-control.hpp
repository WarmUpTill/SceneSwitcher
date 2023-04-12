#pragma once
#include "variable-spinbox.hpp"
#include "obs-data.h"

#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <chrono>

namespace advss {

class Duration {
public:
	Duration() = default;
	Duration(double initialValueInSeconds);

	void Save(obs_data_t *obj, const char *name = "duration") const;
	void Load(obs_data_t *obj, const char *name = "duration");

	bool DurationReached();
	bool IsReset() const;
	double Seconds() const;
	double Milliseconds() const;
	double TimeRemaining() const;
	void SetTimeRemaining(double);
	void Reset();
	std::string ToString() const;

	enum class Unit {
		SECONDS,
		MINUTES,
		HOURS,
	};
	Unit GetUnit() const { return _unit; }
	void SetUnit(Unit u);

private:
	NumberVariable<double> _value = 0.;
	Unit _unit = Unit::SECONDS;
	std::chrono::high_resolution_clock::time_point _startTime;

	friend class DurationSelection;
};

class DurationSelection : public QWidget {
	Q_OBJECT
public:
	DurationSelection(QWidget *parent = nullptr,
			  bool showUnitSelection = true, double minValue = 0.0);
	void SetDuration(const Duration &);
	QDoubleSpinBox *SpinBox() { return _duration->SpinBox(); }

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
