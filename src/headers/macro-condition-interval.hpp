#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>
#include "duration-control.hpp"

class MacroConditionInterval : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionInterval>();
	}

	Duration _duration;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionIntervalEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionIntervalEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionInterval> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionIntervalEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionInterval>(
				cond));
	}

private slots:
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);

protected:
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionInterval> _entryData;

private:
	bool _loading = true;
};
