#pragma once
#include "macro-action-edit.hpp"
#include "macro-edit.hpp"
#include "resizable-widget.hpp"
#include "variable-spinbox.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace advss {

class MacroActionLoop : public MacroAction {
public:
	MacroActionLoop(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad();
	std::string GetId() const { return id; }
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	std::shared_ptr<Macro> _loopMacro = std::make_shared<Macro>();
	IntVariable _maxIterations = 100;
	int _customWidgetHeight = 0;

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroActionLoopEdit : public ResizableWidget {
	Q_OBJECT

public:
	MacroActionLoopEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionLoop> entryData = nullptr);
	~MacroActionLoopEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionLoopEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionLoop>(action));
	}

private slots:
	void MaxIterationsChanged(const NumberVariable<int> &value);

private:
	MacroEdit *_macroEdit;
	VariableSpinBox *_maxIterations;

	std::shared_ptr<MacroActionLoop> _entryData;
	bool _loading = true;
};

} // namespace advss
