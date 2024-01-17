#pragma once
#include "macro-segment.hpp"
#include "macro-ref.hpp"

namespace advss {

class EXPORT MacroAction : public MacroSegment {
public:
	MacroAction(Macro *m, bool supportsVariableValue = false);
	virtual ~MacroAction() = default;
	virtual bool PerformAction() = 0;
	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual void LogAction() const;
	void SetEnabled(bool);
	bool Enabled() const;

	static std::string_view GetDefaultID();

private:
	bool _enabled = true;
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
