#pragma once
#include "macro-action-edit.hpp"
#include "help-icon.hpp"
#include "process-config.hpp"
#include "duration-control.hpp"

#include <QCheckBox>

namespace advss {

class MacroActionRun : public MacroAction {
public:
	MacroActionRun(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();
	void SetWaitEnabled(bool value);
	bool IsWaitEnabled() const { return _wait; }

	ProcessConfig _procConfig;
	Duration _timeout = 1;

private:
	void SetupTempVars();
	void SetTempVarValues();

	bool _wait = false;
	static bool _registered;
	static const std::string id;
};

class MacroActionRunEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionRunEdit(QWidget *parent,
			   std::shared_ptr<MacroActionRun> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionRunEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionRun>(action));
	}

private slots:
	void ProcessConfigChanged(const ProcessConfig &);
	void ProcessConfigAdvancedSettingsShown();
	void WaitChanged(int);
	void TimeoutChanged(const Duration &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	ProcessConfigEdit *_procConfig;
	QHBoxLayout *_waitLayout;
	QCheckBox *_wait;
	DurationSelection *_timeout;
	HelpIcon *_waitHelp;

	std::shared_ptr<MacroActionRun> _entryData;
	bool _loading = true;
};

} // namespace advss
