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

struct PosInfo {
	std::string name;
	std::vector<int> pos = {};
	int curPos = 0;
};

static bool getSceneItemPositions(obs_scene_t *, obs_sceneitem_t *item,
				  void *ptr)
{
	PosInfo *posInfo = reinterpret_cast<PosInfo *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (posInfo->name == sourceName) {
		posInfo->pos.push_back(posInfo->curPos);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemPositions, ptr);
	}
	posInfo->curPos += 1;
	return true;
}

static bool isAbove(std::vector<int> &v1, std::vector<int> &v2)
{
	for (int i : v1) {
		for (int j : v2) {
			if (i > j) {
				return true;
			}
		}
	}
	return false;
}

static bool isBelow(std::vector<int> &v1, std::vector<int> &v2)
{
	for (int i : v1) {
		for (int j : v2) {
			if (i < j) {
				return true;
			}
		}
	}
	return false;
}

bool MacroConditionSceneOrder::CheckCondition()
{
	if (!_source) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto name1 = GetWeakSourceName(_source);
	auto name2 = GetWeakSourceName(_source2);
	PosInfo pos1 = {name1};
	PosInfo pos2 = {name2};
	obs_scene_enum_items(scene, getSceneItemPositions, &pos1);
	obs_scene_enum_items(scene, getSceneItemPositions, &pos2);

	switch (_condition) {
	case SceneOrderCondition::ABOVE:
		ret = isAbove(pos1.pos, pos2.pos);
		break;
	case SceneOrderCondition::BELOW:
		ret = isBelow(pos1.pos, pos2.pos);
		break;
	case SceneOrderCondition::POSITION:
		for (int p : pos1.pos) {
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
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_string(obj, "source2",
			    GetWeakSourceName(_source2).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "position", _position);
	return true;
}

bool MacroConditionSceneOrder::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	const char *source2Name = obs_data_get_string(obj, "source2");
	_source2 = GetWeakSourceByName(source2Name);
	_condition = static_cast<SceneOrderCondition>(
		obs_data_get_int(obj, "condition"));
	_position = obs_data_get_int(obj, "position");
	return true;
}

std::string MacroConditionSceneOrder::GetShortDesc()
{
	if (_source) {
		std::string header =
			_scene.ToString() + " - " + GetWeakSourceName(_source);
		if (_source2 && _condition != SceneOrderCondition::POSITION) {
			header += " - " + GetWeakSourceName(_source2);
		}
		return header;
	}
	return "";
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
	_sources = new QComboBox();
	_sources2 = new QComboBox();
	_conditions = new QComboBox();
	_position = new QSpinBox();
	_posInfo = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.sceneOrder.positionInfo"));

	populateConditionSelection(_conditions);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_sources2, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(Source2Changed(const QString &)));
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
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_scene = s;
	}
	_sources->clear();
	_sources2->clear();
	populateSceneItemSelection(_sources, _entryData->_scene);
	populateSceneItemSelection(_sources2, _entryData->_scene);
}

void MacroConditionSceneOrderEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::Source2Changed(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source2 = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneOrderEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<SceneOrderCondition>(index);
	SetWidgetVisibility(_entryData->_condition ==
			    SceneOrderCondition::POSITION);
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
	populateSceneItemSelection(_sources, _entryData->_scene);
	populateSceneItemSelection(_sources2, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	_sources2->setCurrentText(
		GetWeakSourceName(_entryData->_source2).c_str());
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	SetWidgetVisibility(_entryData->_condition ==
			    SceneOrderCondition::POSITION);
}
