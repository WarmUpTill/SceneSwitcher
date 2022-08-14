#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class ReplayBufferState {
	STOP,
	START,
	SAVE,
};

class MacroConditionReplayBuffer : public MacroCondition {
public:
	MacroConditionReplayBuffer(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionReplayBuffer>(m);
	}

	ReplayBufferState _state = ReplayBufferState::STOP;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionReplayBufferEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionReplayBufferEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionReplayBuffer> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionReplayBufferEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionReplayBuffer>(
				cond));
	}

private slots:
	void StateChanged(int value);

protected:
	QComboBox *_state;
	std::shared_ptr<MacroConditionReplayBuffer> _entryData;

private:
	bool _loading = true;
};
