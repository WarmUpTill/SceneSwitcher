#pragma once
#include "macro-action-edit.hpp"
#include "macro-selection.hpp"
#include "action-queue.hpp"

#include <QHBoxLayout>

namespace advss {

class MacroActionQueue : public MacroRefAction {
public:
	MacroActionQueue(Macro *m) : MacroAction(m), MacroRefAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionQueue>(m);
	}

	enum class Action {
		ADD_TO_QUEUE,
		START_QUEUE,
		STOP_QUEUE,
		CLEAR_QUEUE,
	};
	Action _action = Action::ADD_TO_QUEUE;
	std::weak_ptr<ActionQueue> _queue;

private:
	void AddActions(ActionQueue *);

	static bool _registered;
	static const std::string id;
};

class MacroActionQueueEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionQueueEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionQueue> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionQueueEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionQueue>(action));
	}

private slots:
	void MacroChanged(const QString &text);
	void QueueChanged(const QString &);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	MacroSelection *_macros;
	ActionQueueSelection *_queues;
	QComboBox *_actions;
	QHBoxLayout *_layout;

	std::shared_ptr<MacroActionQueue> _entryData;
	bool _loading = true;
};

} // namespace advss
