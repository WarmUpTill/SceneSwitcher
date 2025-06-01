#pragma once
#include "macro-action-edit.hpp"
#include "macro-segment-script-inline.hpp"

namespace advss {

class MacroActionScriptInline : public MacroAction,
				public MacroSegmentScriptInline {
public:
	MacroActionScriptInline(Macro *m) : MacroAction(m) {}

	bool PerformAction();
	void LogAction() const;

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string GetId() const { return _id; };

	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	void ResolveVariablesToFixedValues();

private:
	static bool _registered;
	static const std::string _id;
};

class MacroActionScriptInlineEdit : public MacroSegmentScriptInlineEdit {
	Q_OBJECT

public:
	MacroActionScriptInlineEdit(
		QWidget *, std::shared_ptr<MacroActionScriptInline> = nullptr);
	static QWidget *Create(QWidget *, std::shared_ptr<MacroAction>);
};

} // namespace advss
