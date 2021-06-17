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
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionReplayBuffer>();
	}

	ReplayBufferState _state;

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
