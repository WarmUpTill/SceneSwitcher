#pragma once
#include "macro-condition-edit.hpp"
#include <QWidget>
#include <QComboBox>

namespace advss {

class MacroConditionPluginState : public MacroCondition {
public:
	MacroConditionPluginState(Macro *m) : MacroCondition(m) {}
	~MacroConditionPluginState();
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionPluginState>(m);
	}

	enum class Condition {
		PLUGIN_START,
		PLUGIN_RESTART,
		PLUGIN_RUNNING,
		OBS_SHUTDOWN,
		SCENE_COLLECTION_CHANGE,
		PLUGIN_SCENE_CHANGE,
	};

	Condition _condition = Condition::PLUGIN_SCENE_CHANGE;

private:
	bool _firstCheckAfterSceneCollectionChange = true;

	const static uint32_t _version;
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
	void ConditionChanged(int idx);

protected:
	void SetWidgetVisibility();

	QComboBox *_condition;
	QLabel *_shutdownLimitation;
	std::shared_ptr<MacroConditionPluginState> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
