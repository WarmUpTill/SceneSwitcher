#pragma once
#include "macro-condition-edit.hpp"
#include "macro-script-handler.hpp"

#include <atomic>

namespace advss {

class MacroConditionScript : public MacroCondition {
public:
	MacroConditionScript(Macro *m, const std::string &id,
			     const std::string &signal);
	MacroConditionScript(const advss::MacroConditionScript &);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };

private:
	static void SignalReceived(void *param, calldata_t *data);

	std::string _id = "";
	std::string _signal = "";
	std::atomic_bool _conditionValue = {false};
};

class MacroConditionScriptEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionScriptEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionScript> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> action)
	{
		return new MacroConditionScriptEdit(
			parent, std::dynamic_pointer_cast<MacroConditionScript>(
					action));
	}

private:
	std::shared_ptr<MacroConditionScript> _entryData;
	bool _loading = true;
};

} // namespace advss
