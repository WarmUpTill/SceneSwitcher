#include "scene-selection.hpp"
#include "obs-module-helper.hpp"
#include "scene-group.hpp"
#include "scene-switch-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "variable.hpp"

#include <obs-frontend-api.h>

#include <QLayout>

Q_DECLARE_METATYPE(OBSWeakCanvas);

namespace advss {

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view nameSaveName = "name";
constexpr std::string_view selectionSaveName = "sceneSelection";
constexpr std::string_view canvasSaveName = "canvasSelection";

void SceneSelection::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	switch (_type) {
	case Type::SCENE:
		obs_data_set_string(data, nameSaveName.data(),
				    GetWeakSourceName(_scene).c_str());
		break;
	case Type::GROUP:
		obs_data_set_string(data, nameSaveName.data(),
				    _group->name.c_str());
		break;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			break;
		}
		obs_data_set_string(data, nameSaveName.data(),
				    var->Name().c_str());
		break;
	}
	default:
		break;
	}
	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(_canvas);
	obs_data_set_string(data, canvasSaveName.data(),
			    canvas ? obs_canvas_get_name(canvas) : "");
	obs_data_set_obj(obj, selectionSaveName.data(), data);
}

static OBSWeakCanvas getWeakCanvasByName(const char *name)
{
	struct Data {
		OBSWeakCanvas canvas;
		const std::string name;
	};

	static const auto enumCanvases = [](void *dataPtr,
					    obs_canvas_t *canvas) -> bool {
		auto data = static_cast<Data *>(dataPtr);
		if (data->name != obs_canvas_get_name(canvas)) {
			return true;
		}

		data->canvas = obs_canvas_get_weak_canvas(canvas);
		obs_weak_canvas_release(data->canvas);
		return false;
	};
	Data data{nullptr, name};
	obs_enum_canvases(enumCanvases, &data);
	return data.canvas;
}

static OBSWeakSource getWeakSceneSourceByName(OBSWeakCanvas &weakCanvas,
					      const char *name)
{
	if (!weakCanvas) {
		return nullptr;
	}

	struct Data {
		OBSWeakSource scene;
		const std::string name;
	};

	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	static const auto enumCanvasScenes = [](void *dataPtr,
						obs_source_t *source) -> bool {
		auto data = static_cast<Data *>(dataPtr);
		if (data->name != obs_source_get_name(source)) {
			return true;
		}

		data->scene = obs_source_get_weak_source(source);
		obs_weak_source_release(data->scene);
		return false;
	};

	Data data{nullptr, name};
	obs_canvas_enum_scenes(canvas, enumCanvasScenes, &data);
	return data.scene;
}

void SceneSelection::Load(obs_data_t *obj, const char *name,
			  const char *typeName)
{
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, selectionSaveName.data())) {
		_type = static_cast<Type>(obs_data_get_int(obj, typeName));

		OBSCanvasAutoRelease mainCanvas = obs_get_main_canvas();
		_canvas = obs_canvas_get_weak_canvas(mainCanvas);
		obs_weak_canvas_release(_canvas);

		auto targetName = obs_data_get_string(obj, name);
		switch (_type) {
		case Type::SCENE:
			_scene = getWeakSceneSourceByName(_canvas, targetName);
			break;
		case Type::GROUP:
			_group = GetSceneGroupByName(targetName);
			break;
		case Type::PREVIOUS:
			break;
		case Type::CURRENT:
			break;
		default:
			break;
		}
		return;
	}

	OBSDataAutoRelease data =
		obs_data_get_obj(obj, selectionSaveName.data());

	if (obs_data_has_user_value(data, canvasSaveName.data())) {
		_canvas = getWeakCanvasByName(
			obs_data_get_string(data, canvasSaveName.data()));
	} else {
		OBSCanvasAutoRelease mainCanvas = obs_get_main_canvas();
		_canvas = obs_canvas_get_weak_canvas(mainCanvas);
		obs_weak_canvas_release(_canvas);
	}

	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	auto targetName = obs_data_get_string(data, nameSaveName.data());
	switch (_type) {
	case Type::SCENE:
		_scene = getWeakSceneSourceByName(_canvas, targetName);
		break;
	case Type::GROUP:
		_group = GetSceneGroupByName(targetName);
		break;
	case Type::PREVIOUS:
		break;
	case Type::CURRENT:
		break;
	case Type::PREVIEW:
		break;
	case Type::VARIABLE:
		_variable = GetWeakVariableByName(targetName);
		break;
	default:
		break;
	}
}

static bool IsScene(const OBSWeakSource &source)
{
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	bool ret = !!obs_scene_from_source(s);
	return ret;
}

OBSWeakSource SceneSelection::GetScene(bool advance) const
{
	switch (_type) {
	case Type::SCENE:
		return _scene;
	case Type::GROUP:
		if (!_group) {
			return nullptr;
		}
		if (advance) {
			return _group->getNextScene();
		}
		return _group->getCurrentScene();
	case Type::PREVIOUS:
		return GetPreviousScene();
	case Type::CURRENT:
		return GetCurrentScene();
	case Type::PREVIEW: {
		OBSSourceAutoRelease s =
			obs_frontend_get_current_preview_scene();
		OBSWeakSourceAutoRelease scene = obs_source_get_weak_source(s);
		return scene.Get();
	}
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return nullptr;
		}
		auto scene = GetWeakSourceByName(var->Value().c_str());
		if (IsScene(scene)) {
			return scene;
		}
		return nullptr;
	}
	default:
		break;
	}
	return nullptr;
}

std::string SceneSelection::ToString(bool resolve) const
{
	switch (_type) {
	case Type::SCENE:
		return GetWeakSourceName(_scene);
	case Type::GROUP:
		if (_group) {
			if (resolve) {
				return _group->name + "[" +
				       GetWeakSourceName(
					       _group->getCurrentScene()) +
				       "]";
			}
			return _group->name;
		}
		break;
	case Type::PREVIOUS:
		return obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	case Type::CURRENT:
		return obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	case Type::PREVIEW:
		return obs_module_text("AdvSceneSwitcher.selectPreviewScene");
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		if (resolve) {
			return var->Name() + "[" + var->Value() + "]";
		}
		return var->Name();
	}
	default:
		break;
	}
	return "";
}

void SceneSelection::ResolveVariables()
{
	_scene = GetScene();
	_type = Type::SCENE;
}

SceneSelection SceneSelectionWidget::CurrentSelection()
{
	SceneSelection s;
	s._canvas = _canvas->itemData(_canvas->currentIndex())
			    .value<OBSWeakCanvas>();

	const int idx = _scenes->currentIndex();
	const auto name = _scenes->currentText();
	if (idx == -1 || name.isEmpty()) {
		return s;
	}

	if (idx < _placeholderEndIdx) {
		if (IsCurrentSceneSelected(name)) {
			s._type = SceneSelection::Type::CURRENT;
		}
		if (IsPreviousSceneSelected(name)) {
			s._type = SceneSelection::Type::PREVIOUS;
		}
		if (IsPreviewSceneSelected(name)) {
			s._type = SceneSelection::Type::PREVIEW;
		}
	} else if (idx < _variablesEndIdx) {
		s._type = SceneSelection::Type::VARIABLE;
		s._variable = GetWeakVariableByQString(name);
	} else if (idx < _groupsEndIdx) {
		s._type = SceneSelection::Type::GROUP;
		s._group = GetSceneGroupByQString(name);
	} else if (idx < _scenesEndIdx) {
		s._type = SceneSelection::Type::SCENE;
		s._scene = getWeakSceneSourceByName(s._canvas,
						    name.toStdString().c_str());
	}
	return s;
}

static QStringList getOrderList(bool current, bool previous, bool preview)
{
	QStringList list;
	if (current) {
		list << obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	}
	if (previous) {
		list << obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	}
	if (preview) {
		list << obs_module_text("AdvSceneSwitcher.selectPreviewScene");
	}
	return list;
}

static QStringList getSceneGroupsList()
{
	QStringList list;
	for (const auto &sg : GetSceneGroups()) {
		list << QString::fromStdString(sg.name);
	}
	list.sort();
	return list;
}

static QStringList getCanvasScenesList(obs_weak_canvas_t *weakCanvas)
{
	if (!weakCanvas) {
		return {};
	}

	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	static const auto enumCanvasScenes = [](void *listPtr,
						obs_source_t *source) -> bool {
		auto list = static_cast<QStringList *>(listPtr);
		*list << obs_source_get_name(source);
		return true;
	};

	QStringList list;
	obs_canvas_enum_scenes(canvas, enumCanvasScenes, &list);
	return list;
}

void SceneSelectionWidget::Reset()
{
	auto previousSel = _currentSelection;
	PopulateSceneSelection(_currentSelection._canvas);
	SetScene(previousSel);
}

void SceneSelectionWidget::PopulateSceneSelection(obs_weak_canvas_t *canvas)
{
	_scenes->clear();
	if (_current || _previous) {
		const QStringList order =
			getOrderList(_current, _previous, _preview);
		AddSelectionGroup(_scenes, order);
	}
	_placeholderEndIdx = _scenes->count();

	if (_variables) {
		const QStringList variables = GetVariablesNameList();
		AddSelectionGroup(_scenes, variables);
	}
	_variablesEndIdx = _scenes->count();

	if (_sceneGroups) {
		const QStringList sceneGroups = getSceneGroupsList();
		AddSelectionGroup(_scenes, sceneGroups);
	}
	_groupsEndIdx = _scenes->count();

	const QStringList scenes = getCanvasScenesList(canvas);
	AddSelectionGroup(_scenes, scenes);
	_scenesEndIdx = _scenes->count();

	// Remove last separator
	_scenes->removeItem(_scenes->count() - 1);
	_scenes->setCurrentIndex(-1);

	Resize();
}

static int getCanvasCount()
{
	static const auto enumCanvases = [](void *countPtr,
					    obs_canvas_t *canvas) -> bool {
		auto count = static_cast<int *>(countPtr);
		(*count)++;
		return true;
	};
	int count = 0;
	obs_enum_canvases(enumCanvases, &count);
	return count;
}

void SceneSelectionWidget::PopulateCanvasSelection()
{
	static const auto enumCanvases = [](void *listPtr,
					    obs_canvas_t *canvas) -> bool {
		auto list = static_cast<FilterComboBox *>(listPtr);
		OBSWeakCanvas weakCanvas = obs_canvas_get_weak_canvas(canvas);
		obs_weak_canvas_release(weakCanvas);
		QVariant variant;
		variant.setValue(weakCanvas);
		list->addItem(obs_canvas_get_name(canvas), variant);
		return true;
	};
	obs_enum_canvases(enumCanvases, _canvas);
}

SceneSelectionWidget::SceneSelectionWidget(QWidget *parent, bool variables,
					   bool sceneGroups, bool previous,
					   bool current, bool preview)
	: QWidget(parent),
	  _scenes(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectScene"))),
	  _canvas(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectCanvas"))),
	  _current(current),
	  _previous(previous),
	  _preview(preview),
	  _variables(variables),
	  _sceneGroups(sceneGroups)
{
	_scenes->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_canvas->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	_scenes->setDuplicatesEnabled(true);
	PopulateCanvasSelection();

	OBSCanvasAutoRelease mainCanvas = obs_get_main_canvas();
	OBSWeakCanvasAutoRelease canvas =
		obs_canvas_get_weak_canvas(mainCanvas);
	PopulateSceneSelection(canvas);

	QWidget::connect(_scenes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));
	QWidget::connect(_canvas, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CanvasChanged(int)));

	// Scene groups
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupAdded(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupRemoved(const QString &)), this,
			 SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		GetSettingsWindow(),
		SIGNAL(SceneGroupRenamed(const QString &, const QString &)),
		this, SLOT(ItemRename(const QString &, const QString &)));

	// Variables
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(ItemRemove(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(ItemRename(const QString &, const QString &)));

	auto layout = new QHBoxLayout();
	layout->addWidget(_canvas);
	layout->addWidget(_scenes);
	setLayout(layout);

	if (getCanvasCount() <= 1) {
		_canvas->hide();
		layout->setContentsMargins(0, 0, 0, 0);
	}

	Resize();
}

void SceneSelectionWidget::SetScene(const SceneSelection &s)
{
	int idx = -1;

	QVariant variant;
	variant.setValue(s._canvas);
	_canvas->setCurrentIndex(_canvas->findData(variant));

	switch (s.GetType()) {
	case SceneSelection::Type::SCENE: {
		if (_scenesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(_scenes, _groupsEndIdx, _scenesEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::GROUP: {
		if (_groupsEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(_scenes, _variablesEndIdx, _groupsEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::PREVIOUS: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			_scenes, _selectIdx, _placeholderEndIdx,
			obs_module_text(
				"AdvSceneSwitcher.selectPreviousScene"));
		break;
	}
	case SceneSelection::Type::CURRENT: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			_scenes, _selectIdx, _placeholderEndIdx,
			obs_module_text("AdvSceneSwitcher.selectCurrentScene"));
		break;
	}
	case SceneSelection::Type::PREVIEW: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			_scenes, _selectIdx, _placeholderEndIdx,
			obs_module_text("AdvSceneSwitcher.selectPreviewScene"));
		break;
	}
	case SceneSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(_scenes, _placeholderEndIdx,
				     _variablesEndIdx, s.ToString());
		break;
	default:
		idx = -1;
		break;
	}
	}
	_scenes->setCurrentIndex(idx);
	_currentSelection = s;

	Resize();
}

void SceneSelectionWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	const QSignalBlocker b1(_scenes);
	const QSignalBlocker b2(_canvas);
	Reset();
}

bool SceneSelectionWidget::IsCurrentSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectCurrentScene")));
}

bool SceneSelectionWidget::IsPreviousSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectPreviousScene")));
}

bool SceneSelectionWidget::IsPreviewSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectPreviewScene")));
}

void SceneSelectionWidget::SelectionChanged(int)
{
	_currentSelection = CurrentSelection();
	emit SceneChanged(_currentSelection);
}

void SceneSelectionWidget::CanvasChanged(int)
{
	_currentSelection = CurrentSelection();
	Reset();
}

void SceneSelectionWidget::ItemAdd(const QString &)
{
	const QSignalBlocker b1(_scenes);
	const QSignalBlocker b2(_canvas);
	Reset();
}

bool SceneSelectionWidget::NameUsed(const QString &name)
{
	if (_currentSelection._type == SceneSelection::Type::GROUP &&
	    _currentSelection._group &&
	    _currentSelection._group->name == name.toStdString()) {
		return true;
	}
	return _currentSelection._type == SceneSelection::Type::VARIABLE &&
	       _scenes->currentText() == name;
}

void SceneSelectionWidget::ItemRemove(const QString &name)
{
	if (!NameUsed(name)) {
		blockSignals(true);
	}
	Reset();
	blockSignals(false);
}

void SceneSelectionWidget::ItemRename(const QString &, const QString &)
{
	const QSignalBlocker b1(_scenes);
	const QSignalBlocker b2(_canvas);
	Reset();
}

void SceneSelectionWidget::Resize()
{
	_scenes->adjustSize();
	_scenes->updateGeometry();
	_canvas->adjustSize();
	_canvas->updateGeometry();
	adjustSize();
	updateGeometry();
}

} // namespace advss
