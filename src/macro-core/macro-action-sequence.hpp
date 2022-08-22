#pragma once
#include "macro-action-edit.hpp"
#include "macro-list.hpp"

#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QTimer>

class MacroActionSequence : public MultiMacroRefAction {
public:
	MacroActionSequence(Macro *m) : MacroAction(m), MultiMacroRefAction(m)
	{
	}
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
	void Add(const std::string &);
	void Remove(int);
	void Up(int);
	void Down(int);
	void Replace(int, const std::string &);
	void ContinueFromClicked();
	void RestartChanged(int state);
	void UpdateStatusLine();

protected:
	std::shared_ptr<MacroActionSequence> _entryData;

private:
	MacroList *_list;
	QPushButton *_continueFrom;
	QCheckBox *_restart;
	QLabel *_statusLine;
	QTimer _statusTimer;
	bool _loading = true;
};
