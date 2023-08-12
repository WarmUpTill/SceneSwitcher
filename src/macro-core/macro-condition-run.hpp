#pragma once
#include "macro-condition-edit.hpp"
#include "process-config.hpp"
#include "duration-control.hpp"

#include <thread>
#include <QCheckBox>
#include <QSpinBox>

namespace advss {

class MacroConditionRun : public MacroCondition {
public:
	MacroConditionRun(Macro *m) : MacroCondition(m, true) {}
	~MacroConditionRun();
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionRun>(m);
	}

	ProcessConfig _procConfig;
	bool _checkExitCode = true;
	int _exitCode = 0;
	Duration _timeout = Duration(0.1);

private:
	enum class Status {
		NONE,
		FAILED_TO_START,
		TIMEOUT,
		OK,
	};

	void RunProcess();

	std::thread _thread;
	std::atomic_bool _threadDone{true};
	Status _procStatus = Status::NONE;
	int _procExitCode = 0;

	static bool _registered;
	static const std::string id;
};

class MacroConditionRunEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionRunEdit(QWidget *parent,
			      std::shared_ptr<MacroConditionRun> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionRunEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionRun>(cond));
	}

private slots:
	void ProcessConfigChanged(const ProcessConfig &);
	void TimeoutChanged(const Duration &);
	void CheckExitCodeChanged(int);
	void ExitCodeChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	ProcessConfigEdit *_procConfig;
	QCheckBox *_checkExitCode;
	QSpinBox *_exitCode;
	DurationSelection *_timeout;
	std::shared_ptr<MacroConditionRun> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
