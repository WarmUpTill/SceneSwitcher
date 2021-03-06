#include "headers/macro-action-scene-order.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

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

bool MacroActionSceneOrder::PerformAction()
{
	auto s = obs_weak_source_get_source(_scene.GetScene());
	auto scene = obs_scene_from_source(s);
	auto name = GetWeakSourceName(_source);
	auto items = getSceneItemsWithName(scene, name);

	for (auto &i : items) {
		switch (_action) {
		case SceneOrderAction::MOVE_UP:
			obs_sceneitem_set_order(i, OBS_ORDER_MOVE_UP);
			break;
		case SceneOrderAction::MOVE_DOWN:
			obs_sceneitem_set_order(i, OBS_ORDER_MOVE_DOWN);
			break;
		case SceneOrderAction::MOVE_TOP:
			obs_sceneitem_set_order(i, OBS_ORDER_MOVE_TOP);
			break;
		case SceneOrderAction::MOVE_BOTTOM:
			obs_sceneitem_set_order(i, OBS_ORDER_MOVE_BOTTOM);
			break;
		case SceneOrderAction::POSITION:
			obs_sceneitem_set_order_position(i, _position);
			break;
		default:
			break;
		}
		obs_sceneitem_release(i);
	}

	obs_source_release(s);
	return true;
}

void MacroActionSceneOrder::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed order action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), GetWeakSourceName(_source).c_str(),
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
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "position", _position);
	return true;
}

bool MacroActionSceneOrder::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_action =
		static_cast<SceneOrderAction>(obs_data_get_int(obj, "action"));
	_position = obs_data_get_int(obj, "position");
	return true;
}

std::string MacroActionSceneOrder::GetShortDesc()
{
	if (_source) {
		return _scene.ToString() + " - " + GetWeakSourceName(_source);
	}
	return "";
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
	_scenes = new SceneSelectionWidget(window(), false, false, true);
	_sources = new QComboBox();
	_actions = new QComboBox();
	_position = new QSpinBox();

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
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
	populateSceneItemSelection(_sources, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	_position->setValue(_entryData->_position);
	_position->setVisible(_entryData->_action ==
			      SceneOrderAction::POSITION);
}

void MacroActionSceneOrderEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_scene = s;
	}
	_sources->clear();
	populateSceneItemSelection(_sources, _entryData->_scene);
}

void MacroActionSceneOrderEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
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
