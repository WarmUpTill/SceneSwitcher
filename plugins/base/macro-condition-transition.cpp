#include "macro-condition-transition.hpp"
#include "layout-helpers.hpp"
#include "scene-switch-helpers.hpp"

namespace advss {

const std::string MacroConditionTransition::id = "transition";

bool MacroConditionTransition::_registered = MacroConditionFactory::Register(
	MacroConditionTransition::id,
	{MacroConditionTransition::Create, MacroConditionTransitionEdit::Create,
	 "AdvSceneSwitcher.condition.transition"});

const static std::map<MacroConditionTransition::Condition, std::string>
	conditionTypes = {
		{MacroConditionTransition::Condition::CURRENT,
		 "AdvSceneSwitcher.condition.transition.type.current"},
		{MacroConditionTransition::Condition::DURATION,
		 "AdvSceneSwitcher.condition.transition.type.duration"},
		{MacroConditionTransition::Condition::STARTED,
		 "AdvSceneSwitcher.condition.transition.type.started"},
		{MacroConditionTransition::Condition::ENDED,
		 "AdvSceneSwitcher.condition.transition.type.ended"},
		{MacroConditionTransition::Condition::VIDEO_ENDED,
		 "AdvSceneSwitcher.condition.transition.type.videoEnded"},
		{MacroConditionTransition::Condition::TRANSITION_SOURCE,
		 "AdvSceneSwitcher.condition.transition.type.transitionSource"},
		{MacroConditionTransition::Condition::TRANSITION_TARGET,
		 "AdvSceneSwitcher.condition.transition.type.transitionTarget"},
};

static bool isCurrentTransition(OBSWeakSource &transition)
{
	OBSSourceAutoRelease source = obs_frontend_get_current_transition();
	OBSWeakSourceAutoRelease weakSource =
		obs_source_get_weak_source(source);
	return transition == weakSource;
}

static bool isTargetScene(OBSWeakSource &targetScene)
{
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	OBSWeakSourceAutoRelease scene = obs_source_get_weak_source(source);
	return targetScene == scene;
}

MacroConditionTransition::MacroConditionTransition(Macro *m) : MacroCondition(m)
{
	obs_frontend_add_event_callback(HandleFrontendEvent, this);
}

MacroConditionTransition::~MacroConditionTransition()
{
	obs_frontend_remove_event_callback(HandleFrontendEvent, this);
}

static bool sceneListContainsScene(const std::vector<OBSWeakSource> &scenes,
				   const OBSWeakSource &sceneToCheck)
{
	for (const auto &scene : scenes) {
		if (scene == sceneToCheck) {
			return true;
		}
	}
	return false;
}

bool MacroConditionTransition::CheckCondition()
{
	std::lock_guard<std::mutex> lock(_mutex);

	bool ret = false;
	switch (_condition) {
	case Condition::CURRENT: {
		auto transition = _transition.GetTransition();
		ret = isCurrentTransition(transition);
		break;
	}
	case Condition::DURATION:
		ret = _duration.Milliseconds() ==
		      obs_frontend_get_transition_duration();
		break;
	case Condition::STARTED:
		ret = _started;
		break;
	case Condition::ENDED:
		ret = _ended;
		break;
	case Condition::VIDEO_ENDED:
		ret = _videoEnded;
		break;
	case Condition::TRANSITION_SOURCE:
		ret = sceneListContainsScene(_transitionStartScenes,
					     _scene.GetScene());
		break;
	case Condition::TRANSITION_TARGET:
		ret = sceneListContainsScene(_transitionEndScenes,
					     _scene.GetScene());
		break;
	default:
		break;
	}

	// Reset for next interval
	_started = false;
	_ended = false;
	_videoEnded = false;
	_transitionStartScenes.clear();
	_transitionEndScenes.clear();

	return ret;
}

void advss::MacroConditionTransition::AddTransitionSignals(
	obs_source_t *transition)
{
	signal_handler_t *sh = obs_source_get_signal_handler(transition);
	_signals.emplace_back(sh, "transition_start", TransitionStarted, this);
	_signals.emplace_back(sh, "transition_stop", TransitionEnded, this);
	_signals.emplace_back(sh, "transition_video_stop", TransitionVideoEnded,
			      this);
}

void MacroConditionTransition::ConnectToTransitionSignals()
{
	_signals.clear();

	const bool watchSingleTransitionType =
		_transition.GetType() ==
			TransitionSelection::Type::TRANSITION &&
		!(_condition == Condition::TRANSITION_SOURCE ||
		  _condition == Condition::TRANSITION_TARGET);

	if (watchSingleTransitionType) {
		OBSSourceAutoRelease transition =
			OBSGetStrongRef(_transition.GetTransition());
		AddTransitionSignals(transition);
		return;
	}

	obs_frontend_source_list transitions = {};
	obs_frontend_get_transitions(&transitions);
	for (size_t i = 0; i < transitions.sources.num; i++) {
		obs_source_t *transition = transitions.sources.array[i];
		AddTransitionSignals(transition);
	}
	obs_frontend_source_list_free(&transitions);
}

void MacroConditionTransition::TransitionStarted(void *data, calldata_t *cd)
{
	auto *condition = static_cast<MacroConditionTransition *>(data);
	if (!condition) {
		return;
	}

	auto transitionSource = (obs_source_t *)calldata_ptr(cd, "source");

	OBSSourceAutoRelease startSource = obs_transition_get_source(
		transitionSource, OBS_TRANSITION_SOURCE_A);
	OBSSourceAutoRelease endSource =
		obs_frontend_preview_program_mode_active()
			? obs_frontend_get_current_scene()
			: obs_transition_get_source(transitionSource,
						    OBS_TRANSITION_SOURCE_B);

	std::lock_guard<std::mutex> lock(condition->_mutex);
	condition->_started = true;
	condition->_transitionStartScenes.emplace_back(
		OBSGetWeakRef(startSource));
	condition->_transitionEndScenes.emplace_back(OBSGetWeakRef(endSource));
}

void MacroConditionTransition::TransitionEnded(void *data, calldata_t *)
{
	auto *condition = static_cast<MacroConditionTransition *>(data);
	if (!condition) {
		return;
	}

	std::lock_guard<std::mutex> lock(condition->_mutex);
	condition->_ended = true;
}

void MacroConditionTransition::TransitionVideoEnded(void *data, calldata_t *)
{
	auto *condition = static_cast<MacroConditionTransition *>(data);
	if (!condition) {
		return;
	}

	std::lock_guard<std::mutex> lock(condition->_mutex);
	condition->_videoEnded = true;
}

void MacroConditionTransition::HandleFrontendEvent(obs_frontend_event event,
						   void *data)
{
	auto condition = reinterpret_cast<MacroConditionTransition *>(data);
	if (!condition) {
		return;
	}

	switch (event) {
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
		condition->ConnectToTransitionSignals();
		break;
	default:
		break;
	};
}

bool MacroConditionTransition::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_transition.Save(obj);
	_scene.Save(obj);
	_duration.Save(obj);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionTransition::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<MacroConditionTransition::Condition>(
		obs_data_get_int(obj, "condition"));

	_transition.Load(obj);
	_scene.Load(obj);
	_duration.Load(obj);

	// Backwards compatibility
	if (obs_data_get_int(obj, "version") < 1) {
		enum class TransitionCondition {
			CURRENT,
			DURATION,
			STARTED,
			ENDED,
			TRANSITION_SOURCE,
			TRANSITION_TARGET,
		};
		TransitionCondition oldValue = static_cast<TransitionCondition>(
			obs_data_get_int(obj, "condition"));
		switch (oldValue) {
		case TransitionCondition::CURRENT:
			_condition = Condition::CURRENT;
			break;
		case TransitionCondition::DURATION:
			_condition = Condition::DURATION;
			break;
		case TransitionCondition::STARTED:
			_condition = Condition::STARTED;
			break;
		case TransitionCondition::ENDED:
			_condition = Condition::ENDED;
			break;
		case TransitionCondition::TRANSITION_SOURCE:
			_condition = Condition::TRANSITION_SOURCE;
			break;
		case TransitionCondition::TRANSITION_TARGET:
			_condition = Condition::TRANSITION_TARGET;
			break;
		default:
			break;
		}
	}

	ConnectToTransitionSignals();
	return true;
}

std::string MacroConditionTransition::GetShortDesc() const
{
	if (_condition == Condition::CURRENT ||
	    _condition == Condition::DURATION ||
	    _condition == Condition::STARTED ||
	    _condition == Condition::ENDED) {
		return _transition.ToString();
	}
	return "";
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[value, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

MacroConditionTransitionEdit::MacroConditionTransitionEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTransition> entryData)
	: QWidget(parent)
{
	_conditions = new QComboBox();
	_transitions = new TransitionSelectionWidget(this, true, true);
	_scenes = new SceneSelectionWidget(this, true, false, true, true);
	_duration = new DurationSelection(this, false);
	_durationSuffix = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.transition.durationSuffix"));

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_transitions,
			 SIGNAL(TransitionChanged(const TransitionSelection &)),
			 this,
			 SLOT(TransitionChanged(const TransitionSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.transition.entry"),
		layout,
		{{"{{conditions}}", _conditions},
		 {"{{transitions}}", _transitions},
		 {"{{scenes}}", _scenes},
		 {"{{duration}}", _duration},
		 {"{{durationSuffix}}", _durationSuffix}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionTransitionEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_condition =
			static_cast<MacroConditionTransition::Condition>(
				_conditions->itemData(index).toInt());
	}
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionTransitionEdit::TransitionChanged(
	const TransitionSelection &t)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_transition = t;
	_entryData->ConnectToTransitionSignals();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionTransitionEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = s;
}

void MacroConditionTransitionEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_duration = dur;
}

void MacroConditionTransitionEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_transitions->setVisible(
		_entryData->_condition ==
			MacroConditionTransition::Condition::CURRENT ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::STARTED ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::ENDED ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::VIDEO_ENDED);
	_scenes->setVisible(
		_entryData->_condition ==
			MacroConditionTransition::Condition::TRANSITION_SOURCE ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::TRANSITION_TARGET);
	_duration->setVisible(_entryData->_condition ==
			      MacroConditionTransition::Condition::DURATION);
	_durationSuffix->setVisible(
		_entryData->_condition ==
		MacroConditionTransition::Condition::DURATION);

	_transitions->EnableCurrentEntry(
		_entryData->_condition ==
		MacroConditionTransition::Condition::DURATION);
	_transitions->EnableAnyEntry(
		_entryData->_condition ==
			MacroConditionTransition::Condition::STARTED ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::ENDED ||
		_entryData->_condition ==
			MacroConditionTransition::Condition::VIDEO_ENDED);
}

void MacroConditionTransitionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	SetWidgetVisibility();

	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->_condition)));
	_transitions->SetTransition(_entryData->_transition);
	_scenes->SetScene(_entryData->_scene);
	_duration->SetDuration(_entryData->_duration);
}

} // namespace advss
