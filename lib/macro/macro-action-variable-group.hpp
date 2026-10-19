#pragma once
#include "macro-action-variable.hpp"

#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace advss {

struct VariableActionOption {
	MacroActionVariable::Action action;
	const char *label;
	const char *layout = "AdvSceneSwitcher.action.variable.layout.default";
	bool expandInlineRow = false;
};

class VariableActionGroup {
public:
	virtual ~VariableActionGroup() = default;
	virtual std::unique_ptr<VariableActionGroup> Copy() const = 0;
	virtual void Save(obs_data_t *, const MacroActionVariable &) const {}
	virtual void Load(obs_data_t *, MacroActionVariable &) {}
	virtual void PostLoad(MacroActionVariable &) {}
	virtual void ResolveVariables() {}
	virtual std::vector<TempVariableRef> GetTempVarRefs() const
	{
		return {};
	}
	// Returning false aborts the macro
	virtual bool Perform(MacroActionVariable &, MacroActionVariable::Action,
			     Variable &) = 0;
};

class VariableActionGroupEdit : public QWidget {
	Q_OBJECT

public:
	VariableActionGroupEdit(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData);
	virtual std::unordered_map<std::string, QWidget *> GetInlineWidgets()
	{
		return {};
	}
	virtual void UpdateEntryData() = 0;
	virtual void SetWidgetVisibility(MacroActionVariable::Action) = 0;

protected:
	void UpdateSize();

	std::shared_ptr<MacroActionVariable> _entryData;
	bool _loading = true;
};

struct VariableActionGroupInfo {
	int order;
	std::vector<VariableActionOption> options;
	std::function<std::unique_ptr<VariableActionGroup>()> createData;
	std::function<VariableActionGroupEdit *(
		QWidget *, std::shared_ptr<MacroActionVariable>)>
		createEdit;
};

bool RegisterVariableActionGroup(const VariableActionGroupInfo &);
const std::vector<VariableActionGroupInfo> &GetVariableActionGroups();
int GetVariableActionGroupIndex(MacroActionVariable::Action);
const VariableActionOption *
	GetVariableActionOption(MacroActionVariable::Action);

} // namespace advss
