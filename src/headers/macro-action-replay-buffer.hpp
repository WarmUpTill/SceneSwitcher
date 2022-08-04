#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

#include <QHBoxLayout>

enum class ReplayBufferAction {
	STOP,
	START,
	SAVE,
};

class MacroActionReplayBuffer : public MacroAction {
public:
	MacroActionReplayBuffer(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionReplayBuffer>(m);
	}

	ReplayBufferAction _action = ReplayBufferAction::STOP;

private:
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
	QLabel *_saveWarning;
	std::shared_ptr<MacroActionReplayBuffer> _entryData;

private:
	bool _loading = true;
};
