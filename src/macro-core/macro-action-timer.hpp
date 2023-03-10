#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "macro-selection.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>

enum class TimerAction {
	PAUSE,
	CONTINUE,
	RESET,
	SET_TIME_REMAINING,
};

class MacroActionTimer : public MacroRefAction {
public:
	MacroActionTimer(Macro *m) : MacroAction(m), MacroRefAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionTimer>(m);
	}
	Duration _duration;
	TimerAction _actionType = TimerAction::PAUSE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionTimerEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTimerEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTimer> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTimerEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTimer>(action));
	}

private slots:
	void MacroChanged(const QString &text);
	void DurationChanged(const Duration &value);
	void ActionTypeChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	void SetWidgetVisibility();

	MacroSelection *_macros;
	DurationSelection *_duration;
	QComboBox *_timerAction;
	std::shared_ptr<MacroActionTimer> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
