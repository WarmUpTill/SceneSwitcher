#include "macro-action-scene-visibility.hpp"
#include "layout-helpers.hpp"
#include "transition-helpers.hpp"

namespace advss {

const std::string MacroActionSceneVisibility::id = "scene_visibility";

bool MacroActionSceneVisibility::_registered = MacroActionFactory::Register(
	MacroActionSceneVisibility::id,
	{MacroActionSceneVisibility::Create,
	 MacroActionSceneVisibilityEdit::Create,
	 "AdvSceneSwitcher.action.sceneVisibility"});

const static std::map<MacroActionSceneVisibility::Action, std::string>
	actionTypes = {
		{MacroActionSceneVisibility::Action::SHOW,
		 "AdvSceneSwitcher.action.sceneVisibility.type.show"},
		{MacroActionSceneVisibility::Action::HIDE,
		 "AdvSceneSwitcher.action.sceneVisibility.type.hide"},
		{MacroActionSceneVisibility::Action::TOGGLE,
		 "AdvSceneSwitcher.action.sceneVisibility.type.toggle"},
};

namespace {

struct TransitionRestoreContext {
	obs_sceneitem_t *item;
	bool wasVisible;

	bool restoreTransition;
	OBSSource originalTransition;
	bool restoreDuration;
	uint32_t originalDuration;

	signal_handler_t *sh = nullptr;

	~TransitionRestoreContext()
	{
		if (!sh) {
			return;
		}

		signal_handler_disconnect(
			sh, "transition_stop",
			&TransitionRestoreContext::ResetSceneItemTransition,
			this);
	}

	static void ResetSceneItemTransition(void *param, calldata_t *)
	{
		auto *ctx = static_cast<TransitionRestoreContext *>(param);
		SetSceneItemTransition(ctx->item, ctx->originalTransition,
				       !ctx->wasVisible);

		obs_sceneitem_set_transition_duration(
			ctx->item, !ctx->wasVisible, ctx->originalDuration);
		delete ctx;
	}
};

} // namespace

static void setSceneItemVisibility(obs_sceneitem_t *item,
				   const bool setTransition,
				   const OBSWeakSource &transitionWeak,
				   const bool setDuration,
				   const Duration &duration,
				   MacroActionSceneVisibility::Action action)
{
	const OBSSourceAutoRelease transition = OBSGetStrongRef(transitionWeak);
	const bool itemIsVisible = obs_sceneitem_visible(item);

	const OBSSource currentTransition =
		obs_sceneitem_get_transition(item, !itemIsVisible);
	const uint32_t currentTransitionDuration =
		obs_sceneitem_get_transition_duration(item, !itemIsVisible);

	obs_source_t *privateTransitionSource = nullptr;
	if (setTransition) {
		privateTransitionSource = SetSceneItemTransition(
			item, transition, !itemIsVisible);
	} else {
		privateTransitionSource =
			obs_sceneitem_get_transition(item, !itemIsVisible);
	}

	if (setDuration) {
		obs_sceneitem_set_transition_duration(item, !itemIsVisible,
						      duration.Milliseconds());
	}

	switch (action) {
	case MacroActionSceneVisibility::Action::SHOW:
		obs_sceneitem_set_visible(item, true);
		break;
	case MacroActionSceneVisibility::Action::HIDE:
		obs_sceneitem_set_visible(item, false);
		break;
	case MacroActionSceneVisibility::Action::TOGGLE:
		obs_sceneitem_set_visible(item, !itemIsVisible);
		break;
	}

	if (!privateTransitionSource) {
		if (setTransition) {
			SetSceneItemTransition(item, currentTransition,
					       !itemIsVisible);
		}
		if (setDuration) {
			obs_sceneitem_set_transition_duration(
				item, !itemIsVisible,
				currentTransitionDuration);
		}
		return;
	}

	auto sh = obs_source_get_signal_handler(privateTransitionSource);
	if (!sh) {
		return;
	}

	auto ctx = new TransitionRestoreContext{item,
						itemIsVisible,
						setTransition,
						currentTransition,
						setDuration,
						currentTransitionDuration,
						sh};

	signal_handler_connect(
		sh, "transition_stop",
		&TransitionRestoreContext::ResetSceneItemTransition, ctx);
}

bool MacroActionSceneVisibility::PerformAction()
{
	auto items = _source.GetSceneItems(_scene);
	for (const auto &item : items) {
		setSceneItemVisibility(item, _updateTransition,
				       _transition.GetTransition(),
				       _updateDuration, _duration, _action);
	}
	return true;
}

void MacroActionSceneVisibility::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO,
		      "performed visibility action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), _source.ToString(true).c_str(),
		      _scene.ToString(true).c_str());

	} else {
		blog(LOG_WARNING, "ignored unknown SceneVisibility action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneVisibility::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_bool(obj, "updateTransition", _updateTransition);
	_transition.Save(obj);
	obs_data_set_bool(obj, "updateDuration", _updateDuration);
	_duration.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionSceneVisibility::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_updateTransition = obs_data_get_bool(obj, "updateTransition");
	_transition.Load(obj);
	_updateDuration = obs_data_get_bool(obj, "updateDuration");
	_duration.Load(obj);
	_action = static_cast<MacroActionSceneVisibility::Action>(
		obs_data_get_int(obj, "action"));

	// TODO: Remove in future version
	if (obs_data_get_int(obj, "sourceType") != 0) {
		auto sourceGroup = obs_data_get_string(obj, "sourceGroup");
		_source.SetSourceTypeSelection(sourceGroup);
	}
	return true;
}

std::string MacroActionSceneVisibility::GetShortDesc() const
{
	if (!_source.ToString().empty()) {
		return _scene.ToString() + " - " + _source.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionSceneVisibility::Create(Macro *m)
{
	return std::make_shared<MacroActionSceneVisibility>(m);
}

std::shared_ptr<MacroAction> MacroActionSceneVisibility::Copy() const
{
	return std::make_shared<MacroActionSceneVisibility>(*this);
}

void MacroActionSceneVisibility::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_source.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneVisibilityEdit::MacroActionSceneVisibilityEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneVisibility> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true)),
	  _sources(new SceneItemSelectionWidget(
		  parent,
		  {
			  SceneItemSelection::Type::SOURCE_NAME,
			  SceneItemSelection::Type::VARIABLE_NAME,
			  SceneItemSelection::Type::SOURCE_NAME_PATTERN,
			  SceneItemSelection::Type::SOURCE_GROUP,
			  SceneItemSelection::Type::SOURCE_TYPE,
			  SceneItemSelection::Type::INDEX,
			  SceneItemSelection::Type::INDEX_RANGE,
			  SceneItemSelection::Type::ALL,
		  },
		  SceneItemSelectionWidget::NameClashMode::ALL)),
	  _updateTransition(new QCheckBox(this)),
	  _transitions(new TransitionSelectionWidget(this, false, false)),
	  _updateDuration(new QCheckBox(this)),
	  _duration(new DurationSelection(this, false)),
	  _durationLayout(new QHBoxLayout),
	  _actions(new QComboBox())
{
	populateActionSelection(_actions);

	_duration->SpinBox()->setSpecialValueText("-");

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_updateTransition, SIGNAL(stateChanged(int)), this,
			 SLOT(UpdateTransitionChanged(int)));
	QWidget::connect(_transitions,
			 SIGNAL(TransitionChanged(const TransitionSelection &)),
			 this,
			 SLOT(TransitionChanged(const TransitionSelection &)));
	QWidget::connect(_updateDuration, SIGNAL(stateChanged(int)), this,
			 SLOT(UpdateDurationChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	auto sceneItemLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.sceneVisibility.layout"),
		     sceneItemLayout,
		     {{"{{scenes}}", _scenes},
		      {"{{sources}}", _sources},
		      {"{{actions}}", _actions}});

	auto transitionLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.sceneVisibility.layout.transition"),
		transitionLayout,
		{{"{{updateTransition}}", _updateTransition},
		 {"{{transitions}}", _transitions}});

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.sceneVisibility.layout.duration"),
		_durationLayout,
		{{"{{updateDuration}}", _updateDuration},
		 {"{{duration}}", _duration}});

	auto layout = new QVBoxLayout;
	layout->addLayout(sceneItemLayout);
	layout->addLayout(transitionLayout);
	layout->addLayout(_durationLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	SetWidgetVisibility();
	_loading = false;
}

void MacroActionSceneVisibilityEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem(_entryData->_source);
	_updateTransition->setChecked(_entryData->_updateTransition);
	_transitions->SetTransition(_entryData->_transition);
	_updateDuration->setChecked(_entryData->_updateDuration);
	_duration->SetDuration(_entryData->_duration);
}

void MacroActionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
}

void MacroActionSceneVisibilityEdit::SourceChanged(
	const SceneItemSelection &item)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionSceneVisibilityEdit::UpdateTransitionChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_updateTransition = state;
	SetWidgetVisibility();
}

void MacroActionSceneVisibilityEdit::TransitionChanged(
	const TransitionSelection &t)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_transition = t;
	SetWidgetVisibility();
}

void MacroActionSceneVisibilityEdit::UpdateDurationChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_updateDuration = state;
	SetWidgetVisibility();
}

void MacroActionSceneVisibilityEdit::DurationChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = dur;
}

void MacroActionSceneVisibilityEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action =
		static_cast<MacroActionSceneVisibility::Action>(value);
}

void MacroActionSceneVisibilityEdit::SetWidgetVisibility()
{
	const bool hideDurationSelection =
		_entryData->_updateTransition &&
		IsFixedLengthTransition(
			_entryData->_transition.GetTransition());

	SetLayoutVisible(_durationLayout, !hideDurationSelection);

	_transitions->setEnabled(_entryData->_updateTransition);
	_duration->setEnabled(_entryData->_updateDuration);

	adjustSize();
	updateGeometry();
}

} // namespace advss
