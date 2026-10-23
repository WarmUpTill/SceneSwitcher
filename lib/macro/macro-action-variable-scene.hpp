#pragma once
#include "macro-action-variable-group.hpp"
#include "scene-selection.hpp"
#include "variable-spinbox.hpp"

namespace advss {

class VariableActionScene : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	SceneSelection _scene;
	IntVariable _sceneItemIndex = 1;

private:
	void SetToSceneItemName(Variable &);
};

class VariableActionSceneEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionSceneEdit(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void SceneChanged(const SceneSelection &);
	void SceneItemIndexChanged(const NumberVariable<int> &);

private:
	VariableActionScene *_data;

	SceneSelectionWidget *_scenes;
	VariableSpinBox *_sceneItemIndex;
};

} // namespace advss
