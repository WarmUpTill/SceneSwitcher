#include "macro-action-variable-group.hpp"
#include "log-helper.hpp"

#include <algorithm>

namespace advss {

static std::vector<VariableActionGroupInfo> &getGroups()
{
	static std::vector<VariableActionGroupInfo> groups;
	return groups;
}

bool RegisterVariableActionGroup(const VariableActionGroupInfo &info)
{
	for (const auto &option : info.options) {
		if (GetVariableActionGroupIndex(option.action) != -1) {
			blog(LOG_WARNING,
			     "variable action option %d already registered",
			     static_cast<int>(option.action));
			return false;
		}
	}

	auto &groups = getGroups();
	auto it = std::upper_bound(groups.begin(), groups.end(), info,
				   [](const VariableActionGroupInfo &a,
				      const VariableActionGroupInfo &b) {
					   return a.order < b.order;
				   });
	groups.insert(it, info);
	return true;
}

const std::vector<VariableActionGroupInfo> &GetVariableActionGroups()
{
	return getGroups();
}

int GetVariableActionGroupIndex(MacroActionVariable::Action action)
{
	const auto &groups = getGroups();
	for (size_t idx = 0; idx < groups.size(); idx++) {
		for (const auto &option : groups[idx].options) {
			if (option.action == action) {
				return static_cast<int>(idx);
			}
		}
	}
	return -1;
}

const VariableActionOption *
GetVariableActionOption(MacroActionVariable::Action action)
{
	for (const auto &group : getGroups()) {
		for (const auto &option : group.options) {
			if (option.action == action) {
				return &option;
			}
		}
	}
	return nullptr;
}

VariableActionGroupEdit::VariableActionGroupEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: QWidget(parent),
	  _entryData(entryData)
{
}

void VariableActionGroupEdit::UpdateSize()
{
	adjustSize();
	updateGeometry();
	if (auto parent = parentWidget()) {
		parent->adjustSize();
		parent->updateGeometry();
	}
}

} // namespace advss
