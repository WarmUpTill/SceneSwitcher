#include "macro-action-transition.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

const std::string MacroActionTransition::id = "transition";

bool MacroActionTransition::_registered = MacroActionFactory::Register(
	MacroActionTransition::id,
	{MacroActionTransition::Create, MacroActionTransitionEdit::Create,
	 "AdvSceneSwitcher.action.transition"});

const static std::map<MacroActionTransition::Type, std::string> actionTypes = {
	{MacroActionTransition::Type::SCENE,
	 "AdvSceneSwitcher.action.transition.type.scene"},
	{MacroActionTransition::Type::SCENE_OVERRIDE,
	 "AdvSceneSwitcher.action.transition.type.sceneOverride"},
	{MacroActionTransition::Type::SOURCE_SHOW,
	 "AdvSceneSwitcher.action.transition.type.sourceShow"},
	{MacroActionTransition::Type::SOURCE_HIDE,
	 "AdvSceneSwitcher.action.transition.type.sourceHide"},
};

void MacroActionTransition::SetSceneTransition()
{
	if (_setTransitionType) {
		OBSSourceAutoRelease transition =
			obs_weak_source_get_source(_transition.GetTransition());
		obs_frontend_set_current_transition(transition);
	}
	if (_setDuration) {
		obs_frontend_set_transition_duration(_duration.Seconds() *
						     1000);
	}
}

void MacroActionTransition::SetTransitionOverride()
{
	OBSSourceAutoRelease scene =
		obs_weak_source_get_source(_scene.GetScene());
	OBSDataAutoRelease data = obs_source_get_private_settings(scene);
	if (_setTransitionType) {
		obs_data_set_string(data, "transition",
				    _transition.ToString().c_str());
	}
	if (_setDuration) {
		obs_data_set_int(data, "transition_duration",
				 _duration.Milliseconds());
	}
}

#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(27, 0, 0)) && \
	(LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(28, 0, 0))
static void obs_sceneitem_set_transition(obs_sceneitem_t *item, bool show,
					 obs_source_t *transition)
{
	if (show) {
		obs_sceneitem_set_show_transition(item, transition);
	} else {
		obs_sceneitem_set_hide_transition(item, transition);
	}
}

static void obs_sceneitem_set_transition_duration(obs_sceneitem_t *item,
						  bool show,
						  uint32_t duration_ms)
{
	if (show) {
		obs_sceneitem_set_show_transition_duration(item, duration_ms);
	} else {
		obs_sceneitem_set_hide_transition_duration(item, duration_ms);
	}
}
#endif

static void setSceneItemTransition(const OBSSceneItem &item,
				   const OBSSourceAutoRelease &transition,
				   bool show)
{
	OBSDataAutoRelease settings = obs_source_get_settings(transition);
	if (!transition || !settings) {
		// Set transition to "None"
		obs_sceneitem_set_transition(item, show, nullptr);
		return;
	}

	// We cannot share the transition source between
	// scene items without introducing strange graphical
	// artifacts so we have to create new ones here
	OBSSourceAutoRelease transitionSource = obs_source_create_private(
		obs_source_get_id(transition), obs_source_get_name(transition),
		settings);
	obs_sceneitem_set_transition(item, show, transitionSource);
}

void MacroActionTransition::SetSourceTransition(bool show)
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(27, 0, 0)
	OBSSourceAutoRelease transition =
		obs_weak_source_get_source(_transition.GetTransition());
	const auto items = _source.GetSceneItems(_scene);
	for (const auto &item : items) {
		if (_setTransitionType) {
			setSceneItemTransition(item, transition, show);
		}
		if (_setDuration) {
			obs_sceneitem_set_transition_duration(
				item, show, _duration.Milliseconds());
		}
	}

#endif
}

bool MacroActionTransition::PerformAction()
{
	switch (_type) {
	case Type::SCENE:
		SetSceneTransition();
		break;
	case Type::SCENE_OVERRIDE:
		SetTransitionOverride();
		break;
	case Type::SOURCE_SHOW:
		SetSourceTransition(true);
		break;
	case Type::SOURCE_HIDE:
		SetSourceTransition(false);
		break;
	}
	return true;
}

void MacroActionTransition::LogAction() const
{
	std::string msgBegin;
	switch (_type) {
	case Type::SCENE:
		msgBegin += "set scene transition";
		break;
	case Type::SCENE_OVERRIDE:
		msgBegin += "set scene override transition of " +
			    _scene.ToString(true);
		break;
	case Type::SOURCE_SHOW:
		msgBegin += "set source show transition of " +
			    _source.ToString(true) + " on scene " +
			    _scene.ToString(true);
		break;
	case Type::SOURCE_HIDE:
		msgBegin += "set source hide transition of " +
			    _source.ToString(true) + " on scene " +
			    _scene.ToString(true);
		break;
	}
	if (_setDuration) {
		vblog(LOG_INFO, "%s duration to %s", msgBegin.c_str(),
		      _duration.ToString().c_str());
	}
	if (_setTransitionType) {
		vblog(LOG_INFO, "%s type to \"%s\"", msgBegin.c_str(),
		      _transition.ToString().c_str());
	}
}

bool MacroActionTransition::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "actionType", static_cast<int>(_type));
	_source.Save(obj);
	_scene.Save(obj);
	_duration.Save(obj);
	_transition.Save(obj);
	obs_data_set_bool(obj, "setDuration", _setDuration);
	obs_data_set_bool(obj, "setType", _setTransitionType);
	return true;
}

bool MacroActionTransition::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_type = static_cast<Type>(obs_data_get_int(obj, "actionType"));
	_source.Load(obj);
	_scene.Load(obj);
	_duration.Load(obj);
	_transition.Load(obj);
	_setDuration = obs_data_get_bool(obj, "setDuration");
	_setTransitionType = obs_data_get_bool(obj, "setType");
	return true;
}

std::string MacroActionTransition::GetShortDesc() const
{
	std::string msgBegin;
	switch (_type) {
	case Type::SCENE:
		return _transition.ToString();
	case Type::SCENE_OVERRIDE:
		return _scene.ToString() + " - " + _transition.ToString();
	case Type::SOURCE_SHOW:
		return _scene.ToString() + " - " + _source.ToString() + " - " +
		       _transition.ToString();
	case Type::SOURCE_HIDE:
		return _scene.ToString() + " - " + _source.ToString() + " - " +
		       _transition.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionTransition::Create(Macro *m)
{
	return std::make_shared<MacroActionTransition>(m);
}

std::shared_ptr<MacroAction> MacroActionTransition::Copy() const
{
	return std::make_shared<MacroActionTransition>(*this);
}

void MacroActionTransition::ResolveVariablesToFixedValues()
{
	_source.ResolveVariables();
	_scene.ResolveVariables();
	_duration.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionTransitionEdit::MacroActionTransitionEdit(
	QWidget *parent, std::shared_ptr<MacroActionTransition> entryData)
	: QWidget(parent),
	  _actions(new QComboBox),
	  _sources(new SceneItemSelectionWidget(parent, false)),
	  _scenes(new SceneSelectionWidget(this, true, false, false, true)),
	  _setTransition(new QCheckBox),
	  _setDuration(new QCheckBox),
	  _transitions(new TransitionSelectionWidget(this, false)),
	  _duration(new DurationSelection(this, false)),
	  _transitionLayout(new QHBoxLayout),
	  _durationLayout(new QHBoxLayout)
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_transitions,
			 SIGNAL(TransitionChanged(const TransitionSelection &)),
			 this,
			 SLOT(TransitionChanged(const TransitionSelection &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_setTransition, SIGNAL(stateChanged(int)), this,
			 SLOT(SetTransitionChanged(int)));
	QWidget::connect(_setDuration, SIGNAL(stateChanged(int)), this,
			 SLOT(SetDurationChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _actions},
		{"{{sources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{transitions}}", _transitions},
		{"{{duration}}", _duration},
		{"{{setTransition}}", _setTransition},
		{"{{setDuration}}", _setDuration},
	};
	QHBoxLayout *typeLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.transition.entry.line1"),
		     typeLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.transition.entry.line2"),
		     _transitionLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.transition.entry.line3"),
		     _durationLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(typeLayout);
	mainLayout->addLayout(_transitionLayout);
	mainLayout->addLayout(_durationLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTransitionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_scenes->SetScene(_entryData->_scene);
	_sources->SetSceneItem((_entryData->_source));
	_setDuration->setChecked(_entryData->_setDuration);
	_duration->SetDuration(_entryData->_duration);
	_setTransition->setChecked(_entryData->_setTransitionType);
	_transitions->SetTransition(_entryData->_transition);
	_transitions->setEnabled(_entryData->_setTransitionType);
	_duration->setEnabled(_entryData->_setDuration);
	SetWidgetVisibility();
}

void MacroActionTransitionEdit::SourceChanged(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionTransitionEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type = static_cast<MacroActionTransition::Type>(value);
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionTransitionEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionTransitionEdit::TransitionChanged(const TransitionSelection &t)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_transition = t;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionTransitionEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_duration = dur;
}

void MacroActionTransitionEdit::SetWidgetVisibility()
{
	_sources->setVisible(
		_entryData->_type == MacroActionTransition::Type::SOURCE_HIDE ||
		_entryData->_type == MacroActionTransition::Type::SOURCE_SHOW);
	_scenes->setVisible(_entryData->_type !=
			    MacroActionTransition::Type::SCENE);
	adjustSize();
}

void MacroActionTransitionEdit::SetTransitionChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_setTransitionType = state;
	_transitions->setEnabled(state);
	if (state) {
		emit HeaderInfoChanged(
			QString::fromStdString(_entryData->GetShortDesc()));
	} else {
		emit HeaderInfoChanged("");
	}
}

void MacroActionTransitionEdit::SetDurationChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_setDuration = state;
	_duration->setEnabled(state);
}

} // namespace advss
