#include "macro-action-midi.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroActionMidi::id = "midi";

bool MacroActionMidi::_registered = MacroActionFactory::Register(
	MacroActionMidi::id,
	{MacroActionMidi::Create, MacroActionMidiEdit::Create,
	 "AdvSceneSwitcher.action.midi"});

bool MacroActionMidi::PerformAction()
{
	if (!_device.SendMessge(_message)) {
		blog(LOG_WARNING,
		     "failed to send midi message \"%s\" to \"%s\"",
		     _message.ToString().c_str(), _device.Name().c_str());
	}
	return true;
}

void MacroActionMidi::LogAction() const
{
	vblog(LOG_INFO, "send midi message \"%s\" to \"%s\"",
	      _message.ToString().c_str(), _device.Name().c_str());
}

bool MacroActionMidi::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_message.Save(obj);
	_device.Save(obj);
	return true;
}

bool MacroActionMidi::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_message.Load(obj);
	_device.Load(obj);
	return true;
}

std::string MacroActionMidi::GetShortDesc() const
{
	return _device.Name();
}

MacroActionMidiEdit::MacroActionMidiEdit(
	QWidget *parent, std::shared_ptr<MacroActionMidi> entryData)
	: QWidget(parent),
	  _devices(new MidiDeviceSelection(this, MidiDeviceType::OUTPUT)),
	  _message(new MidiMessageSelection(this)),
	  _resetMidiDevices(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.midi.resetDevices"))),
	  _listenDevices(new MidiDeviceSelection(this, MidiDeviceType::INPUT)),
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
	QWidget::connect(
		_listenDevices,
		SIGNAL(DeviceSelectionChanged(const MidiDevice &)), this,
		SLOT(ListenDeviceSelectionChanged(const MidiDevice &)));
	QWidget::connect(&_listenTimer, SIGNAL(timeout()), this,
			 SLOT(SetMessageSelectionToLastReceived()));

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.midi.entry"),
		     entryLayout, {{"{{device}}", _devices}});
	auto listenLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.midi.entry.listen"),
		listenLayout,
		{{"{{listenButton}}", _listen},
		 {"{{listenDevices}}", _listenDevices}});

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

MacroActionMidiEdit::~MacroActionMidiEdit()
{
	EnableListening(false);
}

void MacroActionMidiEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_message->SetMessage(_entryData->_message);
	_devices->SetDevice(_entryData->_device);

	adjustSize();
	updateGeometry();
}

void MacroActionMidiEdit::DeviceSelectionChanged(const MidiDevice &device)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_device = device;
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionMidiEdit::ListenDeviceSelectionChanged(const MidiDevice &dev)
{
	if (_currentlyListening) {
		ToggleListen();
	}
	_listenDevice = dev;
}

void MacroActionMidiEdit::MidiMessageChanged(const MidiMessage &message)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = message;
}

void MacroActionMidiEdit::EnableListening(bool enable)
{
	if (_currentlyListening == enable) {
		return;
	}
	_listenDevice.UseForMessageSelection(enable);
	if (enable) {
		_listenTimer.start();
	} else {
		_listenTimer.stop();
	}
}

void MacroActionMidiEdit::ResetMidiDevices()
{
	auto lock = LockContext();
	MidiDeviceInstance::ResetAllDevices();
}

void MacroActionMidiEdit::ToggleListen()
{
	if (!_entryData || !_listenDevice.DeviceSelected()) {
		return;
	}

	if (!_currentlyListening && _listenDevice.IsUsedForMessageSelection()) {
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

void MacroActionMidiEdit::SetMessageSelectionToLastReceived()
{
	auto lock = LockContext();
	auto messages = _listenDevice.GetMessages(true);
	if (!_entryData || !messages || messages->empty()) {
		return;
	}

	_message->SetMessage(messages->back());
	_entryData->_message = messages->back();
	_listenDevice.ClearMessageBuffer();
}

} // namespace advss
