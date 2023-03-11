#pragma once
#include "macro-segment.hpp"
#include "macro-ref.hpp"

class MacroAction : public MacroSegment {
public:
	MacroAction(Macro *m, bool supportsVariableValue = false);
	virtual ~MacroAction() = default;
	virtual bool PerformAction() = 0;
	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual void LogAction() const;
};

class MacroRefAction : virtual public MacroAction {
public:
	MacroRefAction(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	MacroRef _macro;
};

class MultiMacroRefAction : virtual public MacroAction {
public:
	MultiMacroRefAction(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	std::vector<MacroRef> _macros;
};
