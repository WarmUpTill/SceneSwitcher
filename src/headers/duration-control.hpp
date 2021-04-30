#pragma once
#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <chrono>

#include "obs-data.h"

enum class DurationUnit {
	SECONDS,
	MINUTES,
	HOURS,
};

class Duration {
public:
	void Save(obs_data_t *obj, const char *secondsName="seconds",
		  const char *unitName = "displayUnit");
	void Load(obs_data_t *obj, const char *secondsName="seconds",
		  const char *unitName = "displayUnit");

	bool DurationReached();
	void Reset();

	double seconds = 0.;
	// only used for UI
	DurationUnit displayUnit = DurationUnit::SECONDS;

private:
	std::chrono::high_resolution_clock::time_point _startTime;
};

class DurationSelection : public QWidget {
	Q_OBJECT
public:
	DurationSelection(QWidget *parent = nullptr,
			  bool showUnitSelection = true);
	void SetValue(double value);
	void SetUnit(DurationUnit u);
	void SetDuration(Duration d);

private slots:
	void _DurationChanged(double value);
	void _UnitChanged(int idx);
signals:
	void DurationChanged(double value); // always reutrn value in seconds
	void UnitChanged(DurationUnit u);

private:
	QDoubleSpinBox *_duration;
	QComboBox *_unitSelection;

	double _unitMultiplier;
};
