#pragma once
#include "macro-action-edit.hpp"

#include <QComboBox>
#include <QHBoxLayout>

enum class RecordAction {
	STOP,
	START,
	PAUSE,
	UNPAUSE,
};

class MacroActionRecord : public MacroAction {
public:
	MacroActionRecord(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionRecord>(m);
	}

	RecordAction _action = RecordAction::STOP;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionRecordEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionRecordEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionRecord> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionRecordEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionRecord>(action));
	}

private slots:
	void ActionChanged(int value);

protected:
	QComboBox *_actions;
	QLabel *_pauseHint;
	std::shared_ptr<MacroActionRecord> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
