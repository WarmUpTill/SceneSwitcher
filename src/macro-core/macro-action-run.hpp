#pragma once
#include "macro-action-edit.hpp"
#include "process-config.hpp"

class MacroActionRun : public MacroAction {
public:
	MacroActionRun(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionRun>(m);
	}

	ProcessConfig _procConfig;

private:
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
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionRun> _entryData;

private:
	ProcessConfigEdit *_procConfig;
	bool _loading = true;
};
