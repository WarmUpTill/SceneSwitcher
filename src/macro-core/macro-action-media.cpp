#include "macro-action-media.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionMedia::id = "media";

bool MacroActionMedia::_registered = MacroActionFactory::Register(
	MacroActionMedia::id,
	{MacroActionMedia::Create, MacroActionMediaEdit::Create,
	 "AdvSceneSwitcher.action.media"});

const static std::map<MacroActionMedia::Action, std::string> actionTypes = {
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

std::string MacroActionMedia::GetShortDesc() const
{
	return _mediaSource.ToString();
}

void MacroActionMedia::SeekToPercentage(obs_source_t *source) const
{
	auto totalTimeMs = obs_source_media_get_duration(source);
	auto percentageTimeMs = round(totalTimeMs * _seekPercentage / 100);

	obs_source_media_set_time(source, percentageTimeMs);
}

bool MacroActionMedia::PerformAction()
{
	auto source = obs_weak_source_get_source(_mediaSource.GetSource());
	obs_media_state state = obs_source_media_get_state(source);

	switch (_action) {
		using enum Action;
	case PLAY:
		if (state == OBS_MEDIA_STATE_STOPPED ||
		    state == OBS_MEDIA_STATE_ENDED) {
			obs_source_media_restart(source);
		} else {
			obs_source_media_play_pause(source, false);
		}
		break;
	case PAUSE:
		obs_source_media_play_pause(source, true);
		break;
	case STOP:
		obs_source_media_stop(source);
		break;
	case RESTART:
		obs_source_media_restart(source);
		break;
	case NEXT:
		obs_source_media_next(source);
		break;
	case PREVIOUS:
		obs_source_media_previous(source);
		break;
	case SEEK_DURATION:
		obs_source_media_set_time(source, _seekDuration.Milliseconds());
		break;
	case SEEK_PERCENTAGE:
		SeekToPercentage(source);
		break;
	default:
		break;
	}

	obs_source_release(source);
	return true;
}

void MacroActionMedia::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" for source \"%s\"",
		      it->second.c_str(), _mediaSource.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown media action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionMedia::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_seekDuration.Save(obj);
	_seekPercentage.Save(obj, "seekPercentage");
	_mediaSource.Save(obj, "mediaSource");

	return true;
}

bool MacroActionMedia::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_seekDuration.Load(obj);
	_seekPercentage.Load(obj, "seekPercentage");
	_mediaSource.Load(obj, "mediaSource");

	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionMediaEdit::MacroActionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroActionMedia> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _seekDuration(new DurationSelection()),
	  _seekPercentage(new SliderSpinBox(
		  0, 100,
		  obs_module_text(
			  "AdvSceneSwitcher.action.media.seek.percentage.label"))),
	  _sources(new SourceSelectionWidget(this, QStringList(), true))
{
	populateActionSelection(_actions);
	auto sources = GetMediaSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
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

	auto layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{seekDuration}}", _seekDuration},
		{"{{seekPercentage}}", _seekPercentage},
		{"{{mediaSources}}", _sources},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.media.entry"),
		     layout, widgetPlaceholders);
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

void MacroActionMediaEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_seekDuration->setVisible(_entryData->_action ==
				  MacroActionMedia::Action::SEEK_DURATION);
	_seekPercentage->setVisible(_entryData->_action ==
				    MacroActionMedia::Action::SEEK_PERCENTAGE);

	adjustSize();
}

void MacroActionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_seekDuration->SetDuration(_entryData->_seekDuration);
	_seekPercentage->SetDoubleValue(_entryData->_seekPercentage);
	_sources->SetSource(_entryData->_mediaSource);

	SetWidgetVisibility();
}

} // namespace advss
