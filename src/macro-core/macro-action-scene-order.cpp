#include "macro-action-scene-order.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionSceneOrder::id = "scene_order";

bool MacroActionSceneOrder::_registered = MacroActionFactory::Register(
	MacroActionSceneOrder::id,
	{MacroActionSceneOrder::Create, MacroActionSceneOrderEdit::Create,
	 "AdvSceneSwitcher.action.sceneOrder"});

const static std::map<SceneOrderAction, std::string> actionTypes = {
	{SceneOrderAction::MOVE_UP,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveUp"},
	{SceneOrderAction::MOVE_DOWN,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveDown"},
	{SceneOrderAction::MOVE_TOP,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveTop"},
	{SceneOrderAction::MOVE_BOTTOM,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveBottom"},
	{SceneOrderAction::POSITION,
	 "AdvSceneSwitcher.action.sceneOrder.type.movePosition"},
};

void moveSceneItemsUp(std::vector<obs_scene_item *> &items)
{
	// In the case of the same source being in two sequential positions
	// moving the sources up will cause the sources to swap positions due to
	// the order scene items are being iterated over, which is most likely
	// not the desired effect.
	// Instead we reverse the order of the scene items so all scene items
	// will be moved up.
	std::reverse(items.begin(), items.end());

	for (auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_UP);
		obs_sceneitem_release(i);
	}
}

void moveSceneItemsDown(std::vector<obs_scene_item *> &items)
{
	for (auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_DOWN);
		obs_sceneitem_release(i);
	}
}

void moveSceneItemsTop(std::vector<obs_scene_item *> &items)
{
	for (auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_TOP);
		obs_sceneitem_release(i);
	}
}

void moveSceneItemsBottom(std::vector<obs_scene_item *> &items)
{
	for (auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_BOTTOM);
		obs_sceneitem_release(i);
	}
}

void moveSceneItemsPos(std::vector<obs_scene_item *> &items, int pos)
{
	for (auto &i : items) {
		obs_sceneitem_set_order_position(i, pos);
		obs_sceneitem_release(i);
	}
}

bool MacroActionSceneOrder::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);

	switch (_action) {
	case SceneOrderAction::MOVE_UP:
		moveSceneItemsUp(items);
		break;
	case SceneOrderAction::MOVE_DOWN:
		moveSceneItemsDown(items);
		break;
	case SceneOrderAction::MOVE_TOP:
		moveSceneItemsTop(items);
		break;
	case SceneOrderAction::MOVE_BOTTOM:
		moveSceneItemsBottom(items);
		break;
	case SceneOrderAction::POSITION:
		moveSceneItemsPos(items, _position);
		break;
	default:
		break;
	}
	return true;
}

void MacroActionSceneOrder::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed order action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), _source.ToString().c_str(),
		      _scene.ToString().c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown scene order action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneOrder::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "position", _position);
	return true;
}

bool MacroActionSceneOrder::Load(obs_data_t *obj)
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
	_action =
		static_cast<SceneOrderAction>(obs_data_get_int(obj, "action"));
	_position = obs_data_get_int(obj, "position");
	return true;
}

std::string MacroActionSceneOrder::GetShortDesc()
{
	if (_source.ToString().empty()) {
		return "";
	}
	return _scene.ToString() + " - " + _source.ToString();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneOrderEdit::MacroActionSceneOrderEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneOrder> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), true, false, false, true);
	_sources = new SceneItemSelectionWidget(parent);
	_actions = new QComboBox();
	_position = new QSpinBox();

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
	QWidget::connect(_position, SIGNAL(valueChanged(int)), this,
			 SLOT(PositionChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
		{"{{position}}", _position},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.sceneOrder.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneOrderEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem(_entryData->_source);
	_position->setValue(_entryData->_position);
	_position->setVisible(_entryData->_action ==
			      SceneOrderAction::POSITION);
}

void MacroActionSceneOrderEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroActionSceneOrderEdit::SourceChanged(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSceneOrderEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<SceneOrderAction>(value);
	_position->setVisible(_entryData->_action ==
			      SceneOrderAction::POSITION);
}

void MacroActionSceneOrderEdit::PositionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_position = value;
}
