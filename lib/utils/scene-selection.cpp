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
	obs_data_set_string(data, canvasSaveName.data(),
			    GetWeakCanvasName(_canvas).c_str());
	obs_data_set_obj(obj, selectionSaveName.data(), data);
}

static OBSWeakSource getWeakSceneSourceByName(const OBSWeakCanvas &weakCanvas,
					      const char *name)
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	return GetWeakSourceByName(name);
#else
	if (!weakCanvas) {
		return nullptr;
	}

	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	if (!canvas) {
		return nullptr;
	}

	OBSSourceAutoRelease source =
		obs_canvas_get_source_by_name(canvas, name);
	OBSWeakSource scene = OBSGetWeakRef(source);
	return scene;
#endif
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
		_canvas = GetWeakCanvasByName(
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

static bool isScene(const OBSWeakSource &source)
{
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	return !!obs_scene_from_source(s);
}

void SceneSelection::SetScene(const OBSWeakSource &scene)
{
	_type = Type::SCENE;
	_scene = scene;
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
		if (!IsMainCanvas(_canvas)) {
			return nullptr;
		}
		return GetPreviousScene();
	case Type::CURRENT:
		if (IsMainCanvas(_canvas)) {
			return GetCurrentScene();
		} else {
			return GetActiveCanvasScene(_canvas);
		}
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
		auto scene =
			getWeakSceneSourceByName(_canvas, var->Value().c_str());
		if (isScene(scene)) {
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

	static auto mainCanvas = obs_get_main_canvas();
	static auto mainCanvasWeak = obs_canvas_get_weak_canvas(mainCanvas);
	[[maybe_unused]] static const bool _ = []() {
		// Let's just hope we don't have to deal with selecting scenes
		// when the OBS main canvas gets deleted and release the
		// references here already to avoid reporting leaks on shutdown
		obs_canvas_release(mainCanvas);
		obs_weak_canvas_release(mainCanvasWeak);
		return true;
	}();

	s._canvas = _forceMainCanvas ? OBSWeakCanvas(mainCanvasWeak)
				     : _canvas->GetCanvas();

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
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(31, 1, 0)
	if (!weakCanvas) {
		return {};
	}
#endif

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
	if ((_current || _previous)) {
		const bool isMain = IsMainCanvas(canvas);
		const QStringList order = getOrderList(
			_current, _previous && isMain, _preview && isMain);
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

	_isPopulated = true;

	Resize();
}

SceneSelectionWidget::SceneSelectionWidget(QWidget *parent, bool variables,
					   bool sceneGroups, bool previous,
					   bool current, bool preview)
	: QWidget(parent),
	  _scenes(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectScene"))),
	  _canvas(new CanvasSelection(this)),
	  _current(current),
	  _previous(previous),
	  _preview(preview),
	  _variables(variables),
	  _sceneGroups(sceneGroups)
{
	_scenes->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_scenes->setDuplicatesEnabled(true);

	QWidget::connect(_scenes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));
	QWidget::connect(_canvas, SIGNAL(CanvasChanged(const OBSWeakCanvas &)),
			 this, SLOT(CanvasChangedSlot(const OBSWeakCanvas &)));
	QWidget::connect(_canvas, SIGNAL(CanvasChanged(const OBSWeakCanvas &)),
			 this, SIGNAL(CanvasChanged(const OBSWeakCanvas &)));

	auto settingsWindow = GetSettingsWindow();
	if (settingsWindow) {
		QWidget::connect(settingsWindow,
				 SIGNAL(SceneGroupAdded(const QString &)), this,
				 SLOT(ItemAdd(const QString &)));
		QWidget::connect(settingsWindow,
				 SIGNAL(SceneGroupRemoved(const QString &)),
				 this, SLOT(ItemRemove(const QString &)));
		QWidget::connect(
			settingsWindow,
			SIGNAL(SceneGroupRenamed(const QString &,
						 const QString &)),
			this,
			SLOT(ItemRename(const QString &, const QString &)));
	}

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
	setContentsMargins(0, 0, 0, 0);
	layout->setContentsMargins(0, 0, 0, 0);

	if (GetCanvasCount() <= 1) {
		_canvas->hide();
	}

	Resize();
}

void SceneSelectionWidget::SetScene(const SceneSelection &s)
{
	int idx = -1;

	_canvas->SetCanvas(s._canvas);
	if (!_isPopulated) {
		PopulateSceneSelection(s._canvas);
	}

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

void SceneSelectionWidget::LockToMainCanvas()
{
	_forceMainCanvas = true;
	_canvas->hide();
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

void SceneSelectionWidget::CanvasChangedSlot(const OBSWeakCanvas &)
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
