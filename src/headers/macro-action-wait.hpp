#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"

enum class WaitType {
	FIXED,
	RANDOM,
};

class MacroActionWait : public MacroAction {
public:
	bool PerformAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionWait>();
	}
	double _duration = 0.;
	double _duration2 = 0.;
	WaitType _waitType = WaitType::FIXED;

private:
	static bool _registered;
	static const int id;
};

class MacroActionWaitEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWaitEdit(
		std::shared_ptr<MacroActionWait> entryData = nullptr);
	void UpdateEntryData();
	void SetupFixedDurationEdit();
	void SetupRandomDurationEdit();
	static QWidget *Create(std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitEdit(
			std::dynamic_pointer_cast<MacroActionWait>(action));
	}

private slots:
	void DurationChanged(double value);
	void Duration2Changed(double value);
	void TypeChanged(int value);

protected:
	QDoubleSpinBox *_duration;
	QDoubleSpinBox *_duration2;
	QComboBox *_waitType;
	std::shared_ptr<MacroActionWait> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
