#pragma once
#include "macro-action-edit.hpp"
#include "macro-script-handler.hpp"
#include "macro-segment-script.hpp"

namespace advss {

class MacroActionScript : public MacroAction, public MacroSegmentScript {
public:
	MacroActionScript(Macro *m, const std::string &id,
			  const OBSData &defaultSettings,
			  const std::string &propertiesSignalName,
			  const std::string &triggerSignalName,
			  const std::string &completionSignalName,
			  const std::string &newInstanceSignalName,
			  const std::string &deletedInstanceSignalName);
	MacroActionScript(const advss::MacroActionScript &);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	std::shared_ptr<MacroAction> Copy() const;

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
