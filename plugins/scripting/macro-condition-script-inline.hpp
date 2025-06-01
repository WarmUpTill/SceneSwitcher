#pragma once
#include "macro-condition-edit.hpp"
#include "macro-segment-script-inline.hpp"

namespace advss {

class MacroConditionScriptInline : public MacroCondition,
				   public MacroSegmentScriptInline {
public:
	MacroConditionScriptInline(Macro *m) : MacroCondition(m) {}

	bool CheckCondition();

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string GetId() const { return _id; };

	static std::shared_ptr<MacroCondition> Create(Macro *m);

private:
	static bool _registered;
	static const std::string _id;
};

class MacroConditionScriptInlineEdit : public MacroSegmentScriptInlineEdit {
	Q_OBJECT

public:
	MacroConditionScriptInlineEdit(
		QWidget *,
		std::shared_ptr<MacroConditionScriptInline> = nullptr);
	static QWidget *Create(QWidget *, std::shared_ptr<MacroCondition>);
};

} // namespace advss
