#include "headers/macro-action-audio.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const int MacroActionAudio::id = 2;

bool MacroActionAudio::_registered = MacroActionFactory::Register(
	MacroActionAudio::id,
	{MacroActionAudio::Create, MacroActionAudioEdit::Create,
	 "AdvSceneSwitcher.action.audio"});

const static std::map<AudioAction, std::string> actionTypes = {
	{AudioAction::MUTE, "AdvSceneSwitcher.action.audio.type.mute"},
	{AudioAction::UNMUTE, "AdvSceneSwitcher.action.audio.type.unmute"},
	{AudioAction::SOURCE_VOLUME,
	 "AdvSceneSwitcher.action.audio.type.sourceVolume"},
	{AudioAction::MASTER_VOLUME,
	 "AdvSceneSwitcher.action.audio.type.masterVolume"},
};

bool MacroActionAudio::PerformAction()
{
	auto s = obs_weak_source_get_source(_audioSource);
	switch (_action) {
	case AudioAction::MUTE:
		obs_source_set_muted(s, true);
		break;
	case AudioAction::UNMUTE:
		obs_source_set_muted(s, false);
		break;
	case AudioAction::SOURCE_VOLUME:
		obs_source_set_volume(s, (float)_volume / 100.0f);
		break;
	case AudioAction::MASTER_VOLUME:
		obs_set_master_volume((float)_volume / 100.0f);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionAudio::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed action \"%s\" for source \"%s\" with volume %d",
		      it->second.c_str(),
		      GetWeakSourceName(_audioSource).c_str(), _volume);
	} else {
		blog(LOG_WARNING, "ignored unknown audio action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionAudio::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(_audioSource).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "volume", _volume);
	return true;
}

bool MacroActionAudio::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	_audioSource = GetWeakSourceByName(audioSourceName);
	_action = static_cast<AudioAction>(obs_data_get_int(obj, "action"));
	_volume = obs_data_get_int(obj, "volume");
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionAudioEdit::MacroActionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroActionAudio> entryData)
	: QWidget(parent)
{
	_audioSources = new QComboBox();
	_actions = new QComboBox();
	_volumePercent = new QSpinBox();
	_volumePercent->setMinimum(0);
	_volumePercent->setMaximum(2000);
	_volumePercent->setSuffix("%");

	populateActionSelection(_actions);
	AdvSceneSwitcher::populateAudioSelection(_audioSources);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_volumePercent, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", _audioSources},
		{"{{actions}}", _actions},
		{"{{volume}}", _volumePercent},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.audio.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

bool hasVolumeControl(AudioAction action)
{
	return action == AudioAction::SOURCE_VOLUME ||
	       action == AudioAction::MASTER_VOLUME;
}

bool hasSourceControl(AudioAction action)
{
	return action != AudioAction::MASTER_VOLUME;
}

void MacroActionAudioEdit::SetWidgetVisibility()
{
	if (hasVolumeControl(_entryData->_action)) {
		_volumePercent->show();
	} else {
		_volumePercent->hide();
	}

	if (hasSourceControl(_entryData->_action)) {
		_audioSources->show();
	} else {
		_audioSources->hide();
	}
}

void MacroActionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_audioSources->setCurrentText(
		GetWeakSourceName(_entryData->_audioSource).c_str());
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_volumePercent->setValue(_entryData->_volume);

	SetWidgetVisibility();
}

void MacroActionAudioEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_audioSource = GetWeakSourceByQString(text);
}

void MacroActionAudioEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<AudioAction>(value);
	SetWidgetVisibility();
}

void MacroActionAudioEdit::VolumeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_volume = value;
}
