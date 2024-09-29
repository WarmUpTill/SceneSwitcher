#pragma once
#include "macro-action-edit.hpp"
#include "file-selection.hpp"
#include "scene-selection.hpp"
#include "screenshot-helper.hpp"
#include "source-selection.hpp"
#include "variable.hpp"

#include <QComboBox>

namespace advss {

class MacroActionScreenshot : public MacroAction {
public:
	MacroActionScreenshot(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class SaveType { OBS_DEFAULT_PATH, CUSTOM_PATH, VARIABLE };
	SaveType _saveType = SaveType::OBS_DEFAULT_PATH;

	enum class TargetType { SOURCE, SCENE, MAIN_OUTPUT };
	TargetType _targetType = TargetType::SOURCE;
	SceneSelection _scene;
	SourceSelection _source;
	StringVariable _path = obs_module_text("AdvSceneSwitcher.enterPath");
	std::weak_ptr<Variable> _variable;

private:
	void FrontendScreenshot(OBSWeakSource &) const;
	void CustomScreenshot(OBSWeakSource &) const;
	void VariableScreenshot(OBSWeakSource &) const;

	static bool _registered;
	static const std::string id;
};

class MacroActionScreenshotEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionScreenshotEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionScreenshot> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionScreenshotEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionScreenshot>(
				action));
	}
private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SourceSelection &);
	void SaveTypeChanged(int index);
	void TargetTypeChanged(int index);
	void PathChanged(const QString &text);
	void VariableChanged(const QString &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	SceneSelectionWidget *_scenes;
	SourceSelectionWidget *_sources;
	QComboBox *_saveType;
	QComboBox *_targetType;
	FileSelection *_savePath;
	VariableSelection *_variables;

	std::shared_ptr<MacroActionScreenshot> _entryData;
	bool _loading = true;
};

} // namespace advss
