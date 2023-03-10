#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QComboBox>
#include <QHBoxLayout>

#include <random>

enum class TimerType {
	FIXED,
	RANDOM,
};

class MacroConditionTimer : public MacroCondition {
public:
	MacroConditionTimer(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionTimer>(m);
	}
	void Pause();
	void Continue();
	void Reset();

	TimerType _type = TimerType::FIXED;
	Duration _duration;
	Duration _duration2;
	bool _paused = false;
	bool _saveRemaining = false;
	double _remaining = 0.;
	bool _oneshot = false;

private:
	void SetRandomTimeRemaining();
	std::default_random_engine _re;
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
	void TimerTypeChanged(int type);
	void DurationChanged(const Duration &seconds);
	void Duration2Changed(const Duration &seconds);
	void SaveRemainingChanged(int state);
	void AutoResetChanged(int state);
	void PauseContinueClicked();
	void ResetClicked();
	void UpdateTimeRemaining();

protected:
	void SetPauseContinueButtonLabel();

	QComboBox *_timerTypes;
	DurationSelection *_duration;
	DurationSelection *_duration2;
	QCheckBox *_autoReset;
	QCheckBox *_saveRemaining;
	QPushButton *_reset;
	QPushButton *_pauseConinue;
	QLabel *_remaining;
	std::shared_ptr<MacroConditionTimer> _entryData;

private:
	void SetWidgetVisibility();

	QHBoxLayout *_timerLayout;
	QTimer timer;
	bool _loading = true;
};
