#pragma once
#include "macro-condition-edit.hpp"
#include "regex-config.hpp"
#include "scene-item-selection.hpp"
#include "scene-selection.hpp"
#include "transform-setting.hpp"
#include "variable-text-edit.hpp"

#include <QCheckBox>

namespace advss {

class MacroConditionSceneTransform : public MacroCondition {
public:
	MacroConditionSceneTransform(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSceneTransform>(m);
	}

	enum class SettingsType { ALL, SINGLE };
	enum class Condition { MATCHES, CHANGED };
	enum class Compare { LESS, EQUAL, MORE };

	void SetSettingsType(SettingsType type);
	SettingsType GetSettingsType() const { return _settingsType; }
	void SetCondition(Condition condition);
	Condition GetCondition() const { return _condition; }

	SceneSelection _scene;
	SceneItemSelection _source;
	Compare _compare = Compare::EQUAL;
	RegexConfig _regex;
	StringVariable _transformString = "";
	StringVariable _singleSetting = "";
	TransformSetting _setting;

private:
	void SetupTempVars();
	bool CheckAllSettings(const std::vector<OBSSceneItem> &);
	bool CheckSingleSetting(const std::vector<OBSSceneItem> &);
	bool
	AnySceneItemTransformSettingChanged(const std::vector<OBSSceneItem> &);
	bool
	AnySceneItemTransformSettingMatches(const std::vector<OBSSceneItem> &);
	void SetTempVarHelper(const std::vector<std::string> &values);

	SettingsType _settingsType = SettingsType::SINGLE;
	Condition _condition = Condition::MATCHES;

	std::vector<std::string> _previousTransform;
	std::vector<std::string> _previousSettingValues;

	static bool _registered;
	static const std::string id;
};

class MacroConditionSceneTransformEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneTransformEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSceneTransform> entryData =
			nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneTransformEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSceneTransform>(
				cond));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void SettingsTypeChanged(int type);
	void CompareChanged(int type);
	void ConditionChanged(int type);
	void SettingSelectionChanged(const TransformSetting &);
	void GetSettingsClicked();
	void GetCurrentValueClicked();
	void TransformChanged();
	void SettingValueChanged();
	void RegexChanged(const RegexConfig &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	void UpdateSettingSelection() const;

	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QComboBox *_settingsType;
	QComboBox *_compare;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	QPushButton *_getCurrentValue;
	VariableTextEdit *_transformString;
	VariableLineEdit *_singleSettingValue;
	RegexConfigWidget *_regex;
	TransformSettingSelection *_settingSelection;
	QHBoxLayout *_compareLayout;

	std::shared_ptr<MacroConditionSceneTransform> _entryData;
	bool _loading = true;
};

} // namespace advss
