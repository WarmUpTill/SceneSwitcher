#pragma once
#include "macro-segment.hpp"
#include "macro-ref.hpp"

class MacroAction : public MacroSegment {
public:
	MacroAction(Macro *m) : MacroSegment(m) {}
	virtual bool PerformAction() = 0;
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual void LogAction();
};

class MacroRefAction : virtual public MacroAction {
public:
	MacroRefAction(Macro *m) : MacroAction(m) {}
	void ResolveMacroRef();
	MacroRef _macro;
};

class MultiMacroRefAction : virtual public MacroAction {
public:
	MultiMacroRefAction(Macro *m) : MacroAction(m) {}
	void ResolveMacroRef();
	std::vector<MacroRef> _macros;
};
