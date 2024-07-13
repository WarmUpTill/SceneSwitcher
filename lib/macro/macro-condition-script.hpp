#pragma once
#include "macro-condition-edit.hpp"
#include "macro-script-handler.hpp"
#include "macro-segment-script.hpp"

namespace advss {

class MacroConditionScript : public MacroCondition, public MacroSegmentScript {
public:
	MacroConditionScript(Macro *m, const std::string &id,
			     const OBSData &defaultSettings,
			     const std::string &propertiesSignalName,
			     const std::string &triggerSignal,
			     const std::string &signalComplete);
	MacroConditionScript(const advss::MacroConditionScript &);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };

private:
	void WaitForCompletion() const;

	std::string _id = "";
};

} // namespace advss
