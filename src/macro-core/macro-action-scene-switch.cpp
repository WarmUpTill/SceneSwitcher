#include "macro-action-scene-switch.hpp"
#include "macro-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "scene-switch-helpers.hpp"
#include "utility.hpp"

namespace advss {

using namespace std::chrono_literals;

const std::string MacroActionSwitchScene::id = GetSceneSwitchActionId().data();

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.scene"});

const static std::map<MacroActionSwitchScene::SceneType, std::string>
	sceneTypes = {
		{MacroActionSwitchScene::SceneType::PROGRAM,
		 "AdvSceneSwitcher.action.scene.type.program"},
		{MacroActionSwitchScene::SceneType::PREVIEW,
		 "AdvSceneSwitcher.action.scene.type.preview"},
};

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
	auto scene = _scene.GetScene();

	if (_sceneType == SceneType::PREVIEW) {
		OBSSourceAutoRelease previewScneSource =
			obs_weak_source_get_source(scene);
		obs_frontend_set_current_preview_scene(previewScneSource);
		return true;
	}

	auto transition = _transition.GetTransition();
	SwitchScene({scene, transition, (int)(_duration.Milliseconds())},
		    obs_frontend_preview_program_mode_active());
	if (_blockUntilTransitionDone && scene && scene != GetCurrentScene()) {
		return WaitForTransition(scene, transition);
	}
	return true;
}

void MacroActionSwitchScene::LogAction() const
{
	vblog(LOG_INFO, "switch%s scene to '%s'",
	      _sceneType == SceneType::PREVIEW ? " preview" : "",
	      _scene.ToString(true).c_str());
}

bool MacroActionSwitchScene::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
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
	return _scene.ToString();
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (const auto &[_, name] : sceneTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionSwitchScene> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, true, true)),
	  _transitions(new TransitionSelectionWidget(this)),
	  _duration(new DurationSelection(parent, false)),
	  _blockUntilTransitionDone(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.scene.blockUntilTransitionDone"))),
	  _sceneTypes(new QComboBox()),
	  _entryLayout(new QHBoxLayout())
{
	_duration->SpinBox()->setSpecialValueText("-");
	populateTypeSelection(_sceneTypes);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_transitions,
			 SIGNAL(TransitionChanged(const TransitionSelection &)),
			 this,
			 SLOT(TransitionChanged(const TransitionSelection &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_blockUntilTransitionDone, SIGNAL(stateChanged(int)),
			 this, SLOT(BlockUntilTransitionDoneChanged(int)));
	QWidget::connect(_sceneTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SceneTypeChanged(int)));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.scene.entry"),
		     _entryLayout,
		     {{"{{scenes}}", _scenes},
		      {"{{transitions}}", _transitions},
		      {"{{duration}}", _duration},
		      {"{{sceneTypes}}", _sceneTypes}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_entryLayout);
	mainLayout->addWidget(_blockUntilTransitionDone);
	setLayout(mainLayout);

	_entryData = entryData;
	_sceneTypes->setCurrentIndex(static_cast<int>(_entryData->_sceneType));
	_scenes->SetScene(_entryData->_scene);
	_transitions->SetTransition(_entryData->_transition);
	_duration->SetDuration(_entryData->_duration);
	_blockUntilTransitionDone->setChecked(
		_entryData->_blockUntilTransitionDone);
	SetWidgetVisibility();
	_loading = false;
}

void MacroActionSwitchSceneEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_duration = dur;
}

void MacroActionSwitchSceneEdit::BlockUntilTransitionDoneChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_blockUntilTransitionDone = state;
}

void MacroActionSwitchSceneEdit::SceneTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_sceneType =
		static_cast<MacroActionSwitchScene::SceneType>(value);
	SetWidgetVisibility();
}

void MacroActionSwitchSceneEdit::SetWidgetVisibility()
{
	_entryLayout->removeWidget(_scenes);
	_entryLayout->removeWidget(_transitions);
	_entryLayout->removeWidget(_duration);
	_entryLayout->removeWidget(_sceneTypes);
	ClearLayout(_entryLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{transitions}}", _transitions},
		{"{{duration}}", _duration},
		{"{{sceneTypes}}", _sceneTypes},
	};

	if (_entryData->_sceneType ==
	    MacroActionSwitchScene::SceneType::PREVIEW) {
		_transitions->hide();
		_duration->hide();
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.scene.entry.preview"),
			_entryLayout, widgetPlaceholders);
		return;
	}

	_transitions->show();
	if (_entryData->_transition.GetType() !=
	    TransitionSelection::Type::TRANSITION) {
		_duration->show();
	}
	const bool fixedDuration = isUsingFixedLengthTransition(
		_entryData->_transition.GetTransition());
	_duration->setVisible(!fixedDuration);

	if (fixedDuration) {
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.scene.entry.noDuration"),
			_entryLayout, widgetPlaceholders);
	} else {
		PlaceWidgets(
			obs_module_text("AdvSceneSwitcher.action.scene.entry"),
			_entryLayout, widgetPlaceholders);
	}
}

void MacroActionSwitchSceneEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSwitchSceneEdit::TransitionChanged(const TransitionSelection &t)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_transition = t;
	SetWidgetVisibility();
}

} // namespace advss
