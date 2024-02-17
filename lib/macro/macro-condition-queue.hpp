#pragma once
#include "macro-condition-edit.hpp"
#include "action-queue.hpp"

#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

namespace advss {

class MacroConditionQueue : public MacroCondition {
public:
	MacroConditionQueue(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionQueue>(m);
	}

	enum class Condition {
		STARTED,
		STOPPED,
		SIZE,
	};
	Condition _condition = Condition::STARTED;
	std::weak_ptr<ActionQueue> _queue;
	IntVariable _size = 1;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionQueueEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionQueueEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionQueue> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionQueueEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionQueue>(cond));
	}

private slots:
	void ConditionChanged(int);
	void QueueChanged(const QString &);
	void SizeChanged(const NumberVariable<int> &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_conditions;
	ActionQueueSelection *_queues;
	VariableSpinBox *_size;
	QHBoxLayout *_layout;

	std::shared_ptr<MacroConditionQueue> _entryData;
	bool _loading = true;
};

} // namespace advss
