#include "macro-action-variable-scene.hpp"
#include "scene-switch-helpers.hpp"
#include "source-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{60,
	 {
		 {Action::CURRENT_SCENE,
		  "AdvSceneSwitcher.action.variable.type.currentScene"},
		 {Action::PREVIOUS_SCENE,
		  "AdvSceneSwitcher.action.variable.type.previousScene"},
		 {Action::PREVIEW_SCENE,
		  "AdvSceneSwitcher.action.variable.type.previewScene"},
		 {Action::CURRENT_TRANSITION,
		  "AdvSceneSwitcher.action.variable.type.currentTransition"},
		 {Action::SCENE_ITEM_NAME,
		  "AdvSceneSwitcher.action.variable.type.sceneItemName",
		  "AdvSceneSwitcher.action.variable.layout.sceneItemName"},
		 {Action::SCENE_ITEM_COUNT,
		  "AdvSceneSwitcher.action.variable.type.sceneItemCount",
		  "AdvSceneSwitcher.action.variable.layout.sceneItemCount"},
	 },
	 []() { return std::make_unique<VariableActionScene>(); },
	 VariableActionSceneEdit::Create});

struct GetSceneItemNameHelper {
	int curIdx = 0;
	int targetIdx = 0;
	std::string name = "";
};

static bool getSceneItemAtIdx(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<GetSceneItemNameHelper *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemAtIdx, ptr);
	}
	if (data->curIdx == data->targetIdx) {
		data->name =
			obs_source_get_name(obs_sceneitem_get_source(item));
		data->curIdx--;
		return false;
	}
	data->curIdx--;
	return true;
}

std::unique_ptr<VariableActionGroup> VariableActionScene::Copy() const
{
	return std::make_unique<VariableActionScene>(*this);
}

void VariableActionScene::Save(obs_data_t *obj,
			       const MacroActionVariable &) const
{
	_scene.Save(obj);
	_sceneItemIndex.Save(obj, "sceneItemIndex");
}

void VariableActionScene::Load(obs_data_t *obj, MacroActionVariable &)
{
	_scene.Load(obj);
	_sceneItemIndex.Load(obj, "sceneItemIndex");
}

void VariableActionScene::ResolveVariables()
{
	_scene.ResolveVariables();
	_sceneItemIndex.ResolveVariables();
}

void VariableActionScene::SetToSceneItemName(Variable &var)
{
	auto weakSource = _scene.GetScene();
	if (!weakSource) {
		var.SetValue("");
		return;
	}
	auto index = _sceneItemIndex.GetValue();
	if (index < 1) {
		var.SetValue("");
		return;
	}
	auto sceneItemCount = GetSceneItemCount(weakSource);
	OBSSourceAutoRelease source = obs_weak_source_get_source(weakSource);
	auto scene = obs_scene_from_source(source);
	GetSceneItemNameHelper data{sceneItemCount, index};
	obs_scene_enum_items(scene, getSceneItemAtIdx, &data);
	var.SetValue(data.name);
}

bool VariableActionScene::Perform(MacroActionVariable &,
				  MacroActionVariable::Action type,
				  Variable &var)
{
	switch (type) {
	case Action::SCENE_ITEM_NAME:
		SetToSceneItemName(var);
		break;
	case Action::SCENE_ITEM_COUNT:
		var.SetValue(GetSceneItemCount(_scene.GetScene(false)));
		break;
	case Action::CURRENT_SCENE:
		var.SetValue(GetWeakSourceName(GetCurrentScene()));
		break;
	case Action::PREVIOUS_SCENE:
		var.SetValue(GetWeakSourceName(GetPreviousScene()));
		break;
	case Action::PREVIEW_SCENE: {
		// Only available in studio mode
		OBSSourceAutoRelease scene =
			obs_frontend_get_current_preview_scene();
		var.SetValue(scene ? obs_source_get_name(scene) : "");
		break;
	}
	case Action::CURRENT_TRANSITION: {
		OBSSourceAutoRelease transition =
			obs_frontend_get_current_transition();
		var.SetValue(transition ? obs_source_get_name(transition) : "");
		break;
	}
	default:
		break;
	}
	return true;
}

VariableActionSceneEdit::VariableActionSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionScene>()
			  : nullptr),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sceneItemIndex(new VariableSpinBox(this))
{
	_sceneItemIndex->setMinimum(1);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(
		_sceneItemIndex,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SceneItemIndexChanged(const NumberVariable<int> &)));
}

VariableActionGroupEdit *
VariableActionSceneEdit::Create(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionSceneEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionSceneEdit::GetInlineWidgets()
{
	return {
		{"{{scenes}}", _scenes},
		{"{{sceneItemIndex}}", _sceneItemIndex},
	};
}

void VariableActionSceneEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_scenes->SetScene(_data->_scene);
	_sceneItemIndex->SetValue(_data->_sceneItemIndex);
	_loading = false;
}

void VariableActionSceneEdit::SetWidgetVisibility(MacroActionVariable::Action)
{
	hide();
}

void VariableActionSceneEdit::SceneChanged(const SceneSelection &scene)
{
	GUARD_LOADING_AND_LOCK();
	_data->_scene = scene;
}

void VariableActionSceneEdit::SceneItemIndexChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_sceneItemIndex = value;
}

} // namespace advss
