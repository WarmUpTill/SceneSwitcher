#include "macro-condition-edit.hpp"
#include "macro-condition-audio.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

constexpr int64_t nsPerMs = 1000000;
const std::string MacroConditionAudio::id = "audio";

bool MacroConditionAudio::_registered = MacroConditionFactory::Register(
	MacroConditionAudio::id,
	{MacroConditionAudio::Create, MacroConditionAudioEdit::Create,
	 "AdvSceneSwitcher.condition.audio"});

static std::map<MacroConditionAudio::Type, std::string> checkTypes = {
	{MacroConditionAudio::Type::OUTPUT_VOLUME,
	 "AdvSceneSwitcher.condition.audio.type.output"},
	{MacroConditionAudio::Type::CONFIGURED_VOLUME,
	 "AdvSceneSwitcher.condition.audio.type.volume"},
	{MacroConditionAudio::Type::SYNC_OFFSET,
	 "AdvSceneSwitcher.condition.audio.type.syncOffset"},
	{MacroConditionAudio::Type::MONITOR,
	 "AdvSceneSwitcher.condition.audio.type.monitor"},
	{MacroConditionAudio::Type::BALANCE,
	 "AdvSceneSwitcher.condition.audio.type.balance"},
};

static std::map<MacroConditionAudio::OutputCondition, std::string>
	audioOutputConditionTypes = {
		{MacroConditionAudio::OutputCondition::ABOVE,
		 "AdvSceneSwitcher.condition.audio.state.above"},
		{MacroConditionAudio::OutputCondition::BELOW,
		 "AdvSceneSwitcher.condition.audio.state.below"},
};

static std::map<MacroConditionAudio::VolumeCondition, std::string>
	audioVolumeConditionTypes = {
		{MacroConditionAudio::VolumeCondition::ABOVE,
		 "AdvSceneSwitcher.condition.audio.state.above"},
		{MacroConditionAudio::VolumeCondition::EXACT,
		 "AdvSceneSwitcher.condition.audio.state.exact"},
		{MacroConditionAudio::VolumeCondition::BELOW,
		 "AdvSceneSwitcher.condition.audio.state.below"},
		{MacroConditionAudio::VolumeCondition::MUTE,
		 "AdvSceneSwitcher.condition.audio.state.mute"},
		{MacroConditionAudio::VolumeCondition::UNMUTE,
		 "AdvSceneSwitcher.condition.audio.state.unmute"},
};

MacroConditionAudio::~MacroConditionAudio()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);
}

bool MacroConditionAudio::CheckOutputCondition()
{
	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource.GetSource());

	double curVolume = ((double)_peak + 60) * 1.7;

	switch (_outputCondition) {
	case OutputCondition::ABOVE:
		// peak will have a value from -60 db to 0 db
		ret = curVolume > _volume;
		break;
	case OutputCondition::BELOW:
		// peak will have a value from -60 db to 0 db
		ret = curVolume < _volume;
		break;
	default:
		break;
	}

	SetVariableValue(std::to_string(curVolume));

	// Reset for next check
	_peak = -std::numeric_limits<float>::infinity();
	obs_source_release(s);
	if (_audioSource.GetType() == SourceSelection::Type::VARIABLE) {
		ResetVolmeter();
	}

	return ret;
}

bool MacroConditionAudio::CheckVolumeCondition()
{
	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource.GetSource());

	float curVolume = obs_source_get_volume(s);
	bool muted = obs_source_muted(s);

	switch (_volumeCondition) {
	case VolumeCondition::ABOVE:
		ret = curVolume > _volume / 100.f;
		SetVariableValue(std::to_string(curVolume));
		break;
	case VolumeCondition::EXACT:
		ret = curVolume == _volume / 100.f;
		SetVariableValue(std::to_string(curVolume));
		break;
	case VolumeCondition::BELOW:
		ret = curVolume < _volume / 100.f;
		SetVariableValue(std::to_string(curVolume));
		break;
	case VolumeCondition::MUTE:
		ret = muted;
		SetVariableValue("");
		break;
	case VolumeCondition::UNMUTE:
		ret = !muted;
		SetVariableValue("");
		break;
	default:
		break;
	}

	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::CheckSyncOffset()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource.GetSource());
	auto curOffset = obs_source_get_sync_offset(s) / nsPerMs;
	if (_outputCondition == OutputCondition::ABOVE) {
		ret = curOffset > _syncOffset;
	} else {
		ret = curOffset < _syncOffset;
	}
	SetVariableValue(std::to_string(curOffset));
	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::CheckMonitor()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource.GetSource());
	ret = obs_source_get_monitoring_type(s) == _monitorType;
	SetVariableValue("");
	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::CheckBalance()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource.GetSource());
	auto curBalance = obs_source_get_balance_value(s);
	if (_outputCondition == OutputCondition::ABOVE) {
		ret = curBalance > _balance;
	} else {
		ret = curBalance < _balance;
	}
	SetVariableValue(std::to_string(curBalance));
	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::CheckCondition()
{
	bool ret = false;
	switch (_checkType) {
	case Type::OUTPUT_VOLUME:
		ret = CheckOutputCondition();
		break;
	case Type::CONFIGURED_VOLUME:
		ret = CheckVolumeCondition();
		break;
	case Type::SYNC_OFFSET:
		ret = CheckSyncOffset();
		break;
	case Type::MONITOR:
		ret = CheckMonitor();
		break;
	case Type::BALANCE:
		ret = CheckBalance();
		break;
	}

	if (GetVariableValue().empty()) {
		SetVariableValue(ret ? "true" : "false");
	}

	return ret;
}

bool MacroConditionAudio::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_audioSource.Save(obj, "audioSource");
	obs_data_set_int(obj, "monitor", _monitorType);
	_volume.Save(obj, "volume");
	_syncOffset.Save(obj, "syncOffset");
	_balance.Save(obj, "balance");
	obs_data_set_int(obj, "checkType", static_cast<int>(_checkType));
	obs_data_set_int(obj, "outputCondition",
			 static_cast<int>(_outputCondition));
	obs_data_set_int(obj, "volumeCondition",
			 static_cast<int>(_volumeCondition));
	obs_data_set_int(obj, "version", 1);
	return true;
}

obs_volmeter_t *AddVolmeterToSource(MacroConditionAudio *entry,
				    obs_weak_source *source)
{
	obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, MacroConditionAudio::SetVolumeLevel,
				  entry);
	obs_source_t *as = obs_weak_source_get_source(source);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);

	return volmeter;
}

bool MacroConditionAudio::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_audioSource.Load(obj, "audioSource");
	_monitorType = static_cast<obs_monitoring_type>(
		obs_data_get_int(obj, "monitor"));
	if (!obs_data_has_user_value(obj, "version")) {
		_volume = obs_data_get_int(obj, "volume");
		_syncOffset = obs_data_get_int(obj, "syncOffset");
		_balance = obs_data_get_double(obj, "balance");
	} else {
		_volume.Load(obj, "volume");
		_syncOffset.Load(obj, "syncOffset");
		_balance.Load(obj, "balance");
	}
	_checkType = static_cast<Type>(obs_data_get_int(obj, "checkType"));
	_outputCondition = static_cast<OutputCondition>(
		obs_data_get_int(obj, "outputCondition"));
	_volumeCondition = static_cast<VolumeCondition>(
		obs_data_get_int(obj, "volumeCondition"));
	_volmeter = AddVolmeterToSource(this, _audioSource.GetSource());
	return true;
}

std::string MacroConditionAudio::GetShortDesc() const
{
	return _audioSource.ToString();
}

void MacroConditionAudio::SetVolumeLevel(void *data, const float *,
					 const float peak[MAX_AUDIO_CHANNELS],
					 const float *)
{
	MacroConditionAudio *c = static_cast<MacroConditionAudio *>(data);
	const auto macro = c->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (peak[i] > c->_peak) {
			c->_peak = peak[i];
		}
	}
}

void MacroConditionAudio::ResetVolmeter()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);

	_volmeter = AddVolmeterToSource(this, _audioSource.GetSource());
}

static inline void populateCheckTypes(QComboBox *list)
{
	list->clear();
	for (const auto &[type, name] : checkTypes) {
		if (type == MacroConditionAudio::Type::MONITOR) {
			if (obs_audio_monitoring_available()) {
				list->addItem(obs_module_text(name.c_str()),
					      static_cast<int>(type));
			}
		} else {
			list->addItem(obs_module_text(name.c_str()),
				      static_cast<int>(type));
		}
	}
}

static inline void populateOutputConditionSelection(QComboBox *list)
{
	list->clear();
	for (auto entry : audioOutputConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateVolumeConditionSelection(QComboBox *list)
{
	list->clear();
	for (auto entry : audioVolumeConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionAudioEdit::MacroConditionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroConditionAudio> entryData)
	: QWidget(parent),
	  _checkTypes(new QComboBox()),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _condition(new QComboBox()),
	  _volume(new VariableSpinBox()),
	  _syncOffset(new VariableSpinBox()),
	  _monitorTypes(new QComboBox),
	  _balance(new SliderSpinBox(0., 1., ""))
{
	_volume->setSuffix("%");
	_volume->setMaximum(100);
	_volume->setMinimum(0);

	_syncOffset->setMinimum(-950);
	_syncOffset->setMaximum(20000);
	_syncOffset->setSuffix("ms");

	auto sources = GetAudioSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_checkTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CheckTypeChanged(int)));
	QWidget::connect(
		_volume,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(VolumeThresholdChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_syncOffset,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SyncOffsetChanged(const NumberVariable<int> &)));
	QWidget::connect(_monitorTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MonitorTypeChanged(int)));
	QWidget::connect(
		_balance,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this, SLOT(BalanceChanged(const NumberVariable<double> &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));

	populateCheckTypes(_checkTypes);
	PopulateMonitorTypeSelection(_monitorTypes);

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkType}}", _checkTypes},
		{"{{audioSources}}", _sources},
		{"{{volume}}", _volume},
		{"{{syncOffset}}", _syncOffset},
		{"{{monitorTypes}}", _monitorTypes},
		{"{{balance}}", _balance},
		{"{{condition}}", _condition},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.audio.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(_balance);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionAudioEdit::UpdateVolmeterSource()
{
	delete _volMeter;
	obs_source_t *soruce = obs_weak_source_get_source(
		_entryData->_audioSource.GetSource());
	_volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	QLayout *layout = this->layout();
	layout->addWidget(_volMeter);

	QWidget::connect(_volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 _volume, SLOT(SetFixedValue(int)));
	QWidget::connect(_volume, SIGNAL(FixedValueChanged(int)),
			 _volMeter->GetSlider(), SLOT(setValue(int)));

	// Slider will default to 0 so set it manually once
	_volMeter->GetSlider()->setValue(_entryData->_volume);
}

void MacroConditionAudioEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_audioSource = source;
		_entryData->ResetVolmeter();
	}
	UpdateVolmeterSource();
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionAudioEdit::VolumeThresholdChanged(
	const NumberVariable<int> &vol)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_volume = vol;
}

void MacroConditionAudioEdit::SyncOffsetChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_syncOffset = value;
}

void MacroConditionAudioEdit::MonitorTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_monitorType = static_cast<obs_monitoring_type>(value);
}

void MacroConditionAudioEdit::BalanceChanged(const NumberVariable<double> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_balance = value;
}

void MacroConditionAudioEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (_entryData->_checkType ==
		    MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->_checkType == MacroConditionAudio::Type::BALANCE ||
	    _entryData->_checkType == MacroConditionAudio::Type::SYNC_OFFSET) {
		_entryData->_outputCondition =
			static_cast<MacroConditionAudio::OutputCondition>(cond);
	} else {
		_entryData->_volumeCondition =
			static_cast<MacroConditionAudio::VolumeCondition>(cond);
	}
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::CheckTypeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_checkType = static_cast<MacroConditionAudio::Type>(
		_checkTypes->itemData(idx).toInt());

	const QSignalBlocker b(_condition);
	if (_entryData->_checkType ==
		    MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->_checkType == MacroConditionAudio::Type::BALANCE ||
	    _entryData->_checkType == MacroConditionAudio::Type::SYNC_OFFSET) {
		populateOutputConditionSelection(_condition);
	} else if (_entryData->_checkType ==
		   MacroConditionAudio::Type::CONFIGURED_VOLUME) {
		populateVolumeConditionSelection(_condition);
	}
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_audioSource);
	_volume->SetValue(_entryData->_volume);
	_syncOffset->SetValue(_entryData->_syncOffset);
	_monitorTypes->setCurrentIndex(_entryData->_monitorType);
	_balance->SetDoubleValue(_entryData->_balance);
	_checkTypes->setCurrentIndex(_checkTypes->findData(
		static_cast<int>(_entryData->_checkType)));

	if (_entryData->_checkType ==
		    MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->_checkType == MacroConditionAudio::Type::BALANCE ||
	    _entryData->_checkType == MacroConditionAudio::Type::SYNC_OFFSET) {
		populateOutputConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_outputCondition));
	} else if (_entryData->_checkType ==
		   MacroConditionAudio::Type::CONFIGURED_VOLUME) {
		populateVolumeConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_volumeCondition));
	}

	UpdateVolmeterSource();
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_volume->setVisible(
		_entryData->_checkType ==
			MacroConditionAudio::Type::OUTPUT_VOLUME ||
		(_entryData->_checkType ==
			 MacroConditionAudio::Type::CONFIGURED_VOLUME &&
		 (_entryData->_volumeCondition ==
			  MacroConditionAudio::VolumeCondition::ABOVE ||
		  _entryData->_volumeCondition ==
			  MacroConditionAudio::VolumeCondition::EXACT ||
		  _entryData->_volumeCondition ==
			  MacroConditionAudio::VolumeCondition::BELOW)));
	_condition->setVisible(
		_entryData->_checkType ==
			MacroConditionAudio::Type::OUTPUT_VOLUME ||
		_entryData->_checkType ==
			MacroConditionAudio::Type::CONFIGURED_VOLUME ||
		_entryData->_checkType == MacroConditionAudio::Type::BALANCE ||
		_entryData->_checkType ==
			MacroConditionAudio::Type::SYNC_OFFSET);
	_syncOffset->setVisible(_entryData->_checkType ==
				MacroConditionAudio::Type::SYNC_OFFSET);
	_monitorTypes->setVisible(_entryData->_checkType ==
				  MacroConditionAudio::Type::MONITOR);
	_balance->setVisible(_entryData->_checkType ==
			     MacroConditionAudio::Type::BALANCE);
	_volMeter->setVisible(_entryData->_checkType ==
			      MacroConditionAudio::Type::OUTPUT_VOLUME);
	adjustSize();
}

} // namespace advss
