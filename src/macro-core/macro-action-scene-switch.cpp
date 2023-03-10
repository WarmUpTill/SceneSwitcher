#include "macro-action-scene-switch.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

using namespace std::chrono_literals;

const std::string MacroActionSwitchScene::id = "scene_switch";

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.switchScene"});

static void waitForTransitionChange(OBSWeakSource &transition,
				    std::unique_lock<std::mutex> *lock,
				    Macro *macro)
{
	const auto time = 100ms;
	obs_source_t *source = obs_weak_source_get_source(transition);

	bool stillTransitioning = true;
	while (stillTransitioning && !switcher->abortMacroWait &&
	       !macro->GetStop()) {
		switcher->macroTransitionCv.wait_for(*lock, time);
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

	while (!switcher->abortMacroWait && !macro->GetStop()) {
		if (switcher->macroTransitionCv.wait_until(*lock, time) ==
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

static int getExpectedTransitionDuration(OBSWeakSource &scene, OBSWeakSource &t,
					 double duration)
{
	OBSWeakSource transition = t;
	if (!switcher->transitionOverrideOverride) {
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
	switcher->abortMacroWait = false;

	std::unique_lock<std::mutex> lock(switcher->m);
	if (expectedTransitionDuration < 0) {
		waitForTransitionChange(transition, &lock, GetMacro());
	} else {
		waitForTransitionChangeFixedDuration(expectedTransitionDuration,
						     &lock, GetMacro());
	}

	return !switcher->abortMacroWait;
}

bool MacroActionSwitchScene::PerformAction()
{
	auto scene = _scene.GetScene();
	auto transition = _transition.GetTransition();
	switchScene({scene, transition, (int)(_duration.Milliseconds())},
		    obs_frontend_preview_program_mode_active());
	if (_blockUntilTransitionDone && scene) {
		return WaitForTransition(scene, transition);
	}
	return true;
}

void MacroActionSwitchScene::LogAction() const
{
	auto t = _scene.GetType();
	auto sceneName = GetWeakSourceName(_scene.GetScene(false));
	switch (t) {
	case SceneSelection::Type::SCENE:
		vblog(LOG_INFO, "switch to scene '%s'",
		      _scene.ToString(true).c_str());
		break;
	case SceneSelection::Type::GROUP:
		vblog(LOG_INFO, "switch to scene '%s' (scene group '%s')",
		      sceneName.c_str(), _scene.ToString(true).c_str());
		break;
	case SceneSelection::Type::PREVIOUS:
		vblog(LOG_INFO, "switch to previous scene '%s'",
		      sceneName.c_str());
		break;
	default:
		break;
	}
}

bool MacroActionSwitchScene::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_transition.Save(obj);
	_duration.Save(obj);
	obs_data_set_bool(obj, "blockUntilTransitionDone",
			  _blockUntilTransitionDone);
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
	return true;
}

std::string MacroActionSwitchScene::GetShortDesc() const
{
	return _scene.ToString();
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	QWidget *parent, std::shared_ptr<MacroActionSwitchScene> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, true, true)),
	  _transitions(new TransitionSelectionWidget(this)),
	  _duration(new DurationSelection(parent, false)),
	  _blockUntilTransitionDone(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.scene.blockUntilTransitionDone"))),
	  _entryLayout(new QHBoxLayout())
{
	_duration->SpinBox()->setSpecialValueText("-");

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

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{transitions}}", _transitions},
		{"{{duration}}", _duration},
		{"{{blockUntilTransitionDone}}", _blockUntilTransitionDone},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.scene.entry"),
		     _entryLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_entryLayout);
	mainLayout->addWidget(_blockUntilTransitionDone);
	setLayout(mainLayout);

	_entryData = entryData;
	_scenes->SetScene(_entryData->_scene);
	_transitions->SetTransition(_entryData->_transition);
	_duration->SetDuration(_entryData->_duration);
	_blockUntilTransitionDone->setChecked(
		_entryData->_blockUntilTransitionDone);
	SetDurationVisibility();
	_loading = false;
}

void MacroActionSwitchSceneEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = dur;
}

void MacroActionSwitchSceneEdit::BlockUntilTransitionDoneChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_blockUntilTransitionDone = state;
}

void MacroActionSwitchSceneEdit::SetDurationVisibility()
{
	if (_entryData->_transition.GetType() !=
	    TransitionSelection::Type::TRANSITION) {
		_duration->show();
	}
	const bool fixedDuration = isUsingFixedLengthTransition(
		_entryData->_transition.GetTransition());
	_duration->setVisible(!fixedDuration);

	_entryLayout->removeWidget(_scenes);
	_entryLayout->removeWidget(_transitions);
	_entryLayout->removeWidget(_duration);
	clearLayout(_entryLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{transitions}}", _transitions},
		{"{{duration}}", _duration},
	};
	if (fixedDuration) {
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.scene.entry.noDuration"),
			_entryLayout, widgetPlaceholders);
	} else {
		placeWidgets(
			obs_module_text("AdvSceneSwitcher.action.scene.entry"),
			_entryLayout, widgetPlaceholders);
	}
}

void MacroActionSwitchSceneEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSwitchSceneEdit::TransitionChanged(const TransitionSelection &t)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_transition = t;
	SetDurationVisibility();
}
