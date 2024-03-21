#pragma once
#include "macro-action-edit.hpp"
#include "macro-list.hpp"
#include "macro-selection.hpp"
#include "variable-spinbox.hpp"

#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QTimer>

namespace advss {

class MacroActionSequence : public MultiMacroRefAction, public MacroRefAction {
public:
	MacroActionSequence(Macro *m)
		: MacroAction(m),
		  MultiMacroRefAction(m),
		  MacroRefAction(m)
	{
	}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad();
	std::string GetId() const { return id; };
	MacroRef GetNextMacro(bool advance = true);
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		RUN_SEQUENCE,
		SET_INDEX,
	};
	Action _action = Action::RUN_SEQUENCE;
	bool _restart = true;
	IntVariable _resetIndex = 1;

	MacroRef _lastSequenceMacro;
	int _lastIdx = -1;

private:
	bool RunSequence();
	bool SetSequenceIndex() const;

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
	void ActionChanged(int);
	void MacroChanged(const QString &text);
	void ResetIndexChanged(const NumberVariable<int> &);

private:
	void SetWidgetVisibility();

	MacroList *_macroList;
	QPushButton *_continueFrom;
	QCheckBox *_restart;
	QLabel *_statusLine;
	QComboBox *_actions;
	MacroSelection *_macros;
	VariableSpinBox *_resetIndex;
	QHBoxLayout *_layout;
	QTimer _statusTimer;

	std::shared_ptr<MacroActionSequence> _entryData;
	bool _loading = true;
};

} // namespace advss
