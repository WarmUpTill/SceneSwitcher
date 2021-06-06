#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

enum class ReplayBufferAction {
	STOP,
	START,
	SAVE,
};

class MacroActionReplayBuffer : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionReplayBuffer>();
	}

	ReplayBufferAction _action = ReplayBufferAction::STOP;

private:
	// Add artifical delay before trying to save the replay buffer again.
	//
	// Continiously calling obs_frontend_replay_buffer_save() does not
	// result in any output actually being written.
	// OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED also seems to be sent before
	// any data is written.
	Duration _duration;

	static bool _registered;
	static const std::string id;
};

class MacroActionReplayBufferEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionReplayBufferEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionReplayBuffer> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionReplayBufferEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionReplayBuffer>(
				action));
	}

private slots:
	void ActionChanged(int value);

protected:
	QComboBox *_actions;
	std::shared_ptr<MacroActionReplayBuffer> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
