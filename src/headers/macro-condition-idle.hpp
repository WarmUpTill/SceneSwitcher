#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QWidget>
#include <QComboBox>

class MacroConditionIdle : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionIdle>();
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
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);

protected:
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionIdle> _entryData;

private:
	bool _loading = true;
};
