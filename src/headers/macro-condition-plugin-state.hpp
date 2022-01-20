#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class PluginStateCondition {
	SCENESWITCHED,
	RUNNING,
};

class MacroConditionPluginState : public MacroCondition {
public:
	MacroConditionPluginState(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionPluginState>(m);
	}

	PluginStateCondition _condition = PluginStateCondition::SCENESWITCHED;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionPluginStateEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionPluginStateEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionPluginState> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionPluginStateEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionPluginState>(
				cond));
	}

private slots:
	void ConditionChanged(int cond);

protected:
	QComboBox *_condition;
	std::shared_ptr<MacroConditionPluginState> _entryData;

private:
	bool _loading = true;
};
