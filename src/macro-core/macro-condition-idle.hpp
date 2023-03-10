#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QWidget>
#include <QComboBox>

class MacroConditionIdle : public MacroCondition {
public:
	MacroConditionIdle(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionIdle>(m);
	}

	Duration _duration;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionIdleEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionIdleEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionIdle> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionIdleEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionIdle>(cond));
	}

private slots:
	void DurationChanged(const Duration &seconds);

protected:
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionIdle> _entryData;

private:
	bool _loading = true;
};
