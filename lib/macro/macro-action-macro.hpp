#pragma once
#include "macro-action-edit.hpp"
#include "macro.hpp"
#include "macro-edit.hpp"
#include "macro-input.hpp"
#include "macro-selection.hpp"
#include "macro-segment-selection.hpp"
#include "resizable-widget.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

namespace advss {

class MacroActionMacro final : public MacroRefAction {
public:
	MacroActionMacro(Macro *m) : MacroAction(m), MacroRefAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad();
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	struct RunOptions {
		void Save(obs_data_t *obj) const;
		void Load(obs_data_t *obj);
		enum class Logic {
			IGNORE_CONDITIONS,
			CONDITIONS,
			INVERT_CONDITIONS,
		};
		Logic logic;
		bool reevaluateConditionState = false;
		bool runElseActions = false;
		bool skipWhenPaused = true;
		bool setInputs = false;
		StringList inputs;
		MacroRef macro;
	};

	enum class Action {
		PAUSE,
		UNPAUSE,
		RESET_COUNTER,
		RUN_ACTIONS,
		STOP,
		DISABLE_ACTION,
		ENABLE_ACTION,
		TOGGLE_ACTION,
		TOGGLE_PAUSE,
		NESTED_MACRO,
	};
	Action _action = Action::NESTED_MACRO;
	IntVariable _actionIndex = 1;
	RunOptions _runOptions = {};
	std::shared_ptr<Macro> _nestedMacro = std::make_shared<Macro>();
	int _customWidgetHeight = 0;

private:
	void RunActions(Macro *actionMacro) const;

	static bool _registered;
	static const std::string id;
};

class MacroActionMacroEdit final : public ResizableWidget {
	Q_OBJECT

public:
	MacroActionMacroEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionMacro> entryData = nullptr);
	~MacroActionMacroEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *, std::shared_ptr<MacroAction>);

private slots:
	void MacroChanged(const QString &text);
	void ActionChanged(int value);
	void ActionIndexChanged(const IntVariable &value);
	void ConditionMacroChanged(const QString &text);
	void ConditionBehaviorChanged(int value);
	void ReevaluateConditionStateChanged(int value);
	void ActionTypeChanged(int value);
	void SkipWhenPausedChanged(int value);
	void SetInputsChanged(int value);
	void InputsChanged(const StringList &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	void SetupMacroInput(Macro *) const;

	MacroSelection *_macros;
	MacroSegmentSelection *_actionIndex;
	QComboBox *_actions;
	MacroSelection *_conditionMacros;
	QComboBox *_conditionBehaviors;
	QCheckBox *_reevaluateConditionState;
	QComboBox *_actionTypes;
	QCheckBox *_skipWhenPaused;
	QCheckBox *_setInputs;
	MacroInputEdit *_inputs;
	QHBoxLayout *_entryLayout;
	QHBoxLayout *_conditionLayout;
	QHBoxLayout *_reevaluateConditionStateLayout;
	QHBoxLayout *_setInputsLayout;
	MacroEdit *_nestedMacro;

	std::shared_ptr<MacroActionMacro> _entryData;
	bool _loading = true;
};

} // namespace advss
