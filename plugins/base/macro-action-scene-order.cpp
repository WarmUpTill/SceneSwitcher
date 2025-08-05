#include "macro-action-scene-order.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionSceneOrder::id = "scene_order";

bool MacroActionSceneOrder::_registered = MacroActionFactory::Register(
	MacroActionSceneOrder::id,
	{MacroActionSceneOrder::Create, MacroActionSceneOrderEdit::Create,
	 "AdvSceneSwitcher.action.sceneOrder"});

const static std::map<MacroActionSceneOrder::Action, std::string> actionTypes = {
	{MacroActionSceneOrder::Action::MOVE_UP,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveUp"},
	{MacroActionSceneOrder::Action::MOVE_DOWN,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveDown"},
	{MacroActionSceneOrder::Action::MOVE_TOP,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveTop"},
	{MacroActionSceneOrder::Action::MOVE_BOTTOM,
	 "AdvSceneSwitcher.action.sceneOrder.type.moveBottom"},
	{MacroActionSceneOrder::Action::POSITION,
	 "AdvSceneSwitcher.action.sceneOrder.type.movePosition"},
	{MacroActionSceneOrder::Action::ABOVE,
	 "AdvSceneSwitcher.action.sceneOrder.type.above"},
	{MacroActionSceneOrder::Action::BELOW,
	 "AdvSceneSwitcher.action.sceneOrder.type.below"},
};

static void moveSceneItemsUp(std::vector<OBSSceneItem> &items)
{
	// In the case of the same source being in two sequential positions
	// moving the sources up will cause the sources to swap positions due to
	// the order scene items are being iterated over, which is most likely
	// not the desired effect.
	// Instead we reverse the order of the scene items so all scene items
	// will be moved up.
	std::reverse(items.begin(), items.end());

	for (const auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_UP);
	}
}

static void moveSceneItemsDown(std::vector<OBSSceneItem> &items)
{
	for (const auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_DOWN);
	}
}

static void moveSceneItemsTop(std::vector<OBSSceneItem> &items)
{
	for (const auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_TOP);
	}
}

static void moveSceneItemsBottom(std::vector<OBSSceneItem> &items)
{
	for (const auto &i : items) {
		obs_sceneitem_set_order(i, OBS_ORDER_MOVE_BOTTOM);
	}
}

static void moveSceneItemsPos(std::vector<OBSSceneItem> &items, int pos)
{
	for (const auto &i : items) {
		obs_sceneitem_set_order_position(i, pos);
	}
}

namespace {

struct PositionData {
	obs_scene_item *item = nullptr;
	bool found = false;
	int position = 0;
};

} // namespace

static bool getSceneItemPositionHelper(obs_scene_t *, obs_sceneitem_t *item,
				       void *data)
{
	auto positionData = reinterpret_cast<PositionData *>(data);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemPositionHelper, data);
	}
	if (positionData->item == item) {
		positionData->found = true;
		return false;
	}

	positionData->position += 1;
	return true;
}

static std::optional<int> getSceneItemPosition(const OBSSceneItem &item,
					       const SceneSelection &scene)
{
	auto sceneSource = OBSGetStrongRef(scene.GetScene());
	auto obsScene = obs_scene_from_source(sceneSource);
	PositionData data{item};
	obs_scene_enum_items(obsScene, getSceneItemPositionHelper, &data);

	if (!data.found) {
		return {};
	}

	return data.position;
}

static void moveItemFromToHelper(MacroActionSceneOrder::Action action,
				 const OBSSceneItem &itemToMove,
				 int currentPosition, int targetPosition)
{
	if (action == MacroActionSceneOrder::Action::ABOVE) {
		if (currentPosition > targetPosition) {
			obs_sceneitem_set_order_position(itemToMove,
							 targetPosition + 1);
		} else {
			obs_sceneitem_set_order_position(itemToMove,
							 targetPosition);
		}
	} else if (action == MacroActionSceneOrder::Action::BELOW) {
		if (currentPosition > targetPosition) {
			obs_sceneitem_set_order_position(itemToMove,
							 targetPosition);
		} else {
			obs_sceneitem_set_order_position(itemToMove,
							 targetPosition - 1);
		}
	}
}

static void moveItemToItemHelper(MacroActionSceneOrder::Action action,
				 const std::vector<OBSSceneItem> &itemsToMove,
				 const SceneItemSelection &target,
				 const SceneSelection &scene)
{
	auto targetItems = target.GetSceneItems(scene);
	if (targetItems.empty()) {
		return;
	}

	auto targetItem = targetItems.at(0);

	for (const auto &item : itemsToMove) {
		if (item == targetItem) {
			continue;
		}

		auto targetPosition = getSceneItemPosition(targetItem, scene);
		if (!targetPosition) {
			continue;
		}

		auto currentPosition = getSceneItemPosition(item, scene);
		if (!currentPosition) {
			continue;
		}

		moveItemFromToHelper(action, item, *currentPosition,
				     *targetPosition);
	}
}

bool MacroActionSceneOrder::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);

	switch (_action) {
	case Action::MOVE_UP:
		moveSceneItemsUp(items);
		break;
	case Action::MOVE_DOWN:
		moveSceneItemsDown(items);
		break;
	case Action::MOVE_TOP:
		moveSceneItemsTop(items);
		break;
	case Action::MOVE_BOTTOM:
		moveSceneItemsBottom(items);
		break;
	case Action::POSITION:
		moveSceneItemsPos(items, _position);
		break;
	case Action::ABOVE:
	case Action::BELOW: {
		moveItemToItemHelper(_action, items, _source2, _scene);
		break;
	}
	default:
		break;
	}
	return true;
}

void MacroActionSceneOrder::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO,
		      "performed order action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), _source.ToString(true).c_str(),
		      _scene.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown scene order action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneOrder::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_source2.Save(obj, "sceneItemSelection2");
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
	_source2.Load(obj, "sceneItemSelection2");
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_position = obs_data_get_int(obj, "position");
	return true;
}

std::string MacroActionSceneOrder::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}
	return _scene.ToString() + " - " + _source.ToString();
}

std::shared_ptr<MacroAction> MacroActionSceneOrder::Create(Macro *m)
{
	return std::make_shared<MacroActionSceneOrder>(m);
}

std::shared_ptr<MacroAction> MacroActionSceneOrder::Copy() const
{
	return std::make_shared<MacroActionSceneOrder>(*this);
}

void MacroActionSceneOrder::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_source.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneOrderEdit::MacroActionSceneOrderEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneOrder> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, false, true)),
	  _sources(new SceneItemSelectionWidget(
		  this,
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
	  _sources2(new SceneItemSelectionWidget(
		  this,
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
	  _actions(new QComboBox(this)),
	  _position(new QSpinBox(this))
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources2, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_sources2,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this,
			 SLOT(Source2Changed(const SceneItemSelection &)));
	QWidget::connect(_position, SIGNAL(valueChanged(int)), this,
			 SLOT(PositionChanged(int)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.sceneOrder.entry"),
		layout,
		{{"{{scenes}}", _scenes},
		 {"{{sources}}", _sources},
		 {"{{sources2}}", _sources2},
		 {"{{actions}}", _actions},
		 {"{{position}}", _position}});
	setLayout(layout);

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
	_sources2->SetSceneItem(_entryData->_source2);
	_position->setValue(_entryData->_position);
	SetWidgetVisibility();
}

void MacroActionSceneOrderEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
}

void MacroActionSceneOrderEdit::SourceChanged(const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionSceneOrderEdit::Source2Changed(const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source2 = item;
	adjustSize();
	updateGeometry();
}

void MacroActionSceneOrderEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSceneOrder::Action>(value);
	SetWidgetVisibility();
}

void MacroActionSceneOrderEdit::PositionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_position = value;
}

void MacroActionSceneOrderEdit::SetWidgetVisibility()
{
	_position->setVisible(_entryData->_action ==
			      MacroActionSceneOrder::Action::POSITION);
	_sources2->setVisible(
		_entryData->_action == MacroActionSceneOrder::Action::ABOVE ||
		_entryData->_action == MacroActionSceneOrder::Action::BELOW);
	adjustSize();
	updateGeometry();
}

} // namespace advss
