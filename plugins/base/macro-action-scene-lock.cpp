#include "macro-action-scene-lock.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionSceneLock::id = "scene_lock";

bool MacroActionSceneLock::_registered = MacroActionFactory::Register(
	MacroActionSceneLock::id,
	{MacroActionSceneLock::Create, MacroActionSceneLockEdit::Create,
	 "AdvSceneSwitcher.action.sceneLock"});

const static std::map<MacroActionSceneLock::Action, std::string> actionTypes = {
	{MacroActionSceneLock::Action::LOCK,
	 "AdvSceneSwitcher.action.sceneLock.type.lock"},
	{MacroActionSceneLock::Action::UNLOCK,
	 "AdvSceneSwitcher.action.sceneLock.type.unlock"},
	{MacroActionSceneLock::Action::TOGGLE,
	 "AdvSceneSwitcher.action.sceneLock.type.toggle"},
};

static void setSceneItemLock(obs_sceneitem_t *item,
			     MacroActionSceneLock::Action action)
{
	switch (action) {
	case MacroActionSceneLock::Action::LOCK:
		obs_sceneitem_set_locked(item, true);
		break;
	case MacroActionSceneLock::Action::UNLOCK:
		obs_sceneitem_set_locked(item, false);
		break;
	case MacroActionSceneLock::Action::TOGGLE:
		obs_sceneitem_set_locked(item, !obs_sceneitem_locked(item));
		break;
	}
}

bool MacroActionSceneLock::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);
	for (const auto &item : items) {
		setSceneItemLock(item, _action);
	}

	return true;
}

void MacroActionSceneLock::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO,
		      "performed action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), _source.ToString(true).c_str(),
		      _scene.ToString(true).c_str());

	} else {
		blog(LOG_WARNING, "ignored unknown scene lock action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneLock::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionSceneLock::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionSceneLock::GetShortDesc() const
{
	if (!_source.ToString().empty()) {
		return _scene.ToString() + " - " + _source.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionSceneLock::Create(Macro *m)
{
	return std::make_shared<MacroActionSceneLock>(m);
}

std::shared_ptr<MacroAction> MacroActionSceneLock::Copy() const
{
	return std::make_shared<MacroActionSceneLock>(*this);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

void MacroActionSceneLock::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_source.ResolveVariables();
}

MacroActionSceneLockEdit::MacroActionSceneLockEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneLock> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true)),
	  _sources(new SceneItemSelectionWidget(
		  parent,
		  {
			  SceneItemSelection::Type::SOURCE_NAME,
			  SceneItemSelection::Type::VARIABLE_NAME,
			  SceneItemSelection::Type::SOURCE_NAME_PATTERN,
			  SceneItemSelection::Type::SOURCE_GROUP,
			  SceneItemSelection::Type::SOURCE_TYPE,
			  SceneItemSelection::Type::INDEX,
			  SceneItemSelection::Type::INDEX_RANGE,
			  SceneItemSelection::Type::ALL,
		  },
		  SceneItemSelectionWidget::NameClashMode::ALL)),
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
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.sceneLock.entry"),
		     layout, widgetPlaceholders);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneLockEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem((_entryData->_source));
}

void MacroActionSceneLockEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
}

void MacroActionSceneLockEdit::SourceChanged(const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionSceneLockEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSceneLock::Action>(value);
}

} // namespace advss
