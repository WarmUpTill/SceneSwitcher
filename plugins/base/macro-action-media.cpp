#include "macro-action-media.hpp"
#include "layout-helpers.hpp"
#include "selection-helpers.hpp"

namespace advss {

const std::string MacroActionMedia::id = "media";

bool MacroActionMedia::_registered = MacroActionFactory::Register(
	MacroActionMedia::id,
	{MacroActionMedia::Create, MacroActionMediaEdit::Create,
	 "AdvSceneSwitcher.action.media"});

static const std::map<MacroActionMedia::Action, std::string> actionTypes = {
	{MacroActionMedia::Action::PLAY,
	 "AdvSceneSwitcher.action.media.type.play"},
	{MacroActionMedia::Action::PAUSE,
	 "AdvSceneSwitcher.action.media.type.pause"},
	{MacroActionMedia::Action::STOP,
	 "AdvSceneSwitcher.action.media.type.stop"},
	{MacroActionMedia::Action::RESTART,
	 "AdvSceneSwitcher.action.media.type.restart"},
	{MacroActionMedia::Action::NEXT,
	 "AdvSceneSwitcher.action.media.type.next"},
	{MacroActionMedia::Action::PREVIOUS,
	 "AdvSceneSwitcher.action.media.type.previous"},
	{MacroActionMedia::Action::SEEK_DURATION,
	 "AdvSceneSwitcher.action.media.type.seek.duration"},
	{MacroActionMedia::Action::SEEK_PERCENTAGE,
	 "AdvSceneSwitcher.action.media.type.seek.percentage"},
};

static const std::map<MacroActionMedia::SelectionType, std::string>
	selectionTypes = {
		{MacroActionMedia::SelectionType::SOURCE,
		 "AdvSceneSwitcher.action.media.selectionType.source"},
		{MacroActionMedia::SelectionType::SCENE_ITEM,
		 "AdvSceneSwitcher.action.media.selectionType.sceneItem"},
};

std::shared_ptr<MacroAction> MacroActionMedia::Create(Macro *m)
{
	return std::make_shared<MacroActionMedia>(m);
}

std::shared_ptr<MacroAction> MacroActionMedia::Copy() const
{
	return std::make_shared<MacroActionMedia>(*this);
}

std::string MacroActionMedia::GetShortDesc() const
{
	if (_selection == SelectionType::SOURCE) {
		return _mediaSource.ToString();
	}
	return _scene.ToString() + " - " + _sceneItem.ToString();
}

void MacroActionMedia::SeekToPercentage(obs_source_t *source) const
{
	auto totalTimeMs = obs_source_media_get_duration(source);
	auto percentageTimeMs = round(totalTimeMs * _seekPercentage / 100);

	obs_source_media_set_time(source, percentageTimeMs);
}

void MacroActionMedia::PerformActionHelper(obs_source_t *source) const
{
	obs_media_state state = obs_source_media_get_state(source);

	switch (_action) {
	case Action::PLAY:
		if (state == OBS_MEDIA_STATE_STOPPED ||
		    state == OBS_MEDIA_STATE_ENDED) {
			obs_source_media_restart(source);
		} else {
			obs_source_media_play_pause(source, false);
		}
		break;
	case Action::PAUSE:
		obs_source_media_play_pause(source, true);
		break;
	case Action::STOP:
		obs_source_media_stop(source);
		break;
	case Action::RESTART:
		obs_source_media_restart(source);
		break;
	case Action::NEXT:
		obs_source_media_next(source);
		break;
	case Action::PREVIOUS:
		obs_source_media_previous(source);
		break;
	case Action::SEEK_DURATION:
		obs_source_media_set_time(source, _seekDuration.Milliseconds());
		break;
	case Action::SEEK_PERCENTAGE:
		SeekToPercentage(source);
		break;
	default:
		break;
	}
}

bool MacroActionMedia::PerformAction()
{
	if (_selection == SelectionType::SOURCE) {
		OBSSourceAutoRelease source =
			obs_weak_source_get_source(_mediaSource.GetSource());
		PerformActionHelper(source);
	} else {
		const auto items = _sceneItem.GetSceneItems(_scene);
		for (const auto &item : items) {
			PerformActionHelper(obs_sceneitem_get_source(item));
		}
	}
	return true;
}

void MacroActionMedia::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\" for source \"%s\"",
		      it->second.c_str(),
		      _selection == SelectionType::SOURCE
			      ? _mediaSource.ToString(true).c_str()
			      : _sceneItem.ToString().c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown media action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionMedia::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "selectionType", static_cast<int>(_selection));
	_seekDuration.Save(obj);
	_seekPercentage.Save(obj, "seekPercentage");
	_mediaSource.Save(obj, "mediaSource");
	_scene.Save(obj);
	_sceneItem.Save(obj);
	return true;
}

bool MacroActionMedia::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_selection = static_cast<SelectionType>(
		obs_data_get_int(obj, "selectionType"));
	_seekDuration.Load(obj);
	_seekPercentage.Load(obj, "seekPercentage");
	_mediaSource.Load(obj, "mediaSource");
	_scene.Load(obj);
	_sceneItem.Load(obj);
	return true;
}

void MacroActionMedia::ResolveVariablesToFixedValues()
{
	_seekDuration.ResolveVariables();
	_seekPercentage.ResolveVariables();
	_mediaSource.ResolveVariables();
	_sceneItem.ResolveVariables();
	_scene.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateSelectionTypeSelection(QComboBox *list)
{
	for (const auto &[_, name] : selectionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionMediaEdit::MacroActionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroActionMedia> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _selectionTypes(new QComboBox()),
	  _seekDuration(new DurationSelection()),
	  _seekPercentage(new SliderSpinBox(
		  0, 100,
		  obs_module_text(
			  "AdvSceneSwitcher.action.media.seek.percentage.label"))),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _sceneItems(new SceneItemSelectionWidget(parent, false)),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true, true))
{
	populateActionSelection(_actions);
	populateSelectionTypeSelection(_selectionTypes);
	auto sources = GetMediaSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_selectionTypes, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(SelectionTypeChanged(int)));
	QWidget::connect(_seekDuration,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(SeekDurationChanged(const Duration &)));
	QWidget::connect(
		_seekPercentage,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(SeekPercentageChanged(const NumberVariable<double> &)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_sceneItems,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sceneItems,
			 SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.media.entry"),
		     layout,
		     {{"{{actions}}", _actions},
		      {"{{selectionTypes}}", _selectionTypes},
		      {"{{seekDuration}}", _seekDuration},
		      {"{{seekPercentage}}", _seekPercentage},
		      {"{{mediaSources}}", _sources},
		      {"{{scenes}}", _scenes},
		      {"{{sceneItems}}", _sceneItems}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMediaEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionMedia::Action>(value);
	SetWidgetVisibility();
}

void MacroActionMediaEdit::SelectionTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_selection =
		static_cast<MacroActionMedia::SelectionType>(value);
	SetWidgetVisibility();
}

void MacroActionMediaEdit::SeekDurationChanged(const Duration &seekDuration)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_seekDuration = seekDuration;
}

void MacroActionMediaEdit::SeekPercentageChanged(
	const NumberVariable<double> &seekPercentage)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_seekPercentage = seekPercentage;
}

void MacroActionMediaEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_mediaSource = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionMediaEdit::SourceChanged(const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_sceneItem = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	adjustSize();
	updateGeometry();
}

void MacroActionMediaEdit::SceneChanged(const SceneSelection &scene)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = scene;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionMediaEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_sources->setVisible(_entryData->_selection ==
			     MacroActionMedia::SelectionType::SOURCE);
	_scenes->setVisible(_entryData->_selection ==
			    MacroActionMedia::SelectionType::SCENE_ITEM);
	_sceneItems->setVisible(_entryData->_selection ==
				MacroActionMedia::SelectionType::SCENE_ITEM);

	_seekDuration->setVisible(_entryData->_action ==
				  MacroActionMedia::Action::SEEK_DURATION);
	_seekPercentage->setVisible(_entryData->_action ==
				    MacroActionMedia::Action::SEEK_PERCENTAGE);

	adjustSize();
	updateGeometry();
}

void MacroActionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_selectionTypes->setCurrentIndex(
		static_cast<int>(_entryData->_selection));
	_seekDuration->SetDuration(_entryData->_seekDuration);
	_seekPercentage->SetDoubleValue(_entryData->_seekPercentage);
	_sources->SetSource(_entryData->_mediaSource);
	_scenes->SetScene(_entryData->_scene);
	_sceneItems->SetSceneItem((_entryData->_sceneItem));
	SetWidgetVisibility();
}

} // namespace advss
