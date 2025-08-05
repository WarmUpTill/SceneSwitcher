#include "macro-condition-scene-order.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroConditionSceneOrder::id = "scene_order";

bool MacroConditionSceneOrder::_registered = MacroConditionFactory::Register(
	MacroConditionSceneOrder::id,
	{MacroConditionSceneOrder::Create, MacroConditionSceneOrderEdit::Create,
	 "AdvSceneSwitcher.condition.sceneOrder"});

const static std::map<MacroConditionSceneOrder::Condition, std::string>
	sceneOrderConditionTypes = {
		{MacroConditionSceneOrder::Condition::ABOVE,
		 "AdvSceneSwitcher.condition.sceneOrder.type.above"},
		{MacroConditionSceneOrder::Condition::BELOW,
		 "AdvSceneSwitcher.condition.sceneOrder.type.below"},
		{MacroConditionSceneOrder::Condition::POSITION,
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
	auto posInfo = reinterpret_cast<PosInfo2 *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemPositionHelper, ptr);
	}
	if (posInfo->item == item) {
		posInfo->pos = posInfo->curPos;
		return false;
	}

	posInfo->curPos += 1;
	return true;
}

static PosInfo2 getSceneItemPos(obs_scene_item *item, obs_scene *scene)
{
	PosInfo2 pos{item};
	obs_scene_enum_items(scene, getSceneItemPositionHelper, &pos);
	return pos;
}

static std::vector<int> getSceneItemPositions(std::vector<OBSSceneItem> &items,
					      obs_scene *scene)
{
	std::vector<int> positions;
	for (const auto &item : items) {
		auto pos = getSceneItemPos(item, scene);
		if (pos.pos != -1) {
			positions.emplace_back(pos.pos);
		}
	}
	return positions;
}

static bool anyAboveAny(const std::vector<int> &a, const std::vector<int> &b)
{
	for (int ai : a)
		for (int bi : b)
			if (ai > bi)
				return true;
	return false;
}

static bool anyAboveAll(const std::vector<int> &a, const std::vector<int> &b)
{
	if (b.empty())
		return false;
	int max_b = *std::max_element(b.begin(), b.end());
	for (int ai : a)
		if (ai > max_b)
			return true;
	return false;
}

static bool allAboveAny(const std::vector<int> &a, const std::vector<int> &b)
{
	for (int ai : a) {
		bool found = false;
		for (int bi : b) {
			if (ai > bi) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

static bool allAboveAll(const std::vector<int> &a, const std::vector<int> &b)
{
	if (b.empty())
		return false;
	int max_b = *std::max_element(b.begin(), b.end());
	for (int ai : a)
		if (ai <= max_b)
			return false;
	return true;
}

static bool anyBelowAny(const std::vector<int> &a, const std::vector<int> &b)
{
	for (int ai : a)
		for (int bi : b)
			if (ai < bi)
				return true;
	return false;
}

static bool anyBelowAll(const std::vector<int> &a, const std::vector<int> &b)
{
	if (b.empty())
		return false;
	int min_b = *std::min_element(b.begin(), b.end());
	for (int ai : a)
		if (ai < min_b)
			return true;
	return false;
}

static bool allBelowAny(const std::vector<int> &a, const std::vector<int> &b)
{
	for (int ai : a) {
		bool found = false;
		for (int bi : b) {
			if (ai < bi) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

static bool allBelowAll(const std::vector<int> &a, const std::vector<int> &b)
{
	if (b.empty())
		return false;
	int min_b = *std::min_element(b.begin(), b.end());
	for (int ai : a)
		if (ai >= min_b)
			return false;
	return true;
}

bool MacroConditionSceneOrder::CheckCondition()
{
	auto items1 = _source.GetSceneItems(_scene);
	if (items1.empty()) {
		return false;
	}

	auto items2 = _source2.GetSceneItems(_scene);
	auto s = OBSGetStrongRef(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto positions1 = getSceneItemPositions(items1, scene);
	auto positions2 = getSceneItemPositions(items2, scene);

	const bool pos1IsAnyCheck = _source.IsSelectionOfTypeAny();
	const bool pos2IsAnyCheck = _source2.IsSelectionOfTypeAny();

	switch (_condition) {
	case Condition::ABOVE:
		if (pos1IsAnyCheck) {
			if (pos2IsAnyCheck) {
				return anyAboveAny(positions1, positions2);
			} else {
				return anyAboveAll(positions1, positions2);
			}
		} else {
			if (pos2IsAnyCheck) {
				return allAboveAny(positions1, positions2);
			} else {
				return allAboveAll(positions1, positions2);
			}
		}
		break;
	case Condition::BELOW:
		if (pos1IsAnyCheck) {
			if (pos2IsAnyCheck) {
				return anyBelowAny(positions1, positions2);
			} else {
				return anyBelowAll(positions1, positions2);
			}
		} else {
			if (pos2IsAnyCheck) {
				return allBelowAny(positions1, positions2);
			} else {
				return allBelowAll(positions1, positions2);
			}
		}
		break;
	case Condition::POSITION:
		if (pos1IsAnyCheck) {
			for (int p : positions1) {
				if (p == _position) {
					return true;
				}
			}
		} else {
			if (positions1.size() == 1) {
				return positions1[0] == _position;
			} else {
				// Multiple scene items can't have the same pos
				return false;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

bool MacroConditionSceneOrder::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	_source2.Save(obj, "sceneItemSelection2");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "position", _position);
	_position.Save(obj, "position");
	obs_data_set_int(obj, "version", 1);
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
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "sceneItem2")) {
		_source2.Load(obj, "sceneItem2", "sceneItemTarget2",
			      "sceneItemIdx2");
	} else {
		_source2.Load(obj, "sceneItemSelection2");
	}
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	if (!obs_data_has_user_value(obj, "version")) {
		_position = obs_data_get_int(obj, "position");
	} else {
		_position.Load(obj, "position");
	}
	return true;
}

std::string MacroConditionSceneOrder::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}
	std::string header = _scene.ToString() + " - " + _source.ToString();
	if (!_source2.ToString().empty() &&
	    _condition != MacroConditionSceneOrder::Condition::POSITION) {
		header += " - " + _source2.ToString();
	}
	return header;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &entry : sceneOrderConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneOrderEdit::MacroConditionSceneOrderEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSceneOrder> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, false, true)),
	  _conditions(new QComboBox()),
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
			  SceneItemSelection::Type::ANY,
		  },
		  SceneItemSelectionWidget::NameClashMode::ANY_AND_ALL)),
	  _sources2(new SceneItemSelectionWidget(
		  parent,
		  {
			  SceneItemSelection::Type::SOURCE_NAME,
			  SceneItemSelection::Type::VARIABLE_NAME,
			  SceneItemSelection::Type::SOURCE_NAME_PATTERN,
			  SceneItemSelection::Type::SOURCE_GROUP,
			  SceneItemSelection::Type::SOURCE_TYPE,
			  SceneItemSelection::Type::INDEX,
			  SceneItemSelection::Type::INDEX_RANGE,
			  SceneItemSelection::Type::ANY,
		  },
		  SceneItemSelectionWidget::NameClashMode::ANY_AND_ALL)),
	  _position(new VariableSpinBox()),
	  _posInfo(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.sceneOrder.positionInfo")))
{
	populateConditionSelection(_conditions);

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
	QWidget::connect(
		_position,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(PositionChanged(const NumberVariable<int> &)));

	auto entryLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.sceneOrder.entry"),
		entryLayout,
		{{"{{scenes}}", _scenes},
		 {"{{sources}}", _sources},
		 {"{{sources2}}", _sources2},
		 {"{{conditions}}", _conditions},
		 {"{{position}}", _position}});
	auto layout = new QVBoxLayout;
	layout->addLayout(entryLayout);
	layout->addWidget(_posInfo);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneOrderEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
}

void MacroConditionSceneOrderEdit::SourceChanged(const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroConditionSceneOrderEdit::Source2Changed(const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source2 = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroConditionSceneOrderEdit::ConditionChanged(int index)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->_condition =
			static_cast<MacroConditionSceneOrder::Condition>(index);
	}
	SetWidgetVisibility(_entryData->_condition ==
			    MacroConditionSceneOrder::Condition::POSITION);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::PositionChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
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
	_position->SetValue(_entryData->_position);
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	SetWidgetVisibility(_entryData->_condition ==
			    MacroConditionSceneOrder::Condition::POSITION);
}

} // namespace advss
