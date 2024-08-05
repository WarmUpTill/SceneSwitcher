#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "macro-script-handler.hpp"

#include <atomic>
#include <obs-data.h>

namespace advss {

class MacroActionScript : public MacroAction {
public:
	MacroActionScript(Macro *m, const std::string &id,
			  const OBSData &defaultSettings,
			  const std::string &propertiesSignalName,
			  const std::string &signal,
			  const std::string &signalComplete);
	MacroActionScript(const advss::MacroActionScript &);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	std::shared_ptr<MacroAction> Copy() const;

	obs_properties_t *GetProperties() const;
	OBSData GetSettings() const { return _settings; }
	void UpdateSettings(obs_data_t *newSettings) const;

	Duration _timeout = Duration(10.0);

private:
	static void CompletionSignalReceived(void *param, calldata_t *data);
	void WaitForActionCompletion() const;

	std::string _id = "";
	OBSData _settings;
	std::string _propertiesSignal = "";
	std::string _triggerSignal = "";
	std::string _completionSignal = "";
	std::atomic_bool _actionIsComplete = {false};
	int64_t _completionId = 0;
};

class MacroActionScriptEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionScriptEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionScript> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionScriptEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionScript>(action));
	}

private slots:
	void TimeoutChanged(const Duration &);

private:
	static obs_properties_t *GetProperties(void *obj);
	static void UpdateSettings(void *obj, obs_data_t *settings);

	DurationSelection *_timeout;

	std::shared_ptr<MacroActionScript> _entryData;
	bool _loading = true;
};

} // namespace advss
