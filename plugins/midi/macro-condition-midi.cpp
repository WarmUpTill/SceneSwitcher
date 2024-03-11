#include "macro-condition-midi.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroConditionMidi::id = "midi";

bool MacroConditionMidi::_registered = MacroConditionFactory::Register(
	MacroConditionMidi::id,
	{MacroConditionMidi::Create, MacroConditionMidiEdit::Create,
	 "AdvSceneSwitcher.condition.midi"});

static const std::map<MacroConditionMidi::Condition, std::string>
	conditionTypes = {
		{MacroConditionMidi::Condition::MIDI_MESSAGE,
		 "AdvSceneSwitcher.condition.midi.condition.message"},
		{MacroConditionMidi::Condition::MIDI_CONNECTED_DEVICE_NAMES,
		 "AdvSceneSwitcher.condition.midi.condition.deviceName"},
};

bool MacroConditionMidi::CheckCondition()
{
	switch (_condition) {
	case Condition::MIDI_MESSAGE:
		return CheckMessage();
	case Condition::MIDI_CONNECTED_DEVICE_NAMES:
		return CheckConnectedDevcieNames();
	default:
		break;
	}
	return false;
}

bool MacroConditionMidi::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_message.Save(obj);
	_device.Save(obj);
	_deviceName.Save(obj, "deviceName");
	_regex.Save(obj);
	return true;
}

bool MacroConditionMidi::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_message.Load(obj);
	_device.Load(obj);
	_deviceName.Load(obj, "deviceName");
	_regex.Load(obj);
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	return true;
}

std::string MacroConditionMidi::GetShortDesc() const
{
	return _condition == MacroConditionMidi::Condition::MIDI_MESSAGE
		       ? _device.Name()
		       : "";
}

void MacroConditionMidi::SetDevice(const MidiDevice &dev)
{
	_device = dev;
	_messageBuffer = dev.RegisterForMidiMessages();
}

void MacroConditionMidi::SetCondition(Condition condition)
{
	_condition = condition;
	if (_condition == Condition::MIDI_MESSAGE) {
		_messageBuffer = _device.RegisterForMidiMessages();
	}
	SetupTempVars();
}

bool MacroConditionMidi::CheckMessage()
{
	if (!_messageBuffer) {
		return false;
	}

	const bool macroWasPausedSinceLastCheck =
		MacroWasPausedSince(GetMacro(), _lastCheck);
	_lastCheck = std::chrono::high_resolution_clock::now();
	if (macroWasPausedSinceLastCheck) {
		_messageBuffer->Clear();
		return false;
	}

	while (!_messageBuffer->Empty()) {
		auto message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}
		if (message->Matches(_message)) {
			SetVariableValues(*message);
			return true;
		}
	}

	return false;
}

bool MacroConditionMidi::CheckConnectedDevcieNames()
{
	auto deviceNames = GetDeviceNames();
	if (!_regex.Enabled()) {
		return std::find(deviceNames.begin(), deviceNames.end(),
				 std::string(_deviceName)) != deviceNames.end();
	}
	for (const auto &deviceName : deviceNames) {
		if (_regex.Matches(deviceName, _deviceName)) {
			return true;
		}
	}
	return false;
}

void MacroConditionMidi::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	if (_condition == Condition::MIDI_CONNECTED_DEVICE_NAMES) {
		return;
	}
	AddTempvar("type",
		   obs_module_text("AdvSceneSwitcher.tempVar.midi.type"));
	AddTempvar("channel",
		   obs_module_text("AdvSceneSwitcher.tempVar.midi.channel"));
	AddTempvar("note",
		   obs_module_text("AdvSceneSwitcher.tempVar.midi.note"));
	AddTempvar("value1",
		   obs_module_text("AdvSceneSwitcher.tempVar.midi.value1"));
	AddTempvar("value2",
		   obs_module_text("AdvSceneSwitcher.tempVar.midi.value2"));
}

void MacroConditionMidi::SetVariableValues(const MidiMessage &m)
{
	SetVariableValue(std::to_string(m.Note()) + " " +
			 std::to_string(m.Value()));
	SetTempVarValue("type", m.MidiTypeToString(m.Type()));
	SetTempVarValue("channel", std::to_string(m.Channel()));
	try {
		SetTempVarValue("note",
				GetAllNotes().at(m.Note()).toStdString());
	} catch (...) {
	}
	SetTempVarValue("value1", std::to_string(m.Note()));
	SetTempVarValue("value2", std::to_string(m.Value()));
}

static void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionMidiEdit::MacroConditionMidiEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMidi> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _devices(new MidiDeviceSelection(this, MidiDeviceType::INPUT)),
	  _message(new MidiMessageSelection(this)),
	  _resetMidiDevices(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.resetDevices"))),
	  _listen(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.startListen"))),
	  _deviceNames(new QComboBox()),
	  _regex(new RegexConfigWidget()),
	  _deviceListInfo(new QLabel()),
	  _listenLayout(new QHBoxLayout()),
	  _entryLayout(new QHBoxLayout())
{
	_deviceNames->addItems(GetDeviceNamesAsQStringList());
	_deviceNames->setEditable(true);

	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_deviceListInfo->setPixmap(pixmap);
	_deviceListInfo->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.midi.condition.deviceName.info"));

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_devices,
			 SIGNAL(DeviceSelectionChanged(const MidiDevice &)),
			 this,
			 SLOT(DeviceSelectionChanged(const MidiDevice &)));
	QWidget::connect(_message,
			 SIGNAL(MidiMessageChanged(const MidiMessage &)), this,
			 SLOT(MidiMessageChanged(const MidiMessage &)));
	QWidget::connect(_resetMidiDevices, SIGNAL(clicked()), this,
			 SLOT(ResetMidiDevices()));
	QWidget::connect(_listen, SIGNAL(clicked()), this,
			 SLOT(ToggleListen()));
	QWidget::connect(&_listenTimer, SIGNAL(timeout()), this,
			 SLOT(SetMessageSelectionToLastReceived()));
	QWidget::connect(_deviceNames,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(DeviceNameChanged(const QString &)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.midi.entry.listen"),
		_listenLayout, {{"{{listenButton}}", _listen}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_entryLayout);
	mainLayout->addWidget(_message);
	mainLayout->addLayout(_listenLayout);
	mainLayout->addWidget(_resetMidiDevices);
	setLayout(mainLayout);

	_listenTimer.setInterval(100);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

MacroConditionMidiEdit::~MacroConditionMidiEdit()
{
	EnableListening(false);
}

void MacroConditionMidiEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_message->SetMessage(_entryData->_message);
	_devices->SetDevice(_entryData->GetDevice());
	_deviceNames->setCurrentText(QString::fromStdString(
		_entryData->_deviceName.UnresolvedValue()));
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroConditionMidiEdit::DeviceSelectionChanged(const MidiDevice &device)
{
	if (_loading || !_entryData) {
		return;
	}

	if (_currentlyListening) {
		ToggleListen();
	}

	{
		auto lock = LockContext();
		_entryData->SetDevice(device);
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionMidiEdit::MidiMessageChanged(const MidiMessage &message)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = message;
}

void MacroConditionMidiEdit::ResetMidiDevices()
{
	auto lock = LockContext();
	MidiDeviceInstance::ResetAllDevices();
}

void MacroConditionMidiEdit::EnableListening(bool enable)
{
	if (_currentlyListening == enable) {
		return;
	}
	if (enable) {
		_messageBuffer =
			_entryData->GetDevice().RegisterForMidiMessages();
		_listenTimer.start();
	} else {
		_messageBuffer.reset();
		_listenTimer.stop();
	}
}

void MacroConditionMidiEdit::SetWidgetVisibility()
{
	_entryLayout->removeWidget(_conditions);
	_entryLayout->removeWidget(_devices);
	_entryLayout->removeWidget(_deviceNames);
	_entryLayout->removeWidget(_regex);

	ClearLayout(_entryLayout);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{device}}", _devices},
		{"{{deviceNames}}", _deviceNames},
		{"{{regex}}", _regex},
		{"{{deviceListInfo}}", _deviceListInfo},
	};

	const bool isMessageCheck = _entryData->GetCondition() ==
				    MacroConditionMidi::Condition::MIDI_MESSAGE;

	auto layoutString =
		isMessageCheck
			? "AdvSceneSwitcher.condition.midi.entry.message"
			: "AdvSceneSwitcher.condition.midi.entry.deviceName";

	PlaceWidgets(obs_module_text(layoutString), _entryLayout,
		     widgetPlaceholders);

	SetLayoutVisible(_listenLayout, isMessageCheck);
	_devices->setVisible(isMessageCheck);
	_message->setVisible(isMessageCheck);
	_resetMidiDevices->setVisible(isMessageCheck);
	_deviceNames->setVisible(!isMessageCheck);
	_regex->setVisible(!isMessageCheck);
	_deviceListInfo->setVisible(!isMessageCheck);

	adjustSize();
	updateGeometry();
}

void MacroConditionMidiEdit::ToggleListen()
{
	if (!_entryData) {
		return;
	}

	_listen->setText(
		_currentlyListening
			? obs_module_text("AdvSceneSwitcher.midi.startListen")
			: obs_module_text("AdvSceneSwitcher.midi.stopListen"));
	EnableListening(!_currentlyListening);
	_currentlyListening = !_currentlyListening;
	_message->setDisabled(_currentlyListening);
}

void MacroConditionMidiEdit::SetMessageSelectionToLastReceived()
{
	auto lock = LockContext();
	if (!_entryData || !_messageBuffer || _messageBuffer->Empty()) {
		return;
	}

	std::optional<MidiMessage> message;
	while (!_messageBuffer->Empty()) {
		message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}
	}

	if (!message) {
		return;
	}

	_message->SetMessage(*message);
	_entryData->_message = *message;
}

void MacroConditionMidiEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetCondition(
		static_cast<MacroConditionMidi::Condition>(value));
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionMidiEdit::DeviceNameChanged(const QString &deviceName)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_deviceName = deviceName.toStdString();
}

void MacroConditionMidiEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

} // namespace advss
