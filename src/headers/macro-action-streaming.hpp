#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"

enum class StreamAction {
	STOP,
	START,
};

class MacroActionStream : public MacroAction {
public:
	bool PerformAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionStream>();
	}

	StreamAction _action = StreamAction::STOP;

private:
	static bool _registered;
	static const int id;
};

class MacroActionStreamEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionStreamEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionStream> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionStreamEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionStream>(action));
	}

private slots:
	void ActionChanged(int value);

protected:
	QComboBox *_actions;
	std::shared_ptr<MacroActionStream> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
