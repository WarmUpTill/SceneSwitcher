#include "macro-condition-midi.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionMidi::id = "midi";

bool MacroConditionMidi::_registered = MacroConditionFactory::Register(
	MacroConditionMidi::id,
	{MacroConditionMidi::Create, MacroConditionMidiEdit::Create,
	 "AdvSceneSwitcher.condition.midi"});

bool MacroConditionMidi::CheckCondition()
{
	auto messages = _device.GetMessages();
	if (!messages) {
		return false;
	}

	for (const auto &m : *messages) {
		if (m.Matches(_message)) {
			SetVariableValue(std::to_string(m.Note()) + " " +
					 std::to_string(m.Value()));
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
	return true;
}

std::string MacroConditionMidi::GetShortDesc() const
{
	return _device.Name();
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
	_devices->SetDevice(_entryData->_device);

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
		_entryData->_device = device;
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
	_entryData->_device.UseForMessageSelection(enable);
	if (enable) {
		_listenTimer.start();
	} else {
		_listenTimer.stop();
	}
}

void MacroConditionMidiEdit::ToggleListen()
{
	if (!_entryData) {
		return;
	}

	if (!_currentlyListening &&
	    _entryData->_device.IsUsedForMessageSelection()) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.midi.startListenFail"));
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
	auto messages = _entryData->_device.GetMessages(true);
	if (!_entryData || !messages || messages->empty()) {
		return;
	}

	_message->SetMessage(messages->back());
	_entryData->_message = messages->back();
	_entryData->_device.ClearMessageBuffer();
}

} // namespace advss
