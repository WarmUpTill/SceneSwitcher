#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QCheckBox>
#include <QPushButton>
#include <QTimer>

class MacroConditionTimer : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionTimer>();
	}
	void Pause();
	void Continue();
	void Reset();

	Duration _duration;
	bool _paused = false;
	bool _saveRemaining = false;
	double _remaining = 0.;
	bool _oneshot = false;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionTimerEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionTimerEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionTimer> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionTimerEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionTimer>(cond));
	}

private slots:
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);
	void SaveRemainingChanged(int state);
	void AutoResetChanged(int state);
	void PauseContinueClicked();
	void ResetClicked();
	void UpdateTimeRemaining();

protected:
	void SetPauseContinueButtonLabel();

	DurationSelection *_duration;
	QCheckBox *_autoReset;
	QCheckBox *_saveRemaining;
	QPushButton *_reset;
	QPushButton *_pauseConinue;
	QLabel *_remaining;
	std::shared_ptr<MacroConditionTimer> _entryData;

private:
	QTimer timer;
	bool _loading = true;
};
