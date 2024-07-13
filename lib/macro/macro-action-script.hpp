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
			  const std::string &triggerSignal,
			  const std::string &signalComplete);
	MacroActionScript(const advss::MacroActionScript &);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	std::shared_ptr<MacroAction> Copy() const;

private:
	void WaitForCompletion() const;

	std::string _id = "";
};

} // namespace advss
