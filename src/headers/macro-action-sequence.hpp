#pragma once
#include "macro-action-edit.hpp"
#include "macro-selection.hpp"

#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QTimer>

class MacroActionSequence : public MultiMacroRefAction {
public:
	MacroActionSequence(Macro *m) : MultiMacroRefAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	MacroRef GetNextMacro(bool advance = true);
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSequence>(m);
	}

	bool _restart = true;
	MacroRef _lastSequenceMacro;
	int _lastIdx = -1;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSequenceEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSequenceEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSequence> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSequenceEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSequence>(action));
	}

private slots:
	void MacroRemove(const QString &name);
	void MacroRename(const QString &oldName, const QString &newName);
	void Add();
	void Remove();
	void Up();
	void Down();
	void ContinueFromClicked();
	void RestartChanged(int state);
	void UpdateStatusLine();

protected:
	std::shared_ptr<MacroActionSequence> _entryData;

private:
	int FindEntry(const std::string &macro);
	void SetMacroListSize();

	QListWidget *_macroList;
	QPushButton *_add;
	QPushButton *_remove;
	QPushButton *_up;
	QPushButton *_down;
	QPushButton *_continueFrom;
	QCheckBox *_restart;
	QLabel *_statusLine;
	QTimer _statusTimer;
	bool _loading = true;
};
