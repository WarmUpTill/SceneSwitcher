#include "macro-action-media.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionMedia::id = "media";

bool MacroActionMedia::_registered = MacroActionFactory::Register(
	MacroActionMedia::id,
	{MacroActionMedia::Create, MacroActionMediaEdit::Create,
	 "AdvSceneSwitcher.action.media"});

const static std::map<MediaAction, std::string> actionTypes = {
	{MediaAction::PLAY, "AdvSceneSwitcher.action.media.type.play"},
	{MediaAction::PAUSE, "AdvSceneSwitcher.action.media.type.pause"},
	{MediaAction::STOP, "AdvSceneSwitcher.action.media.type.stop"},
	{MediaAction::RESTART, "AdvSceneSwitcher.action.media.type.restart"},
	{MediaAction::NEXT, "AdvSceneSwitcher.action.media.type.next"},
	{MediaAction::PREVIOUS, "AdvSceneSwitcher.action.media.type.previous"},
	{MediaAction::SEEK, "AdvSceneSwitcher.action.media.type.seek"},
};

bool MacroActionMedia::PerformAction()
{
	auto source = obs_weak_source_get_source(_mediaSource.GetSource());
	obs_media_state state = obs_source_media_get_state(source);
	switch (_action) {
	case MediaAction::PLAY:
		if (state == OBS_MEDIA_STATE_STOPPED ||
		    state == OBS_MEDIA_STATE_ENDED) {
			obs_source_media_restart(source);
		} else {
			obs_source_media_play_pause(source, false);
		}
		break;
	case MediaAction::PAUSE:
		obs_source_media_play_pause(source, true);
		break;
	case MediaAction::STOP:
		obs_source_media_stop(source);
		break;
	case MediaAction::RESTART:
		obs_source_media_restart(source);
		break;
	case MediaAction::NEXT:
		obs_source_media_next(source);
		break;
	case MediaAction::PREVIOUS:
		obs_source_media_previous(source);
		break;
	case MediaAction::SEEK:
		obs_source_media_set_time(source, _seek.Milliseconds());
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
	_mediaSource.Save(obj, "mediaSource");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_seek.Save(obj);
	return true;
}

bool MacroActionMedia::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_mediaSource.Load(obj, "mediaSource");
	_action = static_cast<MediaAction>(obs_data_get_int(obj, "action"));
	_seek.Load(obj);
	return true;
}

std::string MacroActionMedia::GetShortDesc() const
{
	return _mediaSource.ToString();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionMediaEdit::MacroActionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroActionMedia> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _actions(new QComboBox()),
	  _seek(new DurationSelection())
{
	populateActionSelection(_actions);
	auto sources = GetMediaSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_seek, SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(DurationChanged(const Duration &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{mediaSources}}", _sources},
		{"{{actions}}", _actions},
		{"{{duration}}", _seek},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.media.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_mediaSource);
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_seek->SetDuration(_entryData->_seek);
	SetWidgetVisibility();
}

void MacroActionMediaEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_mediaSource = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionMediaEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<MediaAction>(value);
	SetWidgetVisibility();
}

void MacroActionMediaEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_seek = dur;
}

void MacroActionMediaEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_seek->setVisible(_entryData->_action == MediaAction::SEEK);
	adjustSize();
}

} // namespace advss
