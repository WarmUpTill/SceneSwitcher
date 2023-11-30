#pragma once
#include "macro-condition-edit.hpp"

#include <QWidget>
#include <QComboBox>

namespace advss {

class MacroConditionRecord : public MacroCondition {
public:
	MacroConditionRecord(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionRecord>(m);
	}

	enum class Condition {
		STOP,
		PAUSE,
		START,
		DURATION,
	};

	void SetCondition(Condition);
	Condition GetCondition() const { return _condition; }
	Duration _duration;

private:
	void SetupTempVars();

	Condition _condition = Condition::STOP;
	static bool _registered;
	static const std::string id;
};

class MacroConditionRecordEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionRecordEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionRecord> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionRecordEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionRecord>(cond));
	}

private slots:
	void ConditionChanged(int value);
	void DurationChanged(const Duration &);

private:
	void SetWidgetVisibility();

	QComboBox *_condition;
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionRecord> _entryData;
	bool _loading = true;
};

} // namespace advss
