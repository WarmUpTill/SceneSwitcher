#include "macro-action-scene-switch.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "scene-switch-helpers.hpp"
#include "source-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

using namespace std::chrono_literals;

const std::string MacroActionSwitchScene::id =
	MacroAction::GetDefaultID().data();

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.scene"});

static void waitForTransitionChange(OBSWeakSource &transition,
				    std::unique_lock<std::mutex> *lock,
				    Macro *macro)
{
	const auto time = 100ms;
	obs_source_t *source = obs_weak_source_get_source(transition);
	if (!source) {
		return;
	}

	bool stillTransitioning = true;
	while (stillTransitioning && !MacroWaitShouldAbort() &&
	       !MacroIsStopped(macro)) {
		GetMacroTransitionCV().wait_for(*lock, time);
		float t = obs_transition_get_time(source);
		stillTransitioning = t < 1.0f && t > 0.0f;
	}
	obs_source_release(source);
}

static void waitForTransitionChangeFixedDuration(
	int duration, std::unique_lock<std::mutex> *lock, Macro *macro)
{
	duration += 200; // It seems to be necessary to add a small buffer
	auto time = std::chrono::high_resolution_clock::now() +
		    std::chrono::milliseconds(duration);

	while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
		if (GetMacroTransitionCV().wait_until(*lock, time) ==
		    std::cv_status::timeout) {
			break;
		}
	}
}

static int getTransitionOverrideDuration(OBSWeakSource &scene)
{
	int duration = 0;
	obs_source_t *source = obs_weak_source_get_source(scene);
	obs_data_t *data = obs_source_get_private_settings(source);
	auto name = obs_data_get_string(data, "transition");
	if (strlen(name) != 0) {
		duration = obs_data_get_int(data, "transition_duration");
	}
	obs_data_release(data);
	obs_source_release(source);
	return duration;
}

static bool isUsingFixedLengthTransition(const OBSWeakSource &transition)
{
	obs_source_t *source = obs_weak_source_get_source(transition);
	bool ret = obs_transition_fixed(source);
	obs_source_release(source);
	return ret;
}

static OBSWeakSource getOverrideTransition(OBSWeakSource &scene)
{
	OBSWeakSource transition;
	obs_source_t *source = obs_weak_source_get_source(scene);
	obs_data_t *data = obs_source_get_private_settings(source);
	transition = GetWeakTransitionByName(
		obs_data_get_string(data, "transition"));
	obs_data_release(data);
	obs_source_release(source);
	return transition;
}

static int getExpectedTransitionDuration(OBSWeakSource &scene,
					 OBSWeakSource &transiton_,
					 double duration)
{
	OBSWeakSource transition = transiton_;

	// If we are not modifying the transition override, we will have to
	// check, if a transition override is set by the user and use its
	// duration instead
	if (!ShouldModifyTransitionOverrides()) {
		auto overrideTransition = getOverrideTransition(scene);
		if (overrideTransition) {
			transition = overrideTransition;
			if (!isUsingFixedLengthTransition(transition)) {
				return getTransitionOverrideDuration(scene);
			}
		}
	}

	if (isUsingFixedLengthTransition(transition)) {
		return -1; // no API is available to access the fixed duration
	}
	if (duration != 0) {
		return duration * 1000;
	}
	return obs_frontend_get_transition_duration();
}

bool MacroActionSwitchScene::WaitForTransition(OBSWeakSource &scene,
					       OBSWeakSource &transition)
{
	const int expectedTransitionDuration = getExpectedTransitionDuration(
		scene, transition, _duration.Seconds());
	SetMacroAbortWait(false);

	std::unique_lock<std::mutex> lock(*GetMutex());
	if (expectedTransitionDuration < 0) {
		waitForTransitionChange(transition, &lock, GetMacro());
	} else {
		waitForTransitionChangeFixedDuration(expectedTransitionDuration,
						     &lock, GetMacro());
	}

	return !MacroWaitShouldAbort();
}

bool MacroActionSwitchScene::PerformAction()
{
	OBSWeakSource sceneToSwitchTo;
	OBSWeakCanvas canvasToSwitchFor;

	OBSCanvasAutoRelease mainCanvas = obs_get_main_canvas();
	OBSWeakCanvas mainCanvasWeak = OBSGetWeakRef(mainCanvas);

	const auto getActiveSceneHelper = [this](const OBSWeakCanvas &canvas) {
		OBSWeakSource activeScene;
		if (IsMainCanvas(canvas) && _sceneType == SceneType::PREVIEW) {
			OBSSourceAutoRelease previewSource =
				obs_frontend_get_current_preview_scene();
			return OBSGetWeakRef(previewSource);
		} else {
			return GetActiveCanvasScene(canvas);
		}
	};

	switch (_action) {
	case Action::SCENE_NAME:
		sceneToSwitchTo = _scene.GetScene();
		canvasToSwitchFor = _scene.GetCanvas();
		break;
	case Action::SCENE_LIST_NEXT: {
		canvasToSwitchFor = _canvas ? _canvas : mainCanvasWeak;
		const auto activeScene =
			getActiveSceneHelper(canvasToSwitchFor);
		const int currentIndex =
			GetIndexOfScene(canvasToSwitchFor, activeScene);
		sceneToSwitchTo =
			GetSceneAtIndex(canvasToSwitchFor, currentIndex + 1);
		break;
	}
	case Action::SCENE_LIST_PREVIOUS: {
		canvasToSwitchFor = _canvas ? _canvas : mainCanvasWeak;
		const auto activeScene =
			getActiveSceneHelper(canvasToSwitchFor);
		const int currentIndex =
			GetIndexOfScene(canvasToSwitchFor, activeScene);
		sceneToSwitchTo =
			GetSceneAtIndex(canvasToSwitchFor, currentIndex - 1);
		break;
	}
	case Action::SCENE_LIST_INDEX: {
		canvasToSwitchFor = _canvas ? _canvas : mainCanvasWeak;
		sceneToSwitchTo = GetSceneAtIndex(canvasToSwitchFor,
						  _index.GetValue() - 1);
		break;
	}
	default:
		break;
	}

	if (_sceneType == SceneType::PREVIEW) {
		OBSSourceAutoRelease previewSceneSource =
			obs_weak_source_get_source(sceneToSwitchTo);
		obs_frontend_set_current_preview_scene(previewSceneSource);
		return true;
	}

	auto transition = _transition.GetTransition();
	SwitchScene(
		{sceneToSwitchTo, transition, (int)(_duration.Milliseconds())},
		obs_frontend_preview_program_mode_active(), canvasToSwitchFor);
	if (_blockUntilTransitionDone && sceneToSwitchTo &&
	    sceneToSwitchTo != GetCurrentScene() &&
	    IsMainCanvas(canvasToSwitchFor)) {
		return WaitForTransition(sceneToSwitchTo, transition);
	}
	return true;
}

void MacroActionSwitchScene::LogAction() const
{
	if (!ActionLoggingEnabled()) {
		return;
	}

	switch (_action) {
	case Action::SCENE_NAME:
		blog(LOG_INFO, "switch %s scene to '%s'",
		     _sceneType == SceneType::PREVIEW ? " preview" : "program",
		     _scene.ToString(true).c_str());
		break;
	case Action::SCENE_LIST_NEXT:
		blog(LOG_INFO, "switch %s to next scene in scene list",
		     _sceneType == SceneType::PREVIEW ? " preview" : "program");
		break;
	case Action::SCENE_LIST_PREVIOUS:
		blog(LOG_INFO, "switch %s to previous scene in scene list",
		     _sceneType == SceneType::PREVIEW ? " preview" : "program");
		break;
	case Action::SCENE_LIST_INDEX:
		blog(LOG_INFO, "switch %s to scene at index %d in scene list",
		     _sceneType == SceneType::PREVIEW ? " preview" : "program",
		     _index.GetValue());
		break;
	default:
		break;
	}
}

bool MacroActionSwitchScene::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_string(obj, "canvas", GetWeakCanvasName(_canvas).c_str());
	_index.Save(obj, "index");
	_scene.Save(obj);
	_transition.Save(obj);
	_duration.Save(obj);
	obs_data_set_bool(obj, "blockUntilTransitionDone",
			  _blockUntilTransitionDone);
	obs_data_set_int(obj, "sceneType", static_cast<int>(_sceneType));
	return true;
}

bool MacroActionSwitchScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	if (obs_data_has_user_value(obj, "canvas")) {
		_canvas =
			GetWeakCanvasByName(obs_data_get_string(obj, "canvas"));
	} else {
		OBSCanvasAutoRelease main = obs_get_main_canvas();
		_canvas = obs_canvas_get_weak_canvas(main);
	}

	_index.Save(obj, "index");
	_scene.Load(obj);
	_transition.Load(obj);
	_duration.Load(obj);
	_blockUntilTransitionDone =
		obs_data_get_bool(obj, "blockUntilTransitionDone");
	_sceneType = static_cast<SceneType>(obs_data_get_int(obj, "sceneType"));
	return true;
}

std::string MacroActionSwitchScene::GetShortDesc() const
{
	return _action == Action::SCENE_NAME ? _scene.ToString() : "";
}

std::shared_ptr<MacroAction> MacroActionSwitchScene::Create(Macro *m)
{
	return std::make_shared<MacroActionSwitchScene>(m);
}

std::shared_ptr<MacroAction> MacroActionSwitchScene::Copy() const
{
	return std::make_shared<MacroActionSwitchScene>(*this);
}

void MacroActionSwitchScene::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_duration.ResolveVariables();
}

template<typename T>
static void populate(QComboBox *list, const std::map<T, std::string> &data)
{
	for (const auto &[value, name] : data) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static inline void populateTypeSelection(QComboBox *list)
{
	static const std::map<MacroActionSwitchScene::SceneType, std::string>
		types = {
			{MacroActionSwitchScene::SceneType::PROGRAM,
			 "AdvSceneSwitcher.action.scene.type.program"},
			{MacroActionSwitchScene::SceneType::PREVIEW,
			 "AdvSceneSwitcher.action.scene.type.preview"},
		};

	populate(list, types);
}

static inline void populateActionSelection(QComboBox *list)
{
	static const std::map<MacroActionSwitchScene::Action, std::string>
		actions = {
			{MacroActionSwitchScene::Action::SCENE_NAME,
			 "AdvSceneSwitcher.action.scene.action.fixed"},
			{MacroActionSwitchScene::Action::SCENE_LIST_NEXT,
			 "AdvSceneSwitcher.action.scene.action.sceneListNext"},
			{MacroActionSwitchScene::Action::SCENE_LIST_PREVIOUS,
			 "AdvSceneSwitcher.action.scene.action.sceneListPrevious"},
			{MacroActionSwitchScene::Action::SCENE_LIST_INDEX,
			 "AdvSceneSwitcher.action.scene.action.sceneListIndex"},
		};

	populate(list, actions);
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionSwitchScene> entryData)
	: QWidget(parent),
	  _actions(new QComboBox(this)),
	  _index(new VariableSpinBox(this)),
	  _scenes(new SceneSelectionWidget(this, true, true, true)),
	  _transitions(new TransitionSelectionWidget(this)),
	  _duration(new DurationSelection(this, false)),
	  _blockUntilTransitionDone(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.scene.blockUntilTransitionDone"))),
	  _sceneTypes(new QComboBox(this)),
	  _canvas(new CanvasSelection(this)),
	  _sceneNameAtIndex(new QLabel(this)),
	  _updateSceneNameTimer(new QTimer(this)),
	  _actionLayout(new QHBoxLayout()),
	  _transitionLayout(new QHBoxLayout()),
	  _notSupportedWarning(new QLabel(this))
{
	_index->setMinimum(1);
	_index->setMaximum(999);

	_duration->SpinBox()->setSpecialValueText("-");

	populateTypeSelection(_sceneTypes);
	populateActionSelection(_actions);

	SetupWidgetConnections();

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_actionLayout);
	mainLayout->addLayout(_transitionLayout);
	mainLayout->addWidget(_blockUntilTransitionDone);
	mainLayout->addWidget(_notSupportedWarning);
	setLayout(mainLayout);

	_entryData = entryData;
	SetWidgetData();

	SetWidgetVisibility();
	_loading = false;
}

void MacroActionSwitchSceneEdit::SetupWidgetConnections() const
{
	connect(_actions, SIGNAL(currentIndexChanged(int)), this,
		SLOT(ActionChanged(int)));
	connect(_sceneTypes, SIGNAL(currentIndexChanged(int)), this,
		SLOT(SceneTypeChanged(int)));
	connect(_canvas, SIGNAL(CanvasChanged(const OBSWeakCanvas &)), this,
		SLOT(CanvasChanged(const OBSWeakCanvas &)));
	connect(_index,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(IndexChanged(const NumberVariable<int> &)));
	connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)), this,
		SLOT(SceneChanged(const SceneSelection &)));
	connect(_scenes, SIGNAL(CanvasChanged(const OBSWeakCanvas &)), this,
		SLOT(SceneSelectionCanvasChanged(const OBSWeakCanvas &)));
	connect(_transitions,
		SIGNAL(TransitionChanged(const TransitionSelection &)), this,
		SLOT(TransitionChanged(const TransitionSelection &)));
	connect(_duration, SIGNAL(DurationChanged(const Duration &)), this,
		SLOT(DurationChanged(const Duration &)));
	connect(_blockUntilTransitionDone, SIGNAL(stateChanged(int)), this,
		SLOT(BlockUntilTransitionDoneChanged(int)));
	connect(_updateSceneNameTimer, SIGNAL(timeout()), this,
		SLOT(UpdateSceneNameAtIndex()));

	_updateSceneNameTimer->setInterval(300);
	_updateSceneNameTimer->start();
}

void MacroActionSwitchSceneEdit::SetWidgetData() const
{
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_index->SetValue(_entryData->_index);
	_sceneTypes->setCurrentIndex(_sceneTypes->findData(
		static_cast<int>(_entryData->_sceneType)));
	_scenes->SetScene(_entryData->_scene);
	_canvas->SetCanvas(_entryData->_canvas);
	_transitions->SetTransition(_entryData->_transition);
	_duration->SetDuration(_entryData->_duration);
	_blockUntilTransitionDone->setChecked(
		_entryData->_blockUntilTransitionDone);
}

void MacroActionSwitchSceneEdit::DurationChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = dur;
}

void MacroActionSwitchSceneEdit::BlockUntilTransitionDoneChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_blockUntilTransitionDone = state;
}

void MacroActionSwitchSceneEdit::SceneTypeChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_sceneType = static_cast<MacroActionSwitchScene::SceneType>(
		_sceneTypes->currentData().toInt());
	SetWidgetVisibility();
}

void MacroActionSwitchSceneEdit::CanvasChanged(const OBSWeakCanvas &canvas)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_canvas = canvas;
	SetWidgetVisibility();
}

void MacroActionSwitchSceneEdit::SceneSelectionCanvasChanged(
	const OBSWeakCanvas &)
{
	SetWidgetVisibility();
}

void MacroActionSwitchSceneEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSwitchScene::Action>(
		_actions->currentData().toInt());
	SetWidgetVisibility();
}

void MacroActionSwitchSceneEdit::IndexChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_index = value;
}

void MacroActionSwitchSceneEdit::UpdateSceneNameAtIndex()
{
	if (!_entryData) {
		return;
	}

	auto weakSource =
		GetSceneAtIndex(_entryData->_canvas, _entryData->_index - 1);
	if (!weakSource) {
		_sceneNameAtIndex->setText("");
		return;
	}

	OBSSourceAutoRelease source = OBSGetStrongRef(weakSource);
	auto name = obs_source_get_name(source);
	if (!name) {
		_sceneNameAtIndex->setText("");
		return;
	}

	const QString text = QString("(") + name + ")";
	_sceneNameAtIndex->setText(text);
}

static bool
shouldShowDurationInTransitionLayout(MacroActionSwitchScene::SceneType type,
				     const TransitionSelection &transition)
{
	if (type == MacroActionSwitchScene::SceneType::PREVIEW) {
		return false;
	}

	if (transition.GetType() != TransitionSelection::Type::TRANSITION) {
		return true;
	}

	if (isUsingFixedLengthTransition(transition.GetTransition())) {
		return false;
	}

	return true;
}

void MacroActionSwitchSceneEdit::SetWidgetVisibility()
{
	_actionLayout->removeWidget(_sceneTypes);
	_actionLayout->removeWidget(_actions);
	_actionLayout->removeWidget(_scenes);
	_actionLayout->removeWidget(_canvas);
	_actionLayout->removeWidget(_index);
	_actionLayout->removeWidget(_sceneNameAtIndex);
	ClearLayout(_actionLayout);

	const int canvasCount = GetCanvasCount();
	const auto &action = _entryData->_action;

	_scenes->setVisible(action ==
			    MacroActionSwitchScene::Action::SCENE_NAME);
	_canvas->setVisible(
		action != MacroActionSwitchScene::Action::SCENE_NAME &&
		canvasCount > 1);
	_index->setVisible(action ==
			   MacroActionSwitchScene::Action::SCENE_LIST_INDEX);
	_sceneNameAtIndex->setVisible(
		action == MacroActionSwitchScene::Action::SCENE_LIST_INDEX);

	const std::unordered_map<std::string, QWidget *> actionWidgets = {
		{"{{actions}}", _actions},
		{"{{sceneTypes}}", _sceneTypes},
		{"{{scenes}}", _scenes},
		{"{{canvas}}", _canvas},
		{"{{index}}", _index},
		{"{{sceneNameAtIndex}}", _sceneNameAtIndex}};

	const char *layoutString = "";
	switch (action) {
	case MacroActionSwitchScene::Action::SCENE_NAME:
		layoutString =
			"AdvSceneSwitcher.action.scene.layout.action.fixed";
		break;
	case MacroActionSwitchScene::Action::SCENE_LIST_NEXT:
	case MacroActionSwitchScene::Action::SCENE_LIST_PREVIOUS:
		layoutString =
			(canvasCount > 1)
				? "AdvSceneSwitcher.action.scene.layout.action.list"
				: "AdvSceneSwitcher.action.scene.layout.action.list.noCanvasSelection";
		break;
	case MacroActionSwitchScene::Action::SCENE_LIST_INDEX:
		layoutString =
			(canvasCount > 1)
				? "AdvSceneSwitcher.action.scene.layout.action.listIndex"
				: "AdvSceneSwitcher.action.scene.layout.action.listIndex.noCanvasSelection";
		break;
	default:
		break;
	}

	PlaceWidgets(obs_module_text(layoutString), _actionLayout,
		     actionWidgets);

	_transitionLayout->removeWidget(_transitions);
	_transitionLayout->removeWidget(_duration);
	ClearLayout(_transitionLayout);

	const bool shouldShowDuration = shouldShowDurationInTransitionLayout(
		_entryData->_sceneType, _entryData->_transition);
	_duration->setVisible(shouldShowDuration);

	layoutString =
		shouldShowDuration
			? "AdvSceneSwitcher.action.scene.layout.transitionWithDuration"
			: "AdvSceneSwitcher.action.scene.layout.transition";
	PlaceWidgets(obs_module_text(layoutString), _transitionLayout,
		     {{"{{transitions}}", _transitions},
		      {"{{duration}}", _duration}});

	OBSWeakCanvas canvas;
	if (action == MacroActionSwitchScene::Action::SCENE_NAME) {
		canvas = _entryData->_scene.GetCanvas();
	} else {
		if (_entryData->_canvas) {
			canvas = _entryData->_canvas;
		} else {
			OBSCanvasAutoRelease mainCanvas = obs_get_main_canvas();
			canvas = OBSGetWeakRef(mainCanvas);
		}
	}
	const bool isMainCanvas = IsMainCanvas(canvas);
	SetLayoutVisible(_transitionLayout, isMainCanvas);
	_blockUntilTransitionDone->setVisible(isMainCanvas);

	_scenes->setVisible(action ==
			    MacroActionSwitchScene::Action::SCENE_NAME);
	_index->setVisible(action ==
			   MacroActionSwitchScene::Action::SCENE_LIST_INDEX);
	_sceneNameAtIndex->setVisible(
		action == MacroActionSwitchScene::Action::SCENE_LIST_INDEX);

	const QString fmt = obs_module_text(
		"AdvSceneSwitcher.action.scene.canvasNotSupported");
	_notSupportedWarning->setText(
		fmt.arg(QString::fromStdString(GetWeakCanvasName(canvas))));
	_notSupportedWarning->setVisible(
		!isMainCanvas &&
		_entryData->_sceneType ==
			MacroActionSwitchScene::SceneType::PREVIEW);

	adjustSize();
	updateGeometry();
}

void MacroActionSwitchSceneEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSwitchSceneEdit::TransitionChanged(const TransitionSelection &t)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_transition = t;
	SetWidgetVisibility();
}

} // namespace advss
