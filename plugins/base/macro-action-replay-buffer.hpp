#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

#include <QHBoxLayout>

namespace advss {

class MacroActionReplayBuffer : public MacroAction {
public:
	MacroActionReplayBuffer(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	enum class Action {
		STOP,
		START,
		SAVE,
		DURATION,
	};
	Action _action = Action::STOP;
	Duration _duration;

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
	void DurationChanged(const Duration &);

private:
	void SetWidgetVisibility();

	QComboBox *_actions;
	QLabel *_warning;
	DurationSelection *_duration;

	std::shared_ptr<MacroActionReplayBuffer> _entryData;
	bool _loading = true;
};

} // namespace advss
