#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-transition.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionTransition::id = "transition";

bool MacroConditionTransition::_registered = MacroConditionFactory::Register(
	MacroConditionTransition::id,
	{MacroConditionTransition::Create, MacroConditionTransitionEdit::Create,
	 "AdvSceneSwitcher.condition.transition"});

static std::map<TransitionCondition, std::string> filterConditionTypes = {
	{TransitionCondition::CURRENT,
	 "AdvSceneSwitcher.condition.transition.type.current"},
	{TransitionCondition::DURATION,
	 "AdvSceneSwitcher.condition.transition.type.duration"},
	{TransitionCondition::STARTED,
	 "AdvSceneSwitcher.condition.transition.type.started"},
	{TransitionCondition::ENDED,
	 "AdvSceneSwitcher.condition.transition.type.ended"},
	{TransitionCondition::TRANSITION_SOURCE,
	 "AdvSceneSwitcher.condition.transition.type.transitionSource"},
	{TransitionCondition::TRANSITION_TARGET,
	 "AdvSceneSwitcher.condition.transition.type.transitionTarget"},
};

bool isCurrentTransition(OBSWeakSource &t)
{
	bool match;
	auto tSource = obs_frontend_get_current_transition();
	auto tWeakSource = obs_source_get_weak_source(tSource);
	match = t == tWeakSource;
	obs_weak_source_release(tWeakSource);
	obs_source_release(tSource);
	return match;
}

bool isTargetScene(OBSWeakSource &target)
{
	auto source = obs_frontend_get_current_scene();
	auto targetScene = obs_source_get_weak_source(source);
	bool ret = target == targetScene;
	obs_weak_source_release(targetScene);
	obs_source_release(source);
	return ret;
}

bool MacroConditionTransition::CheckCondition()
{
	bool anyTransitionEnded = _lastTransitionEndTime !=
				  switcher->lastTransitionEndTime;
	bool anyTransitionStarted = switcher->anySceneTransitionStarted();
	bool transitionStarted = false;
	bool transitionEnded = false;

	if (_transition.GetType() == TransitionSelectionType::ANY) {
		transitionStarted = anyTransitionStarted;
		transitionEnded = anyTransitionEnded;
	} else {
		transitionStarted = _started;
		transitionEnded = _ended;
	}

	bool ret = false;
	switch (_condition) {
	case TransitionCondition::CURRENT: {
		auto transition = _transition.GetTransition();
		ret = isCurrentTransition(transition);
		break;
	}
	case TransitionCondition::DURATION:
		ret = _duration.seconds * 1000 ==
		      obs_frontend_get_transition_duration();
		break;
	case TransitionCondition::STARTED:
		ret = transitionStarted;
		break;
	case TransitionCondition::ENDED:
		ret = transitionEnded;
		break;
	case TransitionCondition::TRANSITION_SOURCE:
		ret = anyTransitionStarted &&
		      _scene.GetScene() == switcher->currentScene;
		break;
	case TransitionCondition::TRANSITION_TARGET: {
		auto scene = _scene.GetScene();
		ret = anyTransitionStarted && isTargetScene(scene);
		break;
	}
	default:
		break;
	}

	// Reset for next interval
	if (_started) {
		_started = false;
	}
	if (_ended) {
		_ended = false;
	}
	if (anyTransitionEnded) {
		_lastTransitionEndTime = switcher->lastTransitionEndTime;
	}

	return ret;
}

void MacroConditionTransition::ConnectToTransitionSignals()
{
	auto source = obs_weak_source_get_source(_transition.GetTransition());
	auto sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "transition_start", TransitionStarted, this);
	signal_handler_connect(sh, "transition_stop", TransitionEnded, this);
	obs_source_release(source);
}

void MacroConditionTransition::DisconnectTransitionSignals()
{
	auto source = obs_weak_source_get_source(_transition.GetTransition());
	auto sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "transition_start", TransitionStarted,
				  this);
	signal_handler_disconnect(sh, "transition_stop", TransitionEnded, this);
	obs_source_release(source);
}

void MacroConditionTransition::TransitionStarted(void *data, calldata_t *)
{
	auto *transitionCond = static_cast<MacroConditionTransition *>(data);
	transitionCond->_started = true;
}

void MacroConditionTransition::TransitionEnded(void *data, calldata_t *)
{
	auto *transitionCond = static_cast<MacroConditionTransition *>(data);
	transitionCond->_ended = true;
}

bool MacroConditionTransition::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_transition.Save(obj);
	_scene.Save(obj);
	_duration.Save(obj);
	return true;
}

bool MacroConditionTransition::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<TransitionCondition>(
		obs_data_get_int(obj, "condition"));
	_transition.Load(obj);
	_scene.Load(obj);
	_duration.Load(obj);
	ConnectToTransitionSignals();
	return true;
}

std::string MacroConditionTransition::GetShortDesc()
{
	if (_condition == TransitionCondition::CURRENT ||
	    _condition == TransitionCondition::DURATION ||
	    _condition == TransitionCondition::STARTED ||
	    _condition == TransitionCondition::ENDED) {
		return _transition.ToString();
	}
	return "";
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : filterConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionTransitionEdit::MacroConditionTransitionEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTransition> entryData)
	: QWidget(parent)
{
	_conditions = new QComboBox();
	_transitions = new TransitionSelectionWidget(this, true, true);
	_scenes = new SceneSelectionWidget(this, false, true, true);
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
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{transitions}}", _transitions},
		{"{{scenes}}", _scenes},
		{"{{duration}}", _duration},
		{"{{durationSuffix}}", _durationSuffix},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.transition.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

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
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_condition =
			static_cast<TransitionCondition>(index);
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->DisconnectTransitionSignals();
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroConditionTransitionEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionTransitionEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_transitions->setVisible(
		_entryData->_condition == TransitionCondition::CURRENT ||
		_entryData->_condition == TransitionCondition::STARTED ||
		_entryData->_condition == TransitionCondition::ENDED);
	_scenes->setVisible(_entryData->_condition ==
				    TransitionCondition::TRANSITION_SOURCE ||
			    _entryData->_condition ==
				    TransitionCondition::TRANSITION_TARGET);
	_duration->setVisible(_entryData->_condition ==
			      TransitionCondition::DURATION);
	_durationSuffix->setVisible(_entryData->_condition ==
				    TransitionCondition::DURATION);

	bool addCurrent = _entryData->_condition ==
			  TransitionCondition::DURATION;
	bool addAny = _entryData->_condition == TransitionCondition::STARTED ||
		      _entryData->_condition == TransitionCondition::ENDED;
	_transitions->Repopulate(addCurrent, addAny);
}

void MacroConditionTransitionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	SetWidgetVisibility();
	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_transitions->SetTransition(_entryData->_transition);
	_scenes->SetScene(_entryData->_scene);
	_duration->SetDuration(_entryData->_duration);
}
