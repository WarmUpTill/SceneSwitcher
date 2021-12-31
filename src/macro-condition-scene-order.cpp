#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene-order.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionSceneOrder::id = "scene_order";

bool MacroConditionSceneOrder::_registered = MacroConditionFactory::Register(
	MacroConditionSceneOrder::id,
	{MacroConditionSceneOrder::Create, MacroConditionSceneOrderEdit::Create,
	 "AdvSceneSwitcher.condition.sceneOrder"});

static std::map<SceneOrderCondition, std::string> sceneOrderConditionTypes = {
	{SceneOrderCondition::ABOVE,
	 "AdvSceneSwitcher.condition.sceneOrder.type.above"},
	{SceneOrderCondition::BELOW,
	 "AdvSceneSwitcher.condition.sceneOrder.type.below"},
	{SceneOrderCondition::POSITION,
	 "AdvSceneSwitcher.condition.sceneOrder.type.position"},
};

struct PosInfo2 {
	obs_scene_item *item;
	int pos = -1;
	int curPos = 0;
};

static bool getSceneItemPositionHelper(obs_scene_t *, obs_sceneitem_t *item,
				       void *ptr)
{
	PosInfo2 *posInfo = reinterpret_cast<PosInfo2 *>(ptr);
	if (posInfo->item == item) {
		posInfo->pos = posInfo->curPos;
		return false;
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemPositionHelper, ptr);
	}
	posInfo->curPos += 1;
	return true;
}

PosInfo2 getSceneItemPos(obs_scene_item *item, obs_scene *scene)
{
	PosInfo2 pos{item};
	obs_scene_enum_items(scene, getSceneItemPositionHelper, &pos);
	return pos;
}

std::vector<int> getSceneItemPositions(std::vector<obs_scene_item *> &items,
				       obs_scene *scene)
{
	std::vector<int> positions;
	for (auto item : items) {
		auto pos = getSceneItemPos(item, scene);
		if (pos.pos != -1) {
			positions.emplace_back(pos.pos);
		}
	}
	return positions;
}

static bool isAbove(std::vector<int> &pos1, std::vector<int> &pos2)
{
	if (pos1.empty() || pos2.empty()) {
		return false;
	}
	for (int i : pos1) {
		for (int j : pos2) {
			if (i <= j) {
				return false;
			}
		}
	}
	return true;
}

static bool isBelow(std::vector<int> &pos1, std::vector<int> &pos2)
{
	if (pos1.empty() || pos2.empty()) {
		return false;
	}
	for (int i : pos1) {
		for (int j : pos2) {
			if (i >= j) {
				return false;
			}
		}
	}
	return true;
}

bool MacroConditionSceneOrder::CheckCondition()
{
	auto items1 = _source.GetSceneItems(_scene);
	if (items1.empty()) {
		return false;
	}

	auto items2 = _source2.GetSceneItems(_scene);
	auto s = obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto positions1 = getSceneItemPositions(items1, scene);
	auto positions2 = getSceneItemPositions(items2, scene);

	for (auto i : items1) {
		obs_sceneitem_release(i);
	}
	for (auto i : items2) {
		obs_sceneitem_release(i);
	}

	bool ret = false;

	switch (_condition) {
	case SceneOrderCondition::ABOVE:
		ret = isAbove(positions1, positions2);
		break;
	case SceneOrderCondition::BELOW:
		ret = isBelow(positions1, positions2);
		break;
	case SceneOrderCondition::POSITION:
		for (int p : positions1) {
			if (p == _position)
				ret = true;
		}
		break;
	default:
		break;
	}

	obs_source_release(s);
	return ret;
}

bool MacroConditionSceneOrder::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_source2.Save(obj, "sceneItem2", "sceneItemTarget2", "sceneItemIdx2");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "position", _position);
	return true;
}

bool MacroConditionSceneOrder::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
		auto sourceName2 = obs_data_get_string(obj, "source2");
		obs_data_set_string(obj, "sceneItem2", sourceName2);
	}

	MacroCondition::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_source2.Load(obj, "sceneItem2", "sceneItemTarget2", "sceneItemIdx2");
	_condition = static_cast<SceneOrderCondition>(
		obs_data_get_int(obj, "condition"));
	_position = obs_data_get_int(obj, "position");
	return true;
}

std::string MacroConditionSceneOrder::GetShortDesc()
{
	if (_source.ToString().empty()) {
		return "";
	}
	std::string header = _scene.ToString() + " - " + _source.ToString();
	if (!_source2.ToString().empty() &&
	    _condition != SceneOrderCondition::POSITION) {
		header += " - " + _source2.ToString();
	}
	return header;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : sceneOrderConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneOrderEdit::MacroConditionSceneOrderEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSceneOrder> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, true);
	_sources = new SceneItemSelectionWidget(parent);
	_sources2 = new SceneItemSelectionWidget(parent);
	_conditions = new QComboBox();
	_position = new QSpinBox();
	_posInfo = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.sceneOrder.positionInfo"));

	populateConditionSelection(_conditions);
	if (entryData.get()) {
		if (entryData->_condition == SceneOrderCondition::POSITION) {
			_sources->SetShowAllSelectionType(
				SceneItemSelectionWidget::AllSelectionType::ANY);
		} else {
			_sources->SetShowAllSelectionType(
				SceneItemSelectionWidget::AllSelectionType::ALL);
		}
	}

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
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_position, SIGNAL(valueChanged(int)), this,
			 SLOT(PositionChanged(int)));

	auto entryLayout = new QHBoxLayout();
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},     {"{{sources}}", _sources},
		{"{{sources2}}", _sources2}, {"{{conditions}}", _conditions},
		{"{{position}}", _position},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.sceneOrder.entry"),
		entryLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_posInfo);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneOrderEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroConditionSceneOrderEdit::SourceChanged(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::Source2Changed(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source2 = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_condition =
			static_cast<SceneOrderCondition>(index);
	}
	SetWidgetVisibility(_entryData->_condition ==
			    SceneOrderCondition::POSITION);
	if (_entryData->_condition == SceneOrderCondition::POSITION) {
		_sources->SetShowAllSelectionType(
			SceneItemSelectionWidget::AllSelectionType::ANY);
	} else {
		_sources->SetShowAllSelectionType(
			SceneItemSelectionWidget::AllSelectionType::ALL);
	}

	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::PositionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_position = value;
}

void MacroConditionSceneOrderEdit::SetWidgetVisibility(bool showPos)
{
	_sources2->setVisible(!showPos);
	_position->setVisible(showPos);
	_posInfo->setVisible(showPos);
	adjustSize();
}

void MacroConditionSceneOrderEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem(_entryData->_source);
	_sources2->SetSceneItem(_entryData->_source2);
	_position->setValue(_entryData->_position);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	SetWidgetVisibility(_entryData->_condition ==
			    SceneOrderCondition::POSITION);
}
