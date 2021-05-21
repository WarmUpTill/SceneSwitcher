#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

enum class WaitType {
	FIXED,
	RANDOM,
};

class MacroActionWait : public MacroAction {
public:
	bool PerformAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionWait>();
	}
	Duration _duration;
	Duration _duration2;
	WaitType _waitType = WaitType::FIXED;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionWaitEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWaitEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWait> entryData = nullptr);
	void UpdateEntryData();
	void SetupFixedDurationEdit();
	void SetupRandomDurationEdit();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWait>(action));
	}

private slots:
	void DurationChanged(double value);
	void DurationUnitChanged(DurationUnit unit);
	void Duration2Changed(double value);
	void Duration2UnitChanged(DurationUnit unit);
	void TypeChanged(int value);

protected:
	DurationSelection *_duration;
	DurationSelection *_duration2;
	QComboBox *_waitType;
	std::shared_ptr<MacroActionWait> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
