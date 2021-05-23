#pragma once
#include <QDoubleSpinBox>
#include "macro-action-edit.hpp"

enum class PauseAction {
	PAUSE,
	UNPAUSE,
};

class MacroActionPause : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionPause>();
	}

	PauseAction _action = PauseAction::PAUSE;
	Macro *_macro = nullptr;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionPauseEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionPauseEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionPause> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionPauseEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionPause>(action));
	}

private slots:
	void MacroChanged(const QString &text);
	void ActionChanged(int value);
	void MacroAdd(const QString &name);
	void MacroRemove(const QString &name);
	void MacroRename(const QString &oldName, const QString &newName);

protected:
	QComboBox *_macros;
	QComboBox *_actions;
	std::shared_ptr<MacroActionPause> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
