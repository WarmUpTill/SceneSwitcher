#include "macro-condition-audio.hpp"
#include "audio-helpers.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "selection-helpers.hpp"

namespace advss {

constexpr int64_t nsPerMs = 1000000;
const std::string MacroConditionAudio::id = "audio";

bool MacroConditionAudio::_registered = MacroConditionFactory::Register(
	MacroConditionAudio::id,
	{MacroConditionAudio::Create, MacroConditionAudioEdit::Create,
	 "AdvSceneSwitcher.condition.audio"});

const static std::map<MacroConditionAudio::Type, std::string> checkTypes = {
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

const static std::map<MacroConditionAudio::OutputCondition, std::string>
	audioOutputConditionTypes = {
		{MacroConditionAudio::OutputCondition::ABOVE,
		 "AdvSceneSwitcher.condition.audio.state.above"},
		{MacroConditionAudio::OutputCondition::BELOW,
		 "AdvSceneSwitcher.condition.audio.state.below"},
};

const static std::map<MacroConditionAudio::VolumeCondition, std::string>
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

void MacroConditionAudio::SetType(const Type &type)
{
	_checkType = type;
	SetupTempVars();
}

MacroConditionAudio::~MacroConditionAudio()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);
}

bool MacroConditionAudio::CheckOutputCondition()
{
	bool ret = false;
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_audioSource.GetSource());

	double curVolume = _useDb ? _peak : DecibelToPercent(_peak) * 100;

	switch (_outputCondition) {
	case OutputCondition::ABOVE:
		if (_useDb) {
			ret = curVolume > _volumeDB;
		} else {
			ret = curVolume > _volumePercent;
		}
		break;
	case OutputCondition::BELOW:
		if (_useDb) {
			ret = curVolume < _volumeDB;
		} else {
			ret = curVolume < _volumePercent;
		}
		break;
	default:
		break;
	}

	SetVariableValue(std::to_string(curVolume));
	SetTempVarValue("output_volume", std::to_string(curVolume));

	// Reset for next check
	_peak = -std::numeric_limits<float>::infinity();
	if (_audioSource.GetType() == SourceSelection::Type::VARIABLE) {
		ResetVolmeter();
	}

	return ret && source;
}

bool MacroConditionAudio::CheckVolumeCondition()
{
	bool ret = false;
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_audioSource.GetSource());

	float curVolume =
		_useDb ? PercentToDecibel(obs_source_get_volume(source))
		       : obs_source_get_volume(source);
	bool muted = obs_source_muted(source);

	switch (_volumeCondition) {
	case VolumeCondition::ABOVE:
		if (_useDb) {
			ret = curVolume > _volumeDB;
		} else {
			ret = curVolume > _volumePercent / 100.f;
		}
		SetVariableValue(std::to_string(curVolume));
		break;
	case VolumeCondition::EXACT:
		if (_useDb) {
			ret = curVolume == _volumeDB;
		} else {
			ret = curVolume == _volumePercent / 100.f;
		}
		SetVariableValue(std::to_string(curVolume));
		break;
	case VolumeCondition::BELOW:
		if (_useDb) {
			ret = curVolume < _volumeDB;
		} else {
			ret = curVolume < _volumePercent / 100.f;
		}
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

	SetTempVarValue("configured_volume", std::to_string(curVolume));
	SetTempVarValue("muted", (source && muted) ? "true" : "false");

	return ret && source;
}

bool MacroConditionAudio::CheckSyncOffset()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_audioSource.GetSource());
	auto curOffset = obs_source_get_sync_offset(source) / nsPerMs;
	if (_outputCondition == OutputCondition::ABOVE) {
		ret = curOffset > _syncOffset;
	} else {
		ret = curOffset < _syncOffset;
	}
	SetVariableValue(std::to_string(curOffset));
	SetTempVarValue("sync_offset", std::to_string(curOffset));
	return ret && source;
}

bool MacroConditionAudio::CheckMonitor()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_audioSource.GetSource());
	ret = obs_source_get_monitoring_type(source) == _monitorType;
	SetVariableValue("");
	SetTempVarValue("monitor",
			std::to_string(obs_source_get_monitoring_type(source)));
	return ret && source;
}

bool MacroConditionAudio::CheckBalance()
{
	if (!_audioSource.GetSource()) {
		return false;
	}

	bool ret = false;
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_audioSource.GetSource());
	auto curBalance = obs_source_get_balance_value(source);
	if (_outputCondition == OutputCondition::ABOVE) {
		ret = curBalance > _balance;
	} else {
		ret = curBalance < _balance;
	}
	SetVariableValue(std::to_string(curBalance));
	SetTempVarValue("balance", std::to_string(curBalance));
	return ret && source;
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
	_volumePercent.Save(obj, "volume");
	_syncOffset.Save(obj, "syncOffset");
	_balance.Save(obj, "balance");
	obs_data_set_int(obj, "checkType", static_cast<int>(_checkType));
	obs_data_set_int(obj, "outputCondition",
			 static_cast<int>(_outputCondition));
	obs_data_set_int(obj, "volumeCondition",
			 static_cast<int>(_volumeCondition));
	obs_data_set_bool(obj, "useDb", _useDb);
	_volumeDB.Save(obj, "volumeDB");
	obs_data_set_int(obj, "version", 2);
	return true;
}

obs_volmeter_t *AddVolmeterToSource(MacroConditionAudio *entry,
				    obs_weak_source *source)
{
	obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, MacroConditionAudio::SetVolumeLevel,
				  entry);
	OBSSourceAutoRelease audioSource = obs_weak_source_get_source(source);
	if (!obs_volmeter_attach_source(volmeter, audioSource)) {
		const char *name = obs_source_get_name(audioSource);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}

	return volmeter;
}

bool MacroConditionAudio::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_audioSource.Load(obj, "audioSource");
	_monitorType = static_cast<obs_monitoring_type>(
		obs_data_get_int(obj, "monitor"));
	if (!obs_data_has_user_value(obj, "version")) {
		_volumePercent = obs_data_get_int(obj, "volume");
		_syncOffset = obs_data_get_int(obj, "syncOffset");
		_balance = obs_data_get_double(obj, "balance");
	} else {
		_volumePercent.Load(obj, "volume");
		_syncOffset.Load(obj, "syncOffset");
		_balance.Load(obj, "balance");
	}
	_checkType = static_cast<Type>(obs_data_get_int(obj, "checkType"));
	_outputCondition = static_cast<OutputCondition>(
		obs_data_get_int(obj, "outputCondition"));
	_volumeCondition = static_cast<VolumeCondition>(
		obs_data_get_int(obj, "volumeCondition"));
	_volmeter = AddVolmeterToSource(this, _audioSource.GetSource());
	if (obs_data_get_int(obj, "version") != 2) {
		_useDb = false;
		_volumeDB = 0.0;
	} else {
		_useDb = obs_data_get_bool(obj, "useDb");
		_volumeDB.Load(obj, "volumeDB");
	}
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
	auto c = static_cast<MacroConditionAudio *>(data);
	const auto macro = c->GetMacro();
	if (MacroIsPaused(macro)) {
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

void MacroConditionAudio::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_checkType) {
	case Type::OUTPUT_VOLUME:
		AddTempvar(
			"output_volume",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.audio.output_volume"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.audio.output_volume.description"));
		break;
	case Type::CONFIGURED_VOLUME:
		AddTempvar(
			"configured_volume",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.audio.configured_volume"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.audio.configured_volume.description"));
		AddTempvar("muted",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.audio.muted"));
		break;
	case Type::SYNC_OFFSET:
		AddTempvar(
			"sync_offset",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.audio.sync_offset"));
		break;
	case Type::MONITOR:
		AddTempvar("monitor",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.audio.monitor"));
		break;
	case Type::BALANCE:
		AddTempvar("balance",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.audio.balance"));
		break;
	default:
		break;
	}
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
	for (const auto &[_, name] : audioOutputConditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateVolumeConditionSelection(QComboBox *list)
{
	list->clear();
	for (const auto &[_, name] : audioVolumeConditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionAudioEdit::MacroConditionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroConditionAudio> entryData)
	: QWidget(parent),
	  _checkTypes(new QComboBox()),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _condition(new QComboBox()),
	  _volumePercent(new VariableSpinBox()),
	  _volumeDB(new VariableDoubleSpinBox),
	  _percentDBToggle(new QPushButton),
	  _syncOffset(new VariableSpinBox()),
	  _monitorTypes(new QComboBox),
	  _balance(new SliderSpinBox(0., 1., ""))
{
	_volumePercent->setSuffix("%");
	_volumePercent->setMaximum(100);
	_volumePercent->setMinimum(0);

	_volumeDB->setMinimum(-100);
	_volumeDB->setMaximum(0);
	_volumeDB->setSuffix("dB");
	_volumeDB->specialValueText("-inf");

	_syncOffset->setMinimum(-950);
	_syncOffset->setMaximum(20000);
	_syncOffset->setSuffix("ms");

	auto sources = GetAudioSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_checkTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CheckTypeChanged(int)));
	QWidget::connect(
		_volumePercent,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(VolumePercentChanged(const NumberVariable<int> &)));
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
	QWidget::connect(
		_volumeDB,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(VolumeDBChanged(const NumberVariable<double> &)));
	QWidget::connect(_percentDBToggle, SIGNAL(clicked()), this,
			 SLOT(PercentDBClicked()));

	populateCheckTypes(_checkTypes);
	PopulateMonitorTypeSelection(_monitorTypes);

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkType}}", _checkTypes},
		{"{{audioSources}}", _sources},
		{"{{volume}}", _volumePercent},
		{"{{syncOffset}}", _syncOffset},
		{"{{monitorTypes}}", _monitorTypes},
		{"{{balance}}", _balance},
		{"{{condition}}", _condition},
		{"{{volumeDB}}", _volumeDB},
		{"{{percentDBToggle}}", _percentDBToggle},
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
	OBSSourceAutoRelease soruce = obs_weak_source_get_source(
		_entryData->_audioSource.GetSource());
	_volMeter = new VolControl(soruce.Get());

	auto layout = this->layout();
	layout->addWidget(_volMeter);

	QWidget::connect(_volMeter->GetSlider(), &QSlider::valueChanged,
			 [=](int) { SyncSliderAndValueSelection(true); });

	// Slider will default to 0 so set it manually once
	_volMeter->GetSlider()->setValue(_entryData->_volumePercent);
}

void MacroConditionAudioEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_audioSource = source;
		_entryData->ResetVolmeter();
	}
	UpdateVolmeterSource();
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionAudioEdit::VolumePercentChanged(
	const NumberVariable<int> &vol)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		auto lock = LockContext();
		_entryData->_volumePercent = vol;
	}

	SyncSliderAndValueSelection(false);
}

void MacroConditionAudioEdit::SyncOffsetChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_syncOffset = value;
}

void MacroConditionAudioEdit::MonitorTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_monitorType = static_cast<obs_monitoring_type>(value);
}

void MacroConditionAudioEdit::BalanceChanged(const NumberVariable<double> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_balance = value;
}

void MacroConditionAudioEdit::VolumeDBChanged(
	const NumberVariable<double> &value)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		auto lock = LockContext();
		_entryData->_volumeDB = value;
	}
	SyncSliderAndValueSelection(false);
}

void MacroConditionAudioEdit::PercentDBClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_useDb = !_entryData->_useDb;
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::SyncSliderAndValueSelection(bool sliderMoved)
{
	if (_loading || !_entryData) {
		return;
	}

	if (sliderMoved) {
		if (_entryData->_useDb) {
			_volumeDB->SetFixedValue(PercentToDecibel(
				(float)_volMeter->GetSlider()->value() /
				100.0));
			const QSignalBlocker blocker(this);
			_volumePercent->SetFixedValue(
				_volMeter->GetSlider()->value());
		} else {
			_volumePercent->SetFixedValue(
				_volMeter->GetSlider()->value());
			const QSignalBlocker blocker(this);
			_volumeDB->SetFixedValue(PercentToDecibel(
				(float)_volMeter->GetSlider()->value() /
				100.0));
		}
	} else {
		const QSignalBlocker blocker(this);
		if (_entryData->_useDb) {
			_volMeter->GetSlider()->setValue(
				DecibelToPercent(_entryData->_volumeDB) * 100);
		} else {
			_volMeter->GetSlider()->setValue(
				_entryData->_volumePercent);
		}
	}
}

void MacroConditionAudioEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	if (_entryData->GetType() == MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->GetType() == MacroConditionAudio::Type::BALANCE ||
	    _entryData->GetType() == MacroConditionAudio::Type::SYNC_OFFSET) {
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

	auto lock = LockContext();
	_entryData->SetType(static_cast<MacroConditionAudio::Type>(
		_checkTypes->itemData(idx).toInt()));

	const QSignalBlocker b(_condition);
	if (_entryData->GetType() == MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->GetType() == MacroConditionAudio::Type::BALANCE ||
	    _entryData->GetType() == MacroConditionAudio::Type::SYNC_OFFSET) {
		populateOutputConditionSelection(_condition);
	} else if (_entryData->GetType() ==
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
	_volumePercent->SetValue(_entryData->_volumePercent);
	_volumeDB->SetValue(_entryData->_volumeDB);
	_syncOffset->SetValue(_entryData->_syncOffset);
	_monitorTypes->setCurrentIndex(_entryData->_monitorType);
	_balance->SetDoubleValue(_entryData->_balance);
	_checkTypes->setCurrentIndex(
		_checkTypes->findData(static_cast<int>(_entryData->GetType())));

	if (_entryData->GetType() == MacroConditionAudio::Type::OUTPUT_VOLUME ||
	    _entryData->GetType() == MacroConditionAudio::Type::BALANCE ||
	    _entryData->GetType() == MacroConditionAudio::Type::SYNC_OFFSET) {
		populateOutputConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_outputCondition));
	} else if (_entryData->GetType() ==
		   MacroConditionAudio::Type::CONFIGURED_VOLUME) {
		populateVolumeConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_volumeCondition));
	}

	UpdateVolmeterSource();
	SetWidgetVisibility();
}

bool MacroConditionAudioEdit::HasVolumeControl() const
{
	return _entryData->GetType() ==
		       MacroConditionAudio::Type::OUTPUT_VOLUME ||
	       (_entryData->GetType() ==
			MacroConditionAudio::Type::CONFIGURED_VOLUME &&
		(_entryData->_volumeCondition ==
			 MacroConditionAudio::VolumeCondition::ABOVE ||
		 _entryData->_volumeCondition ==
			 MacroConditionAudio::VolumeCondition::EXACT ||
		 _entryData->_volumeCondition ==
			 MacroConditionAudio::VolumeCondition::BELOW));
}

void MacroConditionAudioEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_condition->setVisible(
		_entryData->GetType() ==
			MacroConditionAudio::Type::OUTPUT_VOLUME ||
		_entryData->GetType() ==
			MacroConditionAudio::Type::CONFIGURED_VOLUME ||
		_entryData->GetType() == MacroConditionAudio::Type::BALANCE ||
		_entryData->GetType() ==
			MacroConditionAudio::Type::SYNC_OFFSET);
	_syncOffset->setVisible(_entryData->GetType() ==
				MacroConditionAudio::Type::SYNC_OFFSET);
	_monitorTypes->setVisible(_entryData->GetType() ==
				  MacroConditionAudio::Type::MONITOR);
	_balance->setVisible(_entryData->GetType() ==
			     MacroConditionAudio::Type::BALANCE);
	_volMeter->setVisible(_entryData->GetType() ==
			      MacroConditionAudio::Type::OUTPUT_VOLUME);
	_volumePercent->setVisible(HasVolumeControl() && !_entryData->_useDb);
	_volumeDB->setVisible(HasVolumeControl() && _entryData->_useDb);
	_percentDBToggle->setText(_entryData->_useDb ? "dB" : "%");
	_percentDBToggle->setVisible(HasVolumeControl());
	adjustSize();
}

} // namespace advss
