#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

namespace advss {

enum class RecordState {
	STOP,
	PAUSE,
	START,
};

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

	RecordState _recordState = RecordState::STOP;

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

protected:
	QComboBox *_recordState;
	std::shared_ptr<MacroConditionRecord> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
