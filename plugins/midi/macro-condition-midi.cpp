#include "macro-condition-midi.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroConditionMidi::id = "midi";

bool MacroConditionMidi::_registered = MacroConditionFactory::Register(
	MacroConditionMidi::id,
	{MacroConditionMidi::Create, MacroConditionMidiEdit::Create,
	 "AdvSceneSwitcher.condition.midi"});

bool MacroConditionMidi::CheckCondition()
{
	if (!_messageBuffer) {
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

bool MacroConditionMidi::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_message.Save(obj);
	_device.Save(obj);
	return true;
}

bool MacroConditionMidi::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_message.Load(obj);
	_device.Load(obj);
	_messageBuffer = _device.RegisterForMidiMessages();
	return true;
}

std::string MacroConditionMidi::GetShortDesc() const
{
	return _device.Name();
}

void MacroConditionMidi::SetDevice(const MidiDevice &dev)
{
	_device = dev;
	_messageBuffer = dev.RegisterForMidiMessages();
}

void MacroConditionMidi::SetupTempVars()
{
	MacroCondition::SetupTempVars();
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

MacroConditionMidiEdit::MacroConditionMidiEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMidi> entryData)
	: QWidget(parent),
	  _devices(new MidiDeviceSelection(this, MidiDeviceType::INPUT)),
	  _message(new MidiMessageSelection(this)),
	  _resetMidiDevices(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.resetDevices"))),
	  _listen(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.startListen")))
{
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

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.midi.entry"),
		     entryLayout, {{"{{device}}", _devices}});
	auto listenLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.midi.entry.listen"),
		listenLayout, {{"{{listenButton}}", _listen}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_message);
	mainLayout->addLayout(listenLayout);
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

	_message->SetMessage(_entryData->_message);
	_devices->SetDevice(_entryData->GetDevice());

	adjustSize();
	updateGeometry();
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

} // namespace advss
