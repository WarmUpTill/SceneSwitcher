#include "scene-item-selection.hpp"
#include "layout-helpers.hpp"
#include "obs-module-helper.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"

#include <set>
#include <variant>

using NameClashMode = advss::SceneItemSelectionWidget::NameClashMode;
using ConflictIndex = std::variant<int, NameClashMode>;

Q_DECLARE_METATYPE(NameClashMode)
Q_DECLARE_METATYPE(ConflictIndex);

namespace advss {

using NameConflictSelection = SceneItemSelection::NameConflictSelection;

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view itemSaveName = "item";
constexpr std::string_view indexSaveName = "index";
constexpr std::string_view indexEndSaveName = "indexEnd";
constexpr std::string_view nameConflictIndexSaveName = "idx";
constexpr std::string_view nameConflictIndexSelectionSaveName = "idxType";
constexpr std::string_view patternSaveName = "pattern";

/* ------------------------------------------------------------------------- */

struct NameMatchData {
	std::string name;
	std::vector<OBSSceneItem> items = {};
};

static bool getSceneItemsByNameHelper(obs_scene_t *, obs_sceneitem_t *item,
				      void *ptr)
{
	auto itemInfo = reinterpret_cast<NameMatchData *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (itemInfo->name == sourceName) {
		itemInfo->items.emplace_back(item);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemsByNameHelper, ptr);
	}

	return true;
}

static std::vector<OBSSceneItem> getSceneItemsWithName(obs_scene_t *scene,
						       const std::string &name)
{
	NameMatchData itemInfo = {name};
	obs_scene_enum_items(scene, getSceneItemsByNameHelper, &itemInfo);
	return itemInfo.items;
}

struct NamePatternMatchData {
	std::string pattern;
	const RegexConfig &regex;
	std::vector<OBSSceneItem> items = {};
};

static bool getSceneItemsByPatternHelper(obs_scene_t *, obs_sceneitem_t *item,
					 void *ptr)
{
	auto data = reinterpret_cast<NamePatternMatchData *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (data->regex.Matches(sourceName, data->pattern)) {
		data->items.emplace_back(item);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemsByPatternHelper, ptr);
	}

	return true;
}

struct ItemCountData {
	std::string name;
	int count = 0;
};

static bool countSceneItemName(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<ItemCountData *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, countSceneItemName, ptr);
	}
	auto name = obs_source_get_name(obs_sceneitem_get_source(item));
	if (name == data->name) {
		data->count++;
	}
	return true;
}

static int getCountOfSceneItemOccurance(const SceneSelection &s,
					const std::string &name,
					bool enumAllScenes = true)
{
	ItemCountData data{name};
	if (enumAllScenes && (s.GetType() != SceneSelection::Type::SCENE)) {
		auto enumScenes = [](void *param, obs_source_t *source) {
			if (!source) {
				return true;
			}
			auto data = reinterpret_cast<ItemCountData *>(param);
			auto scene = obs_scene_from_source(source);
			obs_scene_enum_items(scene, countSceneItemName, data);
			return true;
		};
		obs_enum_scenes(enumScenes, &data);
	} else {
		auto source = obs_weak_source_get_source(s.GetScene(false));
		auto scene = obs_scene_from_source(source);
		obs_scene_enum_items(scene, countSceneItemName, &data);
		obs_source_release(source);
	}
	return data.count;
}

struct IdxData {
	int idxStart = 0;
	int idxEnd = -1;
	int curIdx = 0;
	std::vector<OBSSceneItem> items = {};
};

static bool getSceneItemAtIdx(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<IdxData *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemAtIdx, ptr);
	}
	if (data->curIdx > data->idxEnd) {
		return false;
	}
	if (data->curIdx >= data->idxStart && data->curIdx <= data->idxEnd) {
		data->items.emplace_back(item);
	}
	data->curIdx++;
	return true;
}

struct GroupData {
	std::string type;
	std::vector<OBSSceneItem> items = {};
};

static bool getSceneItemsOfType(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<GroupData *>(ptr);
	auto sourceTypeName = obs_source_get_display_name(
		obs_source_get_id(obs_sceneitem_get_source(item)));
	if (sourceTypeName && data->type == sourceTypeName) {
		data->items.emplace_back(item);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemsOfType, ptr);
	}

	return true;
}

static bool getAllSceneItems(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto items = reinterpret_cast<std::vector<OBSSceneItem> *>(ptr);
	items->emplace_back(item);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getAllSceneItems, ptr);
	}

	return true;
}

/* ------------------------------------------------------------------------- */

void SceneItemSelection::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	obs_data_set_int(data, nameConflictIndexSelectionSaveName.data(),
			 static_cast<int>(_nameConflictSelectionType));
	if (_nameConflictSelectionType == NameConflictSelection::INDIVIDUAL) {
		obs_data_set_int(data, nameConflictIndexSaveName.data(),
				 _nameConflictSelectionIndex);
	} else {
		obs_data_set_int(data, nameConflictIndexSaveName.data(), 0);
	}

	switch (_type) {
	case SceneItemSelection::Type::SOURCE_NAME:
		obs_data_set_string(data, itemSaveName.data(),
				    GetWeakSourceName(_source).c_str());
		break;
	case SceneItemSelection::Type::VARIABLE_NAME: {
		auto var = _variable.lock();
		if (var) {
			obs_data_set_string(data, itemSaveName.data(),
					    var->Name().c_str());
		}
		break;
	}
	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
		_pattern.Save(data, patternSaveName.data());
		_regex.Save(data);
		break;
	case SceneItemSelection::Type::SOURCE_GROUP:
		obs_data_set_string(data, itemSaveName.data(),
				    GetWeakSourceName(_source).c_str());
		break;
	case SceneItemSelection::Type::SOURCE_TYPE:
		obs_data_set_string(obj, "sourceGroup", _sourceType.c_str());
		break;
	case SceneItemSelection::Type::INDEX:
		_index.Save(data, indexSaveName.data());
		break;
	case SceneItemSelection::Type::INDEX_RANGE:
		_index.Save(data, indexSaveName.data());
		_indexEnd.Save(data, indexEndSaveName.data());
		break;
	default:
		break;
	}

	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

// TODO: Remove in future version
void SceneItemSelection::Load(obs_data_t *obj, const char *name,
			      const char *typeName, const char *idxName)
{
	_type = Type::SOURCE_NAME;
	_nameConflictSelectionType = static_cast<NameConflictSelection>(
		obs_data_get_int(obj, typeName));
	_nameConflictSelectionIndex = obs_data_get_int(obj, idxName);
	auto sceneItemName = obs_data_get_string(obj, name);
	_source = GetWeakSourceByName(sceneItemName);
}

void SceneItemSelection::Load(obs_data_t *obj, const char *name)
{
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, name)) {
		Load(obj, "sceneItem", "sceneItemTarget", "sceneItemIdx");
		return;
	}

	auto data = obs_data_get_obj(obj, name);
	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	_nameConflictSelectionType = static_cast<NameConflictSelection>(
		obs_data_get_int(data,
				 nameConflictIndexSelectionSaveName.data()));
	_nameConflictSelectionIndex =
		obs_data_get_int(data, nameConflictIndexSaveName.data());
	const auto itemName = obs_data_get_string(data, itemSaveName.data());
	switch (_type) {
	case SceneItemSelection::Type::SOURCE_NAME:
		_source = GetWeakSourceByName(itemName);
		break;
	case SceneItemSelection::Type::VARIABLE_NAME:
		_variable = GetWeakVariableByName(itemName);
		break;
	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
		_pattern.Load(data, patternSaveName.data());
		_regex.Load(data);
		_regex.SetEnabled(true); // No reason to disable it
		break;
	case SceneItemSelection::Type::SOURCE_GROUP:
		_source = GetWeakSourceByName(itemName);
		break;
	case SceneItemSelection::Type::SOURCE_TYPE:
		_sourceType = obs_data_get_string(obj, "sourceGroup");
		break;
	case SceneItemSelection::Type::INDEX:
		_index.Load(data, indexSaveName.data());
		break;
	case SceneItemSelection::Type::INDEX_RANGE:
		_index.Load(data, indexSaveName.data());
		_indexEnd.Load(data, indexEndSaveName.data());
		break;
	default:
		break;
	}
	obs_data_release(data);
}

void SceneItemSelection::SetSourceTypeSelection(const char *type)
{
	_type = Type::SOURCE_TYPE;
	_sourceType = type;
}

void SceneItemSelection::ResolveVariables()
{
	_index.ResolveVariables();
	_indexEnd.ResolveVariables();
	_pattern = std::string(_pattern);

	if (_type != Type::VARIABLE_NAME) {
		return;
	}
	_type = Type::SOURCE_NAME;

	auto variable = _variable.lock();
	if (variable) {
		_source = GetWeakSourceByName(variable->Value().c_str());
	} else {
		_source = nullptr;
	}
}

void SceneItemSelection::ReduceBasedOnIndexSelection(
	std::vector<OBSSceneItem> &items) const
{
	if (_nameConflictSelectionType ==
		    SceneItemSelection::NameConflictSelection::ALL ||
	    _nameConflictSelectionType ==
		    SceneItemSelection::NameConflictSelection::ANY) {
		return;
	}
	// Index order starts at the bottom and increases to the top
	// As this might be confusing reverse that order internally
	int idx = items.size() - 1 - _nameConflictSelectionIndex;
	if (idx < 0 || idx >= (int)items.size()) {
		items.clear();
		return;
	}

	items = {items[idx]};
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsByName(
	const SceneSelection &sceneSelection) const
{
	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	OBSSceneAutoRelease scene = obs_scene_from_source(s);
	std::string name;
	if (_type == Type::VARIABLE_NAME) {
		auto var = _variable.lock();
		if (!var) {
			return {};
		}
		name = var->Value();
	} else {
		name = GetWeakSourceName(_source);
	}
	auto items = getSceneItemsWithName(scene, name);
	ReduceBasedOnIndexSelection(items);
	return items;
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsByPattern(
	const SceneSelection &sceneSelection) const
{
	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	NamePatternMatchData data{_pattern, _regex};
	obs_scene_enum_items(scene, getSceneItemsByPatternHelper, &data);
	obs_source_release(s);
	ReduceBasedOnIndexSelection(data.items);
	return data.items;
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsOfGroup() const
{
	OBSSourceAutoRelease group = OBSGetStrongRef(_source);
	obs_scene_t *groupScene = obs_group_from_source(group);
	std::vector<OBSSceneItem> sceneItems;
	obs_scene_enum_items(groupScene, getAllSceneItems, &sceneItems);
	return sceneItems;
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsByType(
	const SceneSelection &sceneSelection) const
{
	if (_sourceType.empty()) {
		return {};
	}

	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	GroupData data{_sourceType};
	obs_scene_enum_items(scene, getSceneItemsOfType, &data);
	obs_source_release(s);
	ReduceBasedOnIndexSelection(data.items);
	return data.items;
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsByIdx(
	const SceneSelection &sceneSelection) const
{
	if (!_index.HasValidValue()) {
		return {};
	}

	auto sceneWeakSource = sceneSelection.GetScene(false);
	int count = GetSceneItemCount(sceneWeakSource);
	if (count == 0) {
		return {};
	}

	// Index order starts at the bottom and increases to the top
	// As this might be confusing reverse that order internally
	//
	// Also subtract 1 as the user facing index starts at 1
	int idx = count - _index.GetValue();
	int idxEnd = _type == Type::INDEX_RANGE ? count - _indexEnd.GetValue()
						: idx;
	if (idx > idxEnd) {
		std::swap(idx, idxEnd);
	}

	IdxData data{idx, idxEnd};
	auto s = obs_weak_source_get_source(sceneWeakSource);
	auto scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, getSceneItemAtIdx, &data);
	obs_source_release(s);
	return data.items;
}

std::vector<OBSSceneItem>
SceneItemSelection::GetAllSceneItems(const SceneSelection &sceneSelection) const
{
	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	std::vector<OBSSceneItem> items;
	obs_scene_enum_items(scene, getAllSceneItems, &items);
	obs_source_release(s);
	return items;
}

SceneItemSelection::NameConflictSelection
SceneItemSelection::GetIndexType() const
{
	return _nameConflictSelectionType;
}

std::vector<OBSSceneItem>
SceneItemSelection::GetSceneItems(const SceneSelection &sceneSelection) const
{
	switch (_type) {
	case Type::SOURCE_NAME:
	case Type::VARIABLE_NAME:
		return GetSceneItemsByName(sceneSelection);
	case Type::SOURCE_NAME_PATTERN:
		return GetSceneItemsByPattern(sceneSelection);
	case Type::SOURCE_GROUP:
		return GetSceneItemsOfGroup();
	case Type::SOURCE_TYPE:
		return GetSceneItemsByType(sceneSelection);
	case Type::INDEX:
	case Type::INDEX_RANGE:
		return GetSceneItemsByIdx(sceneSelection);
	case Type::ALL:
	case Type::ANY:
		return GetAllSceneItems(sceneSelection);
	default:
		break;
	}

	return {};
}

bool SceneItemSelection::IsSelectionOfTypeAny() const
{
	if (_type == Type::ANY) {
		return true;
	}

	if (_type == Type::ALL) {
		return false;
	}

	if (_type == Type::INDEX_RANGE) {
		return false;
	}

	if (_nameConflictSelectionType == NameConflictSelection::ANY) {
		return true;
	}

	if (_nameConflictSelectionType == NameConflictSelection::ALL) {
		return false;
	}

	return false;
}

std::string SceneItemSelection::ToString(bool resolve) const
{
	switch (_type) {
	case Type::SOURCE_NAME:
		return GetWeakSourceName(_source);
	case Type::VARIABLE_NAME: {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		if (resolve) {
			return var->Name() + "[" + var->Value() + "]";
		}
		return var->Name();
	}
	case Type::SOURCE_NAME_PATTERN:
		if (resolve) {
			return std::string(obs_module_text(
				       "AdvSceneSwitcher.sceneItemSelection.type.sourceNamePattern")) +
			       " \"" + std::string(_pattern) + "\"";
		}
		return "";
	case Type::SOURCE_GROUP:
		if (resolve) {
			return std::string(obs_module_text(
				       "AdvSceneSwitcher.sceneItemSelection.type.sourceGroup")) +
			       " \"" + GetWeakSourceName(_source) + "\"";
		}
		return _sourceType;
	case Type::SOURCE_TYPE:
		if (resolve) {
			return std::string(obs_module_text(
				       "AdvSceneSwitcher.sceneItemSelection.type.sourceType")) +
			       " \"" + std::string(_sourceType) + "\"";
		}
		return _sourceType;
	case Type::INDEX:
		if (resolve) {
			return std::string(obs_module_text(
				       "AdvSceneSwitcher.sceneItemSelection.type.index")) +
			       " " + std::to_string(_index);
		}
		return "";
	case Type::INDEX_RANGE:
		if (resolve) {
			return std::string(obs_module_text(
				       "AdvSceneSwitcher.sceneItemSelection.type.indexRange")) +
			       " " + std::to_string(_index) + " - " +
			       std::to_string(_indexEnd);
		}
		return "";
	case Type::ALL:
		return obs_module_text(
			"AdvSceneSwitcher.sceneItemSelection.all");
	default:
		break;
	}

	return "";
}

static bool populateSceneItemSelectionHelper(obs_scene_t *,
					     obs_sceneitem_t *item, void *ptr)
{
	auto names = reinterpret_cast<QStringList *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, populateSceneItemSelectionHelper,
				     ptr);
	}
	auto name = obs_source_get_name(obs_sceneitem_get_source(item));
	names->append(name);
	return true;
}

static QStringList getSceneItemsList(SceneSelection &s)
{
	QStringList names;
	if (s.GetType() != SceneSelection::Type::SCENE) {
		auto enumScenes = [](void *param, obs_source_t *source) {
			if (!source) {
				return true;
			}
			auto names = reinterpret_cast<QStringList *>(param);
			auto scene = obs_scene_from_source(source);
			obs_scene_enum_items(
				scene, populateSceneItemSelectionHelper, names);
			return true;
		};

		obs_enum_scenes(enumScenes, &names);
	} else {
		auto source = obs_weak_source_get_source(s.GetScene(false));
		auto scene = obs_scene_from_source(source);
		obs_scene_enum_items(scene, populateSceneItemSelectionHelper,
				     &names);
		obs_source_release(source);
	}

	names.removeDuplicates();
	names.sort();
	return names;
}

void SceneItemSelectionWidget::PopulateItemSelection()
{
	_sources->clear();
	const QStringList sceneItems = getSceneItemsList(_scene);
	AddSelectionGroup(_sources, sceneItems, false);
	_sources->setCurrentIndex(-1);
}

static inline void populateMessageTypeSelection(
	QComboBox *list,
	const std::vector<SceneItemSelection::Type> &allowedTypes)
{
	const static std::map<SceneItemSelection::Type, std::string> types = {
		{SceneItemSelection::Type::SOURCE_NAME,
		 "AdvSceneSwitcher.sceneItemSelection.type.sourceName"},
		{SceneItemSelection::Type::VARIABLE_NAME,
		 "AdvSceneSwitcher.sceneItemSelection.type.sourceVariable"},
		{SceneItemSelection::Type::SOURCE_NAME_PATTERN,
		 "AdvSceneSwitcher.sceneItemSelection.type.sourceNamePattern"},
		{SceneItemSelection::Type::SOURCE_GROUP,
		 "AdvSceneSwitcher.sceneItemSelection.type.sourceGroup"},
		{SceneItemSelection::Type::SOURCE_TYPE,
		 "AdvSceneSwitcher.sceneItemSelection.type.sourceType"},
		{SceneItemSelection::Type::INDEX,
		 "AdvSceneSwitcher.sceneItemSelection.type.index"},
		{SceneItemSelection::Type::INDEX_RANGE,
		 "AdvSceneSwitcher.sceneItemSelection.type.indexRange"},
		{SceneItemSelection::Type::ALL,
		 "AdvSceneSwitcher.sceneItemSelection.type.all"},
		{SceneItemSelection::Type::ANY,
		 "AdvSceneSwitcher.sceneItemSelection.type.any"},
	};

	for (const auto &type : allowedTypes) {
		const auto it = types.find(type);
		assert(it != types.end());
		const auto &[_, name] = *it;
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

static void
populateTypeList(std::set<QString> &list,
		 std::function<bool(size_t idx, const char **id)> enumFunc)
{
	size_t idx = 0;
	char buffer[512] = {};
	const char **idPtr = (const char **)(&buffer);
	while (enumFunc(idx++, idPtr)) {
		// The audio_line source type is only used OBS internally
		if (strcmp(*idPtr, "audio_line") == 0) {
			continue;
		}
		QString name = obs_source_get_display_name(*idPtr);
		if (name.isEmpty()) {
			list.insert(*idPtr);
		} else {
			list.insert(name);
		}
	}
}

static void populateSourceTypeSelection(QComboBox *list)
{
	std::set<QString> sourceTypes;
	populateTypeList(sourceTypes, obs_enum_source_types);
	std::set<QString> filterTypes;
	populateTypeList(filterTypes, obs_enum_filter_types);
	std::set<QString> transitionTypes;
	populateTypeList(transitionTypes, obs_enum_transition_types);

	for (const auto &name : sourceTypes) {
		if (name.isEmpty()) {
			continue;
		}
		if (filterTypes.find(name) == filterTypes.end() &&
		    transitionTypes.find(name) == transitionTypes.end()) {
			list->addItem(name);
		}
	}

	list->model()->sort(0);
	AddSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

static void populateSourceGroupSelection(QComboBox *list)
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		if (obs_source_is_group(source)) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList names;
	obs_enum_sources(sourceEnum, &names);

	for (const auto &name : names) {
		if (name.isEmpty()) {
			continue;
		}
		list->addItem(name);
	}

	list->model()->sort(0);
	AddSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

SceneItemSelectionWidget::SceneItemSelectionWidget(
	QWidget *parent,
	const std::vector<SceneItemSelection::Type> &selections,
	NameClashMode mode)
	: QWidget(parent),
	  _controlsLayout(new QHBoxLayout),
	  _sources(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectItem"))),
	  _sourceGroups(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectItem"))),
	  _variables(new VariableSelection(this)),
	  _nameConflictIndex(new QComboBox(this)),
	  _index(new VariableSpinBox(this)),
	  _indexEnd(new VariableSpinBox(this)),
	  _sourceTypes(new QComboBox(this)),
	  _pattern(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(this, false)),
	  _changeType(new QPushButton(this)),
	  _nameClashMode(mode),
	  _selectionTypes(selections)
{
	static bool setupDone = false;
	if (setupDone) {
		setupDone = true;
		qRegisterMetaType<NameClashMode>("NameClashMode");
		qRegisterMetaType<ConflictIndex>("ConflictIndex");
	}

	_sources->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_sourceGroups->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_nameConflictIndex->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	_changeType->setMaximumWidth(22);
	SetButtonIcon(_changeType,
		      GetThemeTypeName() == "Light"
			      ? ":/settings/images/settings/general.svg"
			      : "theme:Dark/settings/general.svg");
	_changeType->setFlat(true);
	_changeType->setToolTip(obs_module_text(
		"AdvSceneSwitcher.sceneItemSelection.configure"));

	_index->setMinimum(1);
	_index->setMaximum(999);
	_index->setSuffix(".");
	_indexEnd->setMinimum(1);
	_indexEnd->setMaximum(999);
	_indexEnd->setSuffix(".");

	populateSourceGroupSelection(_sourceGroups);
	populateSourceTypeSelection(_sourceTypes);

	QWidget::connect(_sources, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceChanged(int)));
	QWidget::connect(_sourceGroups, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceGroupChanged(int)));
	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_nameConflictIndex, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(NameConflictIndexChanged(int)));
	QWidget::connect(_sourceTypes,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceTypeChanged(const QString &)));
	QWidget::connect(
		_index,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(IndexChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_indexEnd,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(IndexEndChanged(const NumberVariable<int> &)));
	QWidget::connect(_pattern, SIGNAL(editingFinished()), this,
			 SLOT(PatternChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_changeType, SIGNAL(clicked()), this,
			 SLOT(ChangeType()));

	_controlsLayout->setContentsMargins(0, 0, 0, 0);
	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(_controlsLayout);
	layout->addWidget(_changeType);
	setLayout(layout);
	_nameConflictIndex->hide();
}

void SceneItemSelectionWidget::SetSceneItem(const SceneItemSelection &item)
{
	_sources->setCurrentIndex(_sources->findText(
		QString::fromStdString(GetWeakSourceName(item._source))));
	_sourceGroups->setCurrentText(
		QString::fromStdString(GetWeakSourceName(item._source)));
	_variables->SetVariable(item._variable);
	_index->SetValue(item._index);
	_indexEnd->SetValue(item._indexEnd);
	_sourceTypes->setCurrentIndex(_sourceTypes->findText(
		QString::fromStdString(item._sourceType)));
	_pattern->setText(item._pattern);
	_regex->SetRegexConfig(item._regex);

	_currentSelection = item;

	{
		const QSignalBlocker b(_nameConflictIndex);
		SetNameConflictVisibility();
	}

	auto type = item._nameConflictSelectionType;

	if (type == NameConflictSelection::INDIVIDUAL) {
		const int selection = item._nameConflictSelectionIndex;
		const int idx = _nameConflictIndex->findData(
			QVariant::fromValue(ConflictIndex(selection)));
		_nameConflictIndex->setCurrentIndex(idx);
	} else {
		const auto selection = type == NameConflictSelection::ANY
					       ? NameClashMode::ANY
					       : NameClashMode::ALL;
		const int idx = _nameConflictIndex->findData(
			QVariant::fromValue(ConflictIndex(selection)));
		_nameConflictIndex->setCurrentIndex(idx);
	}

	SetWidgetVisibility();
}

void SceneItemSelectionWidget::SetScene(const SceneSelection &s)
{

	_scene = s;
	_nameConflictIndex->hide();
	if (_currentSelection._type != SceneItemSelection::Type::SOURCE_NAME) {
		PopulateItemSelection();
		return;
	}

	auto previous = _currentSelection;
	PopulateItemSelection();
	SetSceneItem(previous);
}

void SceneItemSelectionWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	const QSignalBlocker b1(_sources);
	const QSignalBlocker b2(this);
	PopulateItemSelection();
	SetSceneItem(_currentSelection);
}

void SceneItemSelectionWidget::SceneChanged(const SceneSelection &s)
{
	SetScene(s);
	adjustSize();
}

void SceneItemSelectionWidget::SetNameConflictVisibility()
{
	int sceneItemCount = 0;

	switch (_currentSelection._type) {
	case SceneItemSelection::Type::SOURCE_NAME:
	case SceneItemSelection::Type::VARIABLE_NAME: {
		QString name = "";
		if (_currentSelection._type ==
		    SceneItemSelection::Type::VARIABLE_NAME) {
			auto var = _currentSelection._variable.lock();
			if (var) {
				name = QString::fromStdString(var->Value());
			}
		}
		if (_currentSelection._type ==
		    SceneItemSelection::Type::SOURCE_NAME) {
			name = _sources->currentText();
		}
		if (name.isEmpty()) {
			break;
		}
		sceneItemCount = getCountOfSceneItemOccurance(
			_scene, name.toStdString());
		break;
	}

	case SceneItemSelection::Type::SOURCE_GROUP:
		sceneItemCount = _currentSelection.GetSceneItems(_scene).size();
		break;
	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
	case SceneItemSelection::Type::SOURCE_TYPE:
		sceneItemCount = GetSceneItemCount(_scene.GetScene(false));
		break;
	case SceneItemSelection::Type::INDEX:
	case SceneItemSelection::Type::INDEX_RANGE:
	case SceneItemSelection::Type::ALL:
	case SceneItemSelection::Type::ANY:
		break;
	}

	if (_currentSelection._type ==
	    SceneItemSelection::Type::SOURCE_NAME_PATTERN) {
		int sceneItemCount = GetSceneItemCount(_scene.GetScene(false));
		if (sceneItemCount == 0) {
			_nameConflictIndex->hide();
			return;
		}
		SetupNameConflictIdxSelection(sceneItemCount);
		_nameConflictIndex->show();
		return;
	}

	if (sceneItemCount == 0) {
		_nameConflictIndex->hide();
		return;
	}
	if (sceneItemCount > 1) {
		SetupNameConflictIdxSelection(sceneItemCount);
		_nameConflictIndex->show();
	} else {
		_nameConflictIndex->hide();
	}
}

void SceneItemSelectionWidget::VariableChanged(const QString &text)
{
	_currentSelection._variable = GetWeakVariableByQString(text);
	SetNameConflictVisibility();
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::SourceChanged(int)
{
	_currentSelection._source =
		GetWeakSourceByQString(_sources->currentText());
	SetNameConflictVisibility();
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::SourceGroupChanged(int)
{
	_currentSelection._source =
		GetWeakSourceByQString(_sourceGroups->currentText());
	SetNameConflictVisibility();
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::IndexChanged(const IntVariable &index)
{
	_currentSelection._index = index;
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::IndexEndChanged(const IntVariable &index)
{
	_currentSelection._indexEnd = index;
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::NameConflictIndexChanged(int idx)
{
	if (idx < 0) {
		return;
	}

	QVariant data = _nameConflictIndex->currentData(Qt::UserRole);
	if (!data.canConvert<ConflictIndex>()) {
		return;
	}

	const auto setSelection = [this](int val, NameConflictSelection type) {
		_currentSelection._nameConflictSelectionIndex = val;
		_currentSelection._nameConflictSelectionType = type;
	};

	const auto visit = [this, &setSelection](auto &&val) {
		using T = std::decay_t<decltype(val)>;
		if constexpr (std::is_same_v<T, int>) {
			setSelection(val, NameConflictSelection::INDIVIDUAL);
		} else if constexpr (std::is_same_v<T, NameClashMode>) {
			setSelection(0, val == NameClashMode::ANY
						? NameConflictSelection::ANY
						: NameConflictSelection::ALL);
		}
	};

	const auto variant = data.value<ConflictIndex>();
	std::visit(visit, variant);

	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::PatternChanged()
{
	_currentSelection._pattern = _pattern->text().toStdString();
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::RegexChanged(const RegexConfig &regex)
{
	_currentSelection._regex = regex;
	// No reason for the regex to be disabled
	_currentSelection._regex.SetEnabled(true);
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::SourceTypeChanged(const QString &text)
{
	if (text == obs_module_text("AdvSceneSwitcher.selectItem")) {
		_currentSelection._sourceType = "";
	} else {
		_currentSelection._sourceType = text.toStdString();
	}
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::ChangeType()
{
	bool accepted = SceneItemTypeSelection::AskForSettings(
		this, _currentSelection._type, _selectionTypes);
	if (!accepted) {
		return;
	}

	SetNameConflictVisibility();
	SetWidgetVisibility();
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::SetupNameConflictIdxSelection(int sceneItemCount)
{
	_nameConflictIndex->clear();

	switch (_nameClashMode) {
	case NameClashMode::NONE:
		break;
	case NameClashMode::ALL:
		_nameConflictIndex->addItem(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.all"),
			QVariant::fromValue(ConflictIndex(NameClashMode::ALL)));
		break;
	case NameClashMode::ANY:
		_nameConflictIndex->addItem(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.any"),
			QVariant::fromValue(ConflictIndex(NameClashMode::ANY)));
		break;
	case NameClashMode::ANY_AND_ALL:
		_nameConflictIndex->addItem(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.any"),
			QVariant::fromValue(ConflictIndex(NameClashMode::ANY)));
		_nameConflictIndex->addItem(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.all"),
			QVariant::fromValue(ConflictIndex(NameClashMode::ALL)));
		break;
	default:
		break;
	}

	for (int i = 1; i <= sceneItemCount; ++i) {
		_nameConflictIndex->addItem(
			QString::number(i) + ".",
			QVariant::fromValue(ConflictIndex(i - 1)));
	};

	adjustSize();
	updateGeometry();
}

void SceneItemSelectionWidget::ClearWidgets()
{
	_controlsLayout->removeWidget(_nameConflictIndex);
	_controlsLayout->removeWidget(_sources);
	_controlsLayout->removeWidget(_sourceGroups);
	_controlsLayout->removeWidget(_variables);
	_controlsLayout->removeWidget(_pattern);
	_controlsLayout->removeWidget(_regex);
	_controlsLayout->removeWidget(_sourceTypes);
	_controlsLayout->removeWidget(_index);
	_controlsLayout->removeWidget(_indexEnd);
	ClearLayout(_controlsLayout);
}

void SceneItemSelectionWidget::SetWidgetVisibility()
{
	ClearWidgets();
	const std::unordered_map<std::string, QWidget *> widgetMap = {
		{"{{nameConflictIndex}}", _nameConflictIndex},
		{"{{sourceName}}", _sources},
		{"{{sourceGroups}}", _sourceGroups},
		{"{{variable}}", _variables},
		{"{{pattern}}", _pattern},
		{"{{regex}}", _regex},
		{"{{sourceTypes}}", _sourceTypes},
		{"{{index}}", _index},
		{"{{indexEnd}}", _indexEnd},
	};

	switch (_currentSelection._type) {
	case SceneItemSelection::Type::SOURCE_NAME:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceName.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::VARIABLE_NAME:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceVariable.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceNamePattern.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::SOURCE_GROUP:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceGroup.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::SOURCE_TYPE:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceType.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::INDEX:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.index.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::INDEX_RANGE:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.indexRange.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::ALL:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.all.layout"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::ANY:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.any.layout"),
			_controlsLayout, widgetMap, false);
		break;
	default:
		break;
	}

	// Name based
	_sources->setVisible(_currentSelection._type ==
			     SceneItemSelection::Type::SOURCE_NAME);
	_variables->setVisible(_currentSelection._type ==
			       SceneItemSelection::Type::VARIABLE_NAME);

	const bool isRegexSelection =
		_currentSelection._type ==
		SceneItemSelection::Type::SOURCE_NAME_PATTERN;
	_pattern->setVisible(isRegexSelection);
	_regex->setVisible(isRegexSelection);

	const bool isNameSelection =
		_currentSelection._type ==
			SceneItemSelection::Type::VARIABLE_NAME ||
		_currentSelection._type ==
			SceneItemSelection::Type::SOURCE_NAME;

	// Group based
	_sourceGroups->setVisible(_currentSelection._type ==
				  SceneItemSelection::Type::SOURCE_GROUP);

	_sourceTypes->setVisible(_currentSelection._type ==
				 SceneItemSelection::Type::SOURCE_TYPE);

	if (!isNameSelection && !isRegexSelection &&
	    _currentSelection._type != SceneItemSelection::Type::SOURCE_TYPE &&
	    _currentSelection._type != SceneItemSelection::Type::SOURCE_GROUP) {
		_nameConflictIndex->hide();
	}

	// Index based
	_index->setVisible(_currentSelection._type ==
				   SceneItemSelection::Type::INDEX ||
			   _currentSelection._type ==
				   SceneItemSelection::Type::INDEX_RANGE);
	_indexEnd->setVisible(_currentSelection._type ==
			      SceneItemSelection::Type::INDEX_RANGE);

	if (_nameClashMode == NameClashMode::HIDE) {
		_nameConflictIndex->hide();
	}

	adjustSize();
	updateGeometry();
}

SceneItemTypeSelection::SceneItemTypeSelection(
	QWidget *parent, const SceneItemSelection::Type &type)
	: QDialog(parent),
	  _typeSelection(new QComboBox(this)),
	  _buttonbox(new QDialogButtonBox(QDialogButtonBox::Ok |
					  QDialogButtonBox::Cancel))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	_typeSelection->setCurrentIndex(
		_typeSelection->findData(static_cast<int>(type)));

	QWidget::connect(_buttonbox, &QDialogButtonBox::accepted, this,
			 &QDialog::accept);
	QWidget::connect(_buttonbox, &QDialogButtonBox::rejected, this,
			 &QDialog::reject);
	auto layout = new QVBoxLayout();
	layout->addWidget(_typeSelection);
	layout->addWidget(_buttonbox, Qt::AlignHCenter);
	setLayout(layout);
}

bool SceneItemTypeSelection::AskForSettings(
	QWidget *parent, SceneItemSelection::Type &type,
	const std::vector<SceneItemSelection::Type> &allowedTypes)
{
	SceneItemTypeSelection dialog(parent, type);
	dialog._typeSelection->clear();
	populateMessageTypeSelection(dialog._typeSelection, allowedTypes);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	type = static_cast<SceneItemSelection::Type>(
		dialog._typeSelection->currentData().toInt());
	return true;
}

} // namespace advss
