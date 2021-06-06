#pragma once
#include "macro.hpp"
#include "macro-selection.hpp"
#include <QWidget>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>

enum class CounterCondition {
	BELOW,
	ABOVE,
	EQUAL,
};

class MacroConditionCounter : public MacroRefCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionCounter>();
	}

	CounterCondition _condition = CounterCondition::BELOW;
	int _count = 0;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionCounterEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionCounterEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionCounter> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionCounterEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionCounter>(cond));
	}

private slots:
	void MacroChanged(const QString &text);
	void MacroRemove(const QString &name);
	void CountChanged(int value);
	void ConditionChanged(int cond);
	void ResetClicked();
	void UpdateCount();

protected:
	MacroSelection *_macros;
	QComboBox *_conditions;
	QSpinBox *_count;
	QLabel *_currentCount;
	QPushButton *_resetCount;
	std::unique_ptr<QTimer> _timer;
	std::shared_ptr<MacroConditionCounter> _entryData;

private:
	void ResetTimer();
	bool _loading = true;
};
