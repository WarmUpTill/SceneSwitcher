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
			     const std::string &triggerSignalName,
			     const std::string &completionSignalName,
			     const std::string &newInstanceSignalName,
			     const std::string &deletedInstanceSignalName);
	MacroConditionScript(const advss::MacroConditionScript &);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };

private:
	void WaitForCompletion() const;
	void RegisterTempVarHelper(const std::string &variableId,
				   const std::string &name,
				   const std::string &helpText);
	void DeregisterAllTempVarsHelper();
	void SetTempVarValueHelper(const std::string &variableId,
				   const std::string &value);
	void SetupTempVars();

	std::string _id = "";
};

} // namespace advss
