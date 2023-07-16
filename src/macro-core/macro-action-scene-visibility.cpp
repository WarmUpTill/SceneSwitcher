#include "macro-action-scene-visibility.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionSceneVisibility::id = "scene_visibility";

bool MacroActionSceneVisibility::_registered = MacroActionFactory::Register(
	MacroActionSceneVisibility::id,
	{MacroActionSceneVisibility::Create,
	 MacroActionSceneVisibilityEdit::Create,
	 "AdvSceneSwitcher.action.sceneVisibility"});

const static std::map<MacroActionSceneVisibility::Action, std::string>
	actionTypes = {
		{MacroActionSceneVisibility::Action::SHOW,
		 "AdvSceneSwitcher.action.sceneVisibility.type.show"},
		{MacroActionSceneVisibility::Action::HIDE,
		 "AdvSceneSwitcher.action.sceneVisibility.type.hide"},
		{MacroActionSceneVisibility::Action::TOGGLE,
		 "AdvSceneSwitcher.action.sceneVisibility.type.toggle"},
};

static void setSceneItemVisibility(obs_sceneitem_t *item,
				   MacroActionSceneVisibility::Action action)
{
	switch (action) {
	case MacroActionSceneVisibility::Action::SHOW:
		obs_sceneitem_set_visible(item, true);
		break;
	case MacroActionSceneVisibility::Action::HIDE:
		obs_sceneitem_set_visible(item, false);
		break;
	case MacroActionSceneVisibility::Action::TOGGLE:
		obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
		break;
	}
}

bool MacroActionSceneVisibility::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);
	for (const auto &item : items) {
		setSceneItemVisibility(item, _action);
	}
	return true;
}

void MacroActionSceneVisibility::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed visibility action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), _source.ToString(true).c_str(),
		      _scene.ToString(true).c_str());

	} else {
		blog(LOG_WARNING, "ignored unknown SceneVisibility action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneVisibility::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionSceneVisibility::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_action = static_cast<MacroActionSceneVisibility::Action>(
		obs_data_get_int(obj, "action"));

	// TODO: Remove in future version
	if (obs_data_get_int(obj, "sourceType") != 0) {
		auto sourceGroup = obs_data_get_string(obj, "sourceGroup");
		_source.SetSourceTypeSelection(sourceGroup);
	}
	return true;
}

std::string MacroActionSceneVisibility::GetShortDesc() const
{
	if (!_source.ToString().empty()) {
		return _scene.ToString() + " - " + _source.ToString();
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneVisibilityEdit::MacroActionSceneVisibilityEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneVisibility> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, false, true, true)),
	  _sources(new SceneItemSelectionWidget(parent)),
	  _actions(new QComboBox())
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.sceneVisibility.entry"),
		     layout,
		     {{"{{scenes}}", _scenes},
		      {"{{sources}}", _sources},
		      {"{{actions}}", _actions}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneVisibilityEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem((_entryData->_source));
}

void MacroActionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = s;
}

void MacroActionSceneVisibilityEdit::SourceChanged(
	const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionSceneVisibilityEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action =
		static_cast<MacroActionSceneVisibility::Action>(value);
}

} // namespace advss
