#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"

class MacroActionWait : public MacroAction {
public:
	bool PerformAction();
	bool Save();
	bool Load();
	int GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionWait>();
	}
	double _duration = 0.;

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
	static QWidget *Create(std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitEdit(
			std::dynamic_pointer_cast<MacroActionWait>(action));
	}

private slots:
	void DurationChanged(double value);

protected:
	QDoubleSpinBox *_duration;
	std::shared_ptr<MacroActionWait> _entryData;

private:
	bool _loading = true;
};
