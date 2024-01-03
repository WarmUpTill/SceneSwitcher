#include "scene-item-selection.hpp"
#include "obs-module-helper.hpp"

namespace advss {

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view itemSaveName = "item";
constexpr std::string_view indexSaveName = "index";
constexpr std::string_view indexEndSaveName = "indexEnd";
constexpr std::string_view nameConflictIndexSaveName = "idx";
constexpr std::string_view nameConflictIndexSelectionSaveName = "idxType";
constexpr std::string_view patternSaveName = "pattern";

const static std::map<SceneItemSelection::Type, std::string> types = {
	{SceneItemSelection::Type::SOURCE_NAME,
	 "AdvSceneSwitcher.sceneItemSelection.type.sourceName"},
	{SceneItemSelection::Type::VARIABLE_NAME,
	 "AdvSceneSwitcher.sceneItemSelection.type.sourceVariable"},
	{SceneItemSelection::Type::SOURCE_NAME_PATTERN,
	 "AdvSceneSwitcher.sceneItemSelection.type.sourceNamePattern"},
	{SceneItemSelection::Type::SOURCE_GROUP,
	 "AdvSceneSwitcher.sceneItemSelection.type.sourceGroup"},
	{SceneItemSelection::Type::INDEX,
	 "AdvSceneSwitcher.sceneItemSelection.type.index"},
	{SceneItemSelection::Type::INDEX_RANGE,
	 "AdvSceneSwitcher.sceneItemSelection.type.indexRange"},
	{SceneItemSelection::Type::ALL,
	 "AdvSceneSwitcher.sceneItemSelection.type.all"},
};

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
		obs_data_set_string(obj, "sourceGroup", _sourceGroup.c_str());
		break;
	case SceneItemSelection::Type::INDEX:
		_index.Save(data, indexSaveName.data());
		break;
	case SceneItemSelection::Type::INDEX_RANGE:
		_index.Save(data, indexSaveName.data());
		_index.Save(data, indexEndSaveName.data());
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
		_sourceGroup = obs_data_get_string(obj, "sourceGroup");
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
	_type = Type::SOURCE_GROUP;
	_sourceGroup = type;
}

void SceneItemSelection::ReduceBadedOnIndexSelection(
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
	ReduceBadedOnIndexSelection(items);
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
	ReduceBadedOnIndexSelection(data.items);
	return data.items;
}

std::vector<OBSSceneItem> SceneItemSelection::GetSceneItemsByGroup(
	const SceneSelection &sceneSelection) const
{
	if (_sourceGroup.empty()) {
		return {};
	}

	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	GroupData data{_sourceGroup};
	obs_scene_enum_items(scene, getSceneItemsOfType, &data);
	obs_source_release(s);
	ReduceBadedOnIndexSelection(data.items);
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
		return GetSceneItemsByGroup(sceneSelection);
	case Type::INDEX:
	case Type::INDEX_RANGE:
		return GetSceneItemsByIdx(sceneSelection);
	case Type::ALL:
		return GetAllSceneItems(sceneSelection);
	default:
		break;
	}

	return {};
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
			       " \"" + std::string(_sourceGroup) + "\"";
		}
		return _sourceGroup;
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

QStringList GetSceneItemsList(SceneSelection &s)
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
	const QStringList sceneItmes = GetSceneItemsList(_scene);
	AddSelectionGroup(_sources, sceneItmes, false);
	_sources->setCurrentIndex(-1);
}

static inline void populateMessageTypeSelection(QComboBox *list)
{
	for (const auto &[type, name] : types) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

SceneItemSelectionWidget::SceneItemSelectionWidget(QWidget *parent,
						   bool showAll,
						   Placeholder type)
	: QWidget(parent),
	  _controlsLayout(new QHBoxLayout),
	  _sources(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectItem"))),
	  _variables(new VariableSelection(this)),
	  _nameConflictIndex(new QComboBox(this)),
	  _index(new VariableSpinBox(this)),
	  _indexEnd(new VariableSpinBox(this)),
	  _sourceGroups(new QComboBox(this)),
	  _pattern(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(this, false)),
	  _changeType(new QPushButton(this)),
	  _hasPlaceholderEntry(showAll),
	  _placeholder(type)
{
	_sources->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_nameConflictIndex->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	_changeType->setMaximumWidth(22);
	SetButtonIcon(_changeType, ":/settings/images/settings/general.svg");
	_changeType->setFlat(true);
	_changeType->setToolTip(obs_module_text(
		"AdvSceneSwitcher.sceneItemSelection.configure"));

	_index->setMinimum(1);
	_index->setSuffix(".");
	_indexEnd->setMinimum(1);
	_indexEnd->setSuffix(".");

	PopulateSourceGroupSelection(_sourceGroups);

	QWidget::connect(_sources, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceChanged(int)));
	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_nameConflictIndex, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(NameConflictIndexChanged(int)));
	QWidget::connect(_sourceGroups,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceGroupChanged(const QString &)));
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
	int idx = item._nameConflictSelectionIndex;
	if (_hasPlaceholderEntry) {
		idx += 1;
	}
	_nameConflictIndex->setCurrentIndex(idx);
	_sources->setCurrentText(
		QString::fromStdString(GetWeakSourceName(item._source)));
	_variables->SetVariable(item._variable);
	_index->SetValue(item._index);
	_indexEnd->SetValue(item._indexEnd);
	_sourceGroups->setCurrentIndex(_sourceGroups->findText(
		QString::fromStdString(item._sourceGroup)));
	_pattern->setText(item._pattern);
	_regex->SetRegexConfig(item._regex);

	SetNameConflictVisibility();

	switch (item._nameConflictSelectionType) {
	case SceneItemSelection::NameConflictSelection::ALL:
	case SceneItemSelection::NameConflictSelection::ANY:
		_placeholder = Placeholder::ALL;
		_nameConflictIndex->setCurrentIndex(0);
		break;
	case SceneItemSelection::NameConflictSelection::INDIVIDUAL:
		int idx = item._nameConflictSelectionIndex;
		if (_hasPlaceholderEntry) {
			idx += 1;
		}
		_nameConflictIndex->setCurrentIndex(idx);
		break;
	}
	_currentSelection = item;

	SetWidgetVisibility();
}

void SceneItemSelectionWidget::SetScene(const SceneSelection &s)
{
	_scene = s;
	_sources->clear();
	_nameConflictIndex->hide();
	PopulateItemSelection();
}

void SceneItemSelectionWidget::ShowPlaceholder(bool value)
{
	_hasPlaceholderEntry = value;
}

void SceneItemSelectionWidget::SetPlaceholderType(Placeholder t,
						  bool resetSelection)
{
	_placeholder = t;
	if (resetSelection) {
		_sources->setCurrentIndex(-1);
	} else {
		auto count = _nameConflictIndex->count() - 1;
		const QSignalBlocker b(_nameConflictIndex);
		SetupNameConflictIdxSelection(count > 0 ? count : 1);
	}
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

	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
	case SceneItemSelection::Type::SOURCE_GROUP:
		sceneItemCount = GetSceneItemCount(_scene.GetScene(false));
		break;
	case SceneItemSelection::Type::INDEX:
	case SceneItemSelection::Type::INDEX_RANGE:
	case SceneItemSelection::Type::ALL:
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

	_currentSelection._nameConflictSelectionIndex = idx;
	if (_hasPlaceholderEntry && idx == 0) {
		switch (_placeholder) {
		case SceneItemSelectionWidget::Placeholder::ALL:
			_currentSelection._nameConflictSelectionType =
				SceneItemSelection::NameConflictSelection::ALL;
			break;
		case SceneItemSelectionWidget::Placeholder::ANY:
			_currentSelection._nameConflictSelectionType =
				SceneItemSelection::NameConflictSelection::ANY;
			break;
		}
	}
	if (_hasPlaceholderEntry && idx > 0) {
		_currentSelection._nameConflictSelectionIndex -= 1;
		_currentSelection._nameConflictSelectionType =
			SceneItemSelection::NameConflictSelection::INDIVIDUAL;
	}
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

void SceneItemSelectionWidget::SourceGroupChanged(const QString &text)
{
	if (text == obs_module_text("AdvSceneSwitcher.selectItem")) {
		_currentSelection._sourceGroup = "";
	} else {
		_currentSelection._sourceGroup = text.toStdString();
	}
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::ChangeType()
{
	bool accepted = SceneItemTypeSelection::AskForSettings(
		this, _currentSelection._type);
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
	if (_hasPlaceholderEntry) {
		if (_placeholder == Placeholder::ALL) {
			_nameConflictIndex->addItem(obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.all"));
		} else {
			_nameConflictIndex->addItem(obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.any"));
		}
	}
	for (int i = 1; i <= sceneItemCount; ++i) {
		_nameConflictIndex->addItem(QString::number(i) + ".");
	}
	adjustSize();
	updateGeometry();
}

void SceneItemSelectionWidget::ClearWidgets()
{
	_controlsLayout->removeWidget(_nameConflictIndex);
	_controlsLayout->removeWidget(_sources);
	_controlsLayout->removeWidget(_variables);
	_controlsLayout->removeWidget(_pattern);
	_controlsLayout->removeWidget(_regex);
	_controlsLayout->removeWidget(_sourceGroups);
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
		{"{{variable}}", _variables},
		{"{{pattern}}", _pattern},
		{"{{regex}}", _regex},
		{"{{sourceGroups}}", _sourceGroups},
		{"{{index}}", _index},
		{"{{indexEnd}}", _indexEnd},
	};

	switch (_currentSelection._type) {
	case SceneItemSelection::Type::SOURCE_NAME:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceName.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::VARIABLE_NAME:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceVariable.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::SOURCE_NAME_PATTERN:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceNamePattern.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::SOURCE_GROUP:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.sourceGroup.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::INDEX:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.index.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::INDEX_RANGE:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.indexRange.entry"),
			_controlsLayout, widgetMap, false);
		break;
	case SceneItemSelection::Type::ALL:
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.type.all.entry"),
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

	if (!isNameSelection && !isRegexSelection &&
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

	populateMessageTypeSelection(_typeSelection);
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

bool SceneItemTypeSelection::AskForSettings(QWidget *parent,
					    SceneItemSelection::Type &type)
{
	SceneItemTypeSelection dialog(parent, type);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	type = static_cast<SceneItemSelection::Type>(
		dialog._typeSelection->currentData().toInt());
	return true;
}

} // namespace advss
