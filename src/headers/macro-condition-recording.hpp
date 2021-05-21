#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>
#include "duration-control.hpp"

enum class RecordState {
	STOP,
	PAUSE,
	START,
};

class MacroConditionRecord : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionRecord>();
	}

	RecordState _recordState;
	Duration _duration;

private:
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
	void StateChanged(int value);
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);

protected:
	QComboBox *_recordState;
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionRecord> _entryData;

private:
	bool _loading = true;
};
