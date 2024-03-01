#pragma once
#include "macro-action-edit.hpp"
#include "macro-selection.hpp"
#include "macro-segment-selection.hpp"

#include <QHBoxLayout>

namespace advss {

class MacroActionMacro : public MacroRefAction {
public:
	MacroActionMacro(Macro *m) : MacroAction(m), MacroRefAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		PAUSE,
		UNPAUSE,
		RESET_COUNTER,
		RUN,
		STOP,
		DISABLE_ACTION,
		ENABLE_ACTION,
		TOGGLE_ACTION,
	};
	Action _action = Action::PAUSE;
	IntVariable _actionIndex = 1;

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
	void ActionChanged(int value);
	void ActionIndexChanged(const IntVariable &value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	MacroSelection *_macros;
	MacroSegmentSelection *_actionIndex;
	QComboBox *_actions;
	std::shared_ptr<MacroActionMacro> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
