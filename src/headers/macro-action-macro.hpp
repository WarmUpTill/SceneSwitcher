#pragma once
#include "macro-action-edit.hpp"
#include "macro-selection.hpp"

#include <QHBoxLayout>

enum class PerformMacroAction {
	PAUSE,
	UNPAUSE,
	RESET_COUNTER,
	RUN,
	STOP,
};

class MacroActionMacro : public MacroRefAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionMacro>();
	}

	PerformMacroAction _action = PerformMacroAction::PAUSE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionMacroEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionMacroEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionMacro> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionMacroEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionMacro>(action));
	}

private slots:
	void MacroChanged(const QString &text);
	void MacroRemove(const QString &name);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	MacroSelection *_macros;
	QComboBox *_actions;
	std::shared_ptr<MacroActionMacro> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
