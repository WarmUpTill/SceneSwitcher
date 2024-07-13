#pragma once
#include "duration-control.hpp"
#include "macro-script-handler.hpp"

#include <atomic>
#include <obs-data.h>

namespace advss {

class Macro;
class MacroAction;
class MacroCondition;

class MacroSegmentScript {
public:
	MacroSegmentScript(obs_data_t *defaultSettings,
			   const std::string &propertiesSignalName,
			   const std::string &triggerSignal,
			   const std::string &completionSignal);
	MacroSegmentScript(const advss::MacroSegmentScript &);

protected:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	obs_properties_t *GetProperties() const;
	OBSData GetSettings() const { return _settings.Get(); }
	void UpdateSettings(obs_data_t *newSettings) const;

	bool SendTriggerSignal();
	double GetTimeoutSeconds() const { return _timeout.Seconds(); };
	bool TriggerIsCompleted() const { return _triggerIsComplete; }

private:
	virtual void WaitForCompletion() const = 0;
	static void CompletionSignalReceived(void *param, calldata_t *data);

	OBSDataAutoRelease _settings;
	std::string _propertiesSignal = "";

	std::string _triggerSignal = "";
	std::string _completionSignal = "";
	std::atomic_bool _triggerIsComplete = {false};
	bool _triggerResult = false;
	int64_t _completionId = 0;

	Duration _timeout = Duration(10.0);

	friend class MacroSegmentScriptEdit;
};

class MacroSegmentScriptEdit : public QWidget {
	Q_OBJECT

public:
	MacroSegmentScriptEdit(
		QWidget *parent,
		std::shared_ptr<MacroSegmentScript> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> segment);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> segment);

private slots:
	void TimeoutChanged(const Duration &);

private:
	static obs_properties_t *GetProperties(void *obj);
	static void UpdateSettings(void *obj, obs_data_t *settings);

	DurationSelection *_timeout;

	std::shared_ptr<MacroSegmentScript> _entryData;
	bool _loading = true;
};

} // namespace advss
