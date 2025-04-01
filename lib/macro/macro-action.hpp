#pragma once
#include "macro-segment.hpp"
#include "macro-ref.hpp"

namespace advss {

class EXPORT MacroAction : public MacroSegment {
public:
	MacroAction(Macro *m, bool supportsVariableValue = false);
	virtual ~MacroAction() = default;
	virtual std::shared_ptr<MacroAction> Copy() const = 0;

	virtual bool PerformAction() = 0;
	virtual void LogAction() const;

	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;

	// Used to resolve variables before actions are added to action queues
	virtual void ResolveVariablesToFixedValues();

	static std::string_view GetDefaultID();

private:
};

class EXPORT MacroRefAction : virtual public MacroAction {
public:
	MacroRefAction(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	MacroRef _macro;
};

class EXPORT MultiMacroRefAction : virtual public MacroAction {
public:
	MultiMacroRefAction(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	std::vector<MacroRef> _macros;
};

} // namespace advss
