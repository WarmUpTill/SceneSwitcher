#pragma once
#include "macro.hpp"
#include "duration-control.hpp"
#include "transition-selection.hpp"

#include <QWidget>
#include <QComboBox>

enum class TransitionCondition {
	CURRENT,
	DURATION,
	STARTED,
	ENDED,
};

class MacroConditionTransition : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionTransition>();
	}
	void ConnectToTransitionSignals();
	void DisconnectTransitionSignals();

	TransitionCondition _condition = TransitionCondition::CURRENT;
	TransitionSelection _transition;
	Duration _duration;

private:
	static void TransitionStarted(void *data, calldata_t *);
	static void TransitionEnded(void *data, calldata_t *);

	bool _started = false;
	bool _ended = false;
	std::chrono::high_resolution_clock::time_point _lastTransitionEndTime{};
	static bool _registered;
	static const std::string id;
};

class MacroConditionTransitionEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionTransitionEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionTransition> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionTransitionEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionTransition>(
				cond));
	}

private slots:
	void ConditionChanged(int cond);
	void TransitionChanged(const TransitionSelection &);
	void DurationChanged(double seconds);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_conditions;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QLabel *_durationSuffix;
	std::shared_ptr<MacroConditionTransition> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
