#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "macro-script-handler.hpp"

#include <atomic>

namespace advss {

class MacroActionScript : public MacroAction {
public:
	MacroActionScript(Macro *m, const std::string &id, bool blocking,
			  const std::string &signal,
			  const std::string &signalComplete);
	MacroActionScript(const advss::MacroActionScript &);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return _id; };
	std::shared_ptr<MacroAction> Copy() const;

	Duration _timeout = Duration(10.0);

private:
	static void CompletionSignalReceived(void *param, calldata_t *data);
	void WaitForActionCompletion() const;

	std::string _id = "";
	bool _blocking = false;
	std::string _signal = "";
	std::string _signalComplete = "";
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
	DurationSelection *_timeout;

	std::shared_ptr<MacroActionScript> _entryData;
	bool _loading = true;
};

} // namespace advss
