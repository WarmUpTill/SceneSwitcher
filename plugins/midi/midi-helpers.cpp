#include "midi-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <obs-module-helper.hpp>
#include <plugin-state-helpers.hpp>
#include <selection-helpers.hpp>
#include <sync-helpers.hpp>
#include <ui-helpers.hpp>
#include <utility.hpp>

namespace advss {

static std::map<std::pair<MidiDeviceType, int>, MidiDeviceInstance *>
SetupMidiMessageVector()
{
	AddIntervalResetStep(
		MidiDeviceInstance::ClearMessageBuffersOfAllDevices);
	return {};
}

std::map<std::pair<MidiDeviceType, int>, MidiDeviceInstance *>
	MidiDeviceInstance::devices = SetupMidiMessageVector();

void MidiDeviceInstance::ClearMessageBuffersOfAllDevices()
{
	for (auto const &[_, device] : MidiDeviceInstance::devices) {
		if (device->_skipBufferClear) {
			continue;
		}
		device->ClearMessageBuffer();
	}
}

void MidiDeviceInstance::ResetAllDevices()
{
	for (auto const &[_, device] : MidiDeviceInstance::devices) {
		if (device->_skipBufferClear) {
			continue;
		}
		device->ClosePort();
		device->ClearMessageBuffer();
		device->OpenPort();
	}
}

void MidiDeviceInstance::ClearMessageBuffer()
{
	_messages.clear();
}

MidiMessage::MidiMessage(const libremidi::message &message)
{
	_typeIsOptional = false;
	_type = message.get_message_type();
	_channel = message.get_channel();
	_note = GetMidiNote(message);
	_value = GetMidiValue(message);
}

void MidiMessage::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "typeIsOptional", _typeIsOptional);
	obs_data_set_int(data, "type", static_cast<int>(_type));
	_channel.Save(data, "channel");
	_note.Save(data, "note");
	_value.Save(data, "value");
	obs_data_set_obj(obj, "midiMessage", data);
	obs_data_release(data);
}

void MidiMessage::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "midiMessage");
	_typeIsOptional = obs_data_get_bool(data, "typeIsOptional");
	_type = static_cast<libremidi::message_type>(
		obs_data_get_int(data, "type"));
	_channel.Load(data, "channel");
	_note.Load(data, "note");
	_value.Load(data, "value");
	obs_data_release(data);
}

int MidiMessage::GetMidiNote(const libremidi::message &msg)
{
	switch (msg.get_message_type()) {
	case libremidi::message_type::NOTE_OFF:
	case libremidi::message_type::NOTE_ON:
	case libremidi::message_type::CONTROL_CHANGE:
		return msg[1];
	default:
		return 0;
	}
}

int MidiMessage::GetMidiValue(const libremidi::message &msg)
{
	switch (msg.get_message_type()) {
	case libremidi::message_type::NOTE_ON:
	case libremidi::message_type::NOTE_OFF:
	case libremidi::message_type::CONTROL_CHANGE:
	case libremidi::message_type::PITCH_BEND:
		return msg[2];
	case libremidi::message_type::PROGRAM_CHANGE:
		return msg[1];
	default:
		return -1;
	}
}

std::string MidiMessage::ToString() const
{
	return "Type: " + MidiTypeToString(_type) +
	       " Note: " + std::to_string(_note) +
	       " Channel: " + std::to_string(_channel) +
	       " Value: " + std::to_string(_value);
}

std::string MidiMessage::ToString(const libremidi::message &msg)
{
	return "Type: " + GetMidiType(msg) +
	       " Note: " + std::to_string(GetMidiNote(msg)) +
	       " Channel: " + std::to_string(msg.get_channel()) +
	       " Value: " + std::to_string(GetMidiValue(msg));
}

std::string MidiMessage::MidiTypeToString(libremidi::message_type type)
{
	switch (type) {
	case libremidi::message_type::INVALID:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.invalid");
	// Standard Messages
	case libremidi::message_type::NOTE_OFF:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.noteOff");
	case libremidi::message_type::NOTE_ON:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.noteOn");
	case libremidi::message_type::POLY_PRESSURE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.polyphonicPressure");
	case libremidi::message_type::CONTROL_CHANGE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.controlChange");
	case libremidi::message_type::PROGRAM_CHANGE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.programChange");
	case libremidi::message_type::AFTERTOUCH:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.channelAftertouch");
	case libremidi::message_type::PITCH_BEND:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.pitchBend");
	// System Common Messages
	case libremidi::message_type::SYSTEM_EXCLUSIVE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.systemExclusive");
	case libremidi::message_type::TIME_CODE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.timeCode");
	case libremidi::message_type::SONG_POS_POINTER:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.songPositionPointer");
	case libremidi::message_type::SONG_SELECT:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.songSelect");
	case libremidi::message_type::RESERVED1:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.reserved1");
	case libremidi::message_type::RESERVED2:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.reserved2");
	case libremidi::message_type::TUNE_REQUEST:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.tuneRequest");
	case libremidi::message_type::EOX:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.endOfSystemExclusive");
	// System Realtime Messages
	case libremidi::message_type::TIME_CLOCK:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.timeClock");
	case libremidi::message_type::RESERVED3:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.reserved3");
	case libremidi::message_type::START:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.startFile");
	case libremidi::message_type::CONTINUE:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.continueFile");
	case libremidi::message_type::STOP:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.stopFile");
	case libremidi::message_type::RESERVED4:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.reserved4");
	case libremidi::message_type::ACTIVE_SENSING:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.activeSensing");
	case libremidi::message_type::SYSTEM_RESET:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.systemReset");
	default:
		return obs_module_text(
			"AdvSceneSwitcher.midi.message.type.unknown");
	}
	return "";
}

std::string MidiMessage::GetMidiType(const libremidi::message &msg)
{
	return MidiTypeToString(msg.get_message_type());
}

bool MidiMessage::Matches(const MidiMessage &m) const
{
	const bool channelMatch = _channel == optionalChannelIndicator ||
				  m._channel == optionalChannelIndicator ||
				  (_channel == m._channel);
	const bool noteMatch = _note == optionalNoteIndicator ||
			       m._note == optionalNoteIndicator ||
			       (_note == m._note);
	const bool valueMatch = _value == optionalValueIndicator ||
				m._value == optionalValueIndicator ||
				(_value == m._value);
	const bool typeMatch = _typeIsOptional || m._typeIsOptional ||
			       (_type == m._type);
	return channelMatch && noteMatch && valueMatch && typeMatch;
}

MidiDeviceInstance *MidiDeviceInstance::GetDevice(MidiDeviceType type, int port)
{
	if (port < 0) {
		return nullptr;
	}

	auto key = std::make_pair(type, port);
	auto it = devices.find(key);
	if (it != devices.end()) {
		it->second->OpenPort();
		return it->second;
	}

	try {
		auto device = new MidiDeviceInstance();
		device->_type = type;
		device->_port = port;
		devices[key] = device;
		device->OpenPort();
		return device;
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING,
		     "Failed to create midi %s device instance for port #%d: %s",
		     type == MidiDeviceType::INPUT ? "input" : "output", port,
		     error.what());
	}
	return nullptr;
}

void MidiDevice::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, "type", static_cast<int>(_type));
	obs_data_set_int(data, "port", _dev ? _dev->_port : -1);
	obs_data_set_obj(obj, "midiDevice", data);
	obs_data_release(data);
}

void MidiDevice::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "midiDevice");
	_type = static_cast<MidiDeviceType>(obs_data_get_int(data, "type"));
	obs_data_set_default_int(data, "port", -1);
	_port = obs_data_get_int(data, "port");
	_dev = MidiDeviceInstance::GetDevice(_type, _port);
	obs_data_release(data);
}

bool MidiDevice::SendMessge(const MidiMessage &m)
{
	if (_type == MidiDeviceType::INPUT || _port == -1 || !_dev) {
		return false;
	}

	return _dev->SendMessge(m);
}

bool MidiDeviceInstance::OpenPort()
{
	if ((_type == MidiDeviceType::INPUT && in.is_port_open()) ||
	    (_type == MidiDeviceType::OUTPUT && out.is_port_open())) {
		return true;
	}

	if (_type == MidiDeviceType::OUTPUT) {
		try {
			out.open_port(_port);
			blog(LOG_INFO, "Opened output midi port #%d", _port);
			return true;
		} catch (const libremidi::driver_error &error) {
			blog(LOG_WARNING,
			     "Failed to open output midi port #%d: %s", _port,
			     error.what());
		} catch (const libremidi::system_error &error) {
			blog(LOG_WARNING,
			     "Failed to open output midi port #%d: %s", _port,
			     error.what());
		} catch (const libremidi::midi_exception &error) {
			blog(LOG_WARNING,
			     "Failed to open output midi port #%d: %s", _port,
			     error.what());
		}
		return false;
	}

	auto cb = [this](const libremidi::message &m) {
		this->ReceiveMidiMessage(m);
	};

	in.set_callback(cb);
	try {
		in.open_port(_port);
		blog(LOG_INFO, "Opened input midi port #%d", _port);
		return true;
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING, "Failed to open input midi port #%d: %s",
		     _port, error.what());
	} catch (const libremidi::system_error &error) {
		blog(LOG_WARNING, "Failed to open input midi port #%d: %s",
		     _port, error.what());
	} catch (const libremidi::midi_exception &error) {
		blog(LOG_WARNING, "Failed to open input midi port #%d: %s",
		     _port, error.what());
	}
	return false;
}

void MidiDeviceInstance::ClosePort()
{
	if ((_type == MidiDeviceType::INPUT && !in.is_port_open()) ||
	    (_type == MidiDeviceType::OUTPUT && !out.is_port_open())) {
		return;
	}

	if (_type == MidiDeviceType::OUTPUT) {
		try {
			out.close_port();
			blog(LOG_INFO, "Closed output midi port #%d", _port);
		} catch (const libremidi::driver_error &error) {
			blog(LOG_WARNING,
			     "Failed to close output midi port #%d: %s", _port,
			     error.what());
		} catch (const libremidi::system_error &error) {
			blog(LOG_WARNING,
			     "Failed to close output midi port #%d: %s", _port,
			     error.what());
		} catch (const libremidi::midi_exception &error) {
			blog(LOG_WARNING,
			     "Failed to close output midi port #%d: %s", _port,
			     error.what());
		}
		return;
	}

	auto cb = [this](const libremidi::message &m) {
		this->ReceiveMidiMessage(m);
	};

	in.set_callback(cb);
	try {
		in.close_port();
		blog(LOG_INFO, "Closed input midi port #%d", _port);
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING, "Failed to close input midi port #%d: %s",
		     _port, error.what());
	} catch (const libremidi::system_error &error) {
		blog(LOG_WARNING, "Failed to close input midi port #%d: %s",
		     _port, error.what());
	} catch (const libremidi::midi_exception &error) {
		blog(LOG_WARNING, "Failed to close input midi port #%d: %s",
		     _port, error.what());
	}
}

bool MidiDeviceInstance::SendMessge(const MidiMessage &m)
{
	libremidi::message message;
	int channel = m.Channel();
	int note = m.Note();
	int value = m.Value();

	switch (m.Type()) {
	case libremidi::message_type::NOTE_OFF:
		message = libremidi::message::note_off(channel, note, value);
		break;
	case libremidi::message_type::NOTE_ON:
		message = libremidi::message::note_on(channel, note, value);
		break;
	case libremidi::message_type::CONTROL_CHANGE:
		message = libremidi::message::control_change(channel, note,
							     value);
		break;
	case libremidi::message_type::PROGRAM_CHANGE:
		message = libremidi::message::program_change(channel, value);
		break;
	case libremidi::message_type::PITCH_BEND:
		message = libremidi::message::pitch_bend(
			channel, (value <= 64 ? 0 : value - 64) * 2, value);
		break;
	case libremidi::message_type::POLY_PRESSURE:
		message =
			libremidi::message::poly_pressure(channel, note, value);
		break;
	case libremidi::message_type::AFTERTOUCH:
		message = libremidi::message::aftertouch(channel, value);
		break;
	default:
		message = {libremidi::message::make_command(m.Type(), channel),
			   (unsigned char)note, (unsigned char)value};
		blog(LOG_WARNING,
		     "sending midi message of non-default type \"%s\"",
		     MidiMessage::MidiTypeToString(m.Type()).c_str());
		break;
	}

	try {
		out.send_message(message);
		return true;
	} catch (const libremidi::driver_error &err) {
		blog(LOG_WARNING, "%s", err.what());
	}

	return false;
}

const std::vector<MidiMessage> &MidiDeviceInstance::GetMessages()
{
	return _messages;
}

void MidiDeviceInstance::ReceiveMidiMessage(const libremidi::message &msg)
{
	auto lock = LockContext();
	_messages.emplace_back(msg);
	vblog(LOG_INFO, "received midi: %s",
	      MidiMessage::ToString(msg).c_str());
}

void MidiDevice::UseForMessageSelection(bool skipBufferClear)
{
	if (!_dev) {
		return;
	}

	blog(LOG_INFO, "%s \"listen\" mode for midi input device \"%s\"! %s",
	     skipBufferClear ? "Enable" : "Disable", Name().c_str(),
	     skipBufferClear
		     ? "This will block incoming messages from being processed!"
		     : "");
	ClearMessageBuffer();
	_dev->_skipBufferClear = skipBufferClear;
}

bool MidiDevice::IsUsedForMessageSelection()
{
	return _dev && _dev->_skipBufferClear;
}

void MidiDevice::ClearMessageBuffer()
{
	if (_dev) {
		_dev->ClearMessageBuffer();
	}
}

const std::vector<MidiMessage> *MidiDevice::GetMessages(bool ignoreSkip)
{
	if (_type == MidiDeviceType::OUTPUT || _port == -1 || !_dev ||
	    (_dev->_skipBufferClear && !ignoreSkip)) {
		return nullptr;
	}

	return &_dev->GetMessages();
}

static QString portToName(bool input, int port)
{
	std::string name;
	try {
		if (input) {
			auto midiin = libremidi::midi_in();
			name = midiin.get_port_name(port);
		} else {
			auto midiout = libremidi::midi_out();
			name = midiout.get_port_name(port);
		}
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING,
		     "Failed to get midi %s device name of port #%d: %s",
		     input ? "input" : "output", port, error.what());
	}

	const QString deviceNamePattern =
		obs_module_text("AdvSceneSwitcher.midi.deviceNamePattern");
	return deviceNamePattern.arg(QString::number(port),
				     QString::fromStdString(name));
}

std::string MidiDevice::GetInputName() const
{
	return portToName(true, _port).toStdString();
}

std::string MidiDevice::GetOutputName() const
{
	return portToName(false, _port).toStdString();
}

std::string MidiDevice::Name() const
{
	if (_port < 0) {
		return "";
	}

	if (_type == MidiDeviceType::INPUT) {
		return GetInputName();
	}
	return GetOutputName();
}

static inline QStringList getInputDeviceNames()
{
	QStringList devices;
	try {
		auto midiin = libremidi::midi_in();
		auto nPorts = midiin.get_port_count();
		for (unsigned i = 0; i < nPorts; i++) {
			devices << portToName(true, i);
		}
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING, "Failed to get midi input devices: %s",
		     error.what());
	}
	return devices;
}

static inline QStringList getOutputDeviceNames()
{
	QStringList devices;
	try {
		auto midiout = libremidi::midi_out();
		auto nPorts = midiout.get_port_count();
		for (unsigned i = 0; i < nPorts; i++) {
			devices << portToName(false, i);
		}
	} catch (const libremidi::driver_error &error) {
		blog(LOG_WARNING, "Failed to get midi output devices: %s",
		     error.what());
	}
	return devices;
}

MidiDeviceSelection::MidiDeviceSelection(QWidget *parent, MidiDeviceType t)
	: QComboBox(parent), _type(t)
{
	AddSelectionEntry(this, obs_module_text("AdvSceneSwitcher.selectItem"));

	if (_type == MidiDeviceType::INPUT) {
		addItems(getInputDeviceNames());
	} else {
		addItems(getOutputDeviceNames());
	}

	QWidget::connect(this, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(IdxChangedHelper(int)));
}

void MidiDeviceSelection::SetDevice(const MidiDevice &_dev)
{
	setCurrentText(QString::fromStdString(_dev.Name()));
}

void MidiDeviceSelection::IdxChangedHelper(int idx)
{
	if (idx == 0) {
		emit DeviceSelectionChanged(MidiDevice());
	}

	auto devInstance = MidiDeviceInstance::GetDevice(_type, idx - 1);
	if (!devInstance) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.midi.deviceOpenFail"));
		const QSignalBlocker b(this);
		setCurrentIndex(0);
		idx = 0;
	}

	MidiDevice dev;
	dev._type = _type;
	dev._port = idx - 1;
	dev._dev = devInstance;
	emit DeviceSelectionChanged(dev);
}

static void populateMidiMessageTypeSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.midi.message.type.optional"));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::NOTE_OFF)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::NOTE_ON)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::POLY_PRESSURE)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::CONTROL_CHANGE)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::PROGRAM_CHANGE)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::AFTERTOUCH)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::PITCH_BEND)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::SYSTEM_EXCLUSIVE)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::TIME_CODE)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::SONG_POS_POINTER)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::SONG_SELECT)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::RESERVED1)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::RESERVED2)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::TUNE_REQUEST)));
	list->addItem(QString::fromStdString(
		MidiMessage::MidiTypeToString(libremidi::message_type::EOX)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::TIME_CLOCK)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::RESERVED3)));
	list->addItem(QString::fromStdString(
		MidiMessage::MidiTypeToString(libremidi::message_type::START)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::CONTINUE)));
	list->addItem(QString::fromStdString(
		MidiMessage::MidiTypeToString(libremidi::message_type::STOP)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::RESERVED4)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::ACTIVE_SENSING)));
	list->addItem(QString::fromStdString(MidiMessage::MidiTypeToString(
		libremidi::message_type::SYSTEM_RESET)));
}

MidiMessageSelection::MidiMessageSelection(QWidget *parent)
	: QWidget(parent),
	  _type(new QComboBox()),
	  _channel(new VariableSpinBox()),
	  _noteValue(new VariableSpinBox()),
	  _noteString(new QComboBox()),
	  _noteValueStringToggle(new QPushButton()),
	  _value(new VariableSpinBox())
{
	populateMidiMessageTypeSelection(_type);

	_noteString->addItem(
		obs_module_text("AdvSceneSwitcher.midi.message.placeholder"));
	_noteString->addItems(GetAllNotes());
	_noteString->setEditable(true);
	_noteString->setInsertPolicy(QComboBox::NoInsert);

	_noteValueStringToggle->setMaximumWidth(22);
	_noteValueStringToggle->setCheckable(true);
	const auto path = GetDataFilePath("res/images/" + GetThemeTypeName() +
					  "Note.svg");
	SetButtonIcon(_noteValueStringToggle, path.c_str());

	_channel->specialValueText(
		obs_module_text("AdvSceneSwitcher.midi.message.placeholder"));
	_noteValue->specialValueText(
		obs_module_text("AdvSceneSwitcher.midi.message.placeholder"));
	_value->specialValueText(
		obs_module_text("AdvSceneSwitcher.midi.message.placeholder"));

	_channel->setMinimum(MidiMessage::optionalChannelIndicator);
	_noteValue->setMinimum(MidiMessage::optionalNoteIndicator);
	_value->setMinimum(MidiMessage::optionalValueIndicator);

	_channel->setMaximum(16);
	_noteValue->setMaximum(127);
	_value->setMaximum(127);

	connect(_type, SIGNAL(currentTextChanged(const QString &)), this,
		SLOT(TypeChanged(const QString &)));
	connect(_channel,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(ChannelChanged(const NumberVariable<int> &)));
	connect(_noteValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(NoteChanged(const NumberVariable<int> &)));
	connect(_noteString, SIGNAL(currentIndexChanged(int)), this,
		SLOT(NoteStringIdxChanged(int)));
	connect(_noteValueStringToggle, SIGNAL(toggled(bool)), this,
		SLOT(ShowNote(bool)));
	connect(_value,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(ValueChanged(const NumberVariable<int> &)));

	auto layout = new QGridLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.midi.message.type")),
			  0, 0);
	layout->addWidget(_type, 0, 1);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.midi.message.channel")),
			  1, 0);
	layout->addWidget(_channel, 1, 1);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.midi.message.note")),
			  2, 0);
	auto noteLayout = new QHBoxLayout;
	noteLayout->addWidget(_noteValue);
	noteLayout->addWidget(_noteString);
	noteLayout->addWidget(_noteValueStringToggle);
	noteLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(noteLayout, 2, 1);
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.midi.message.value")),
			  3, 0);
	layout->addWidget(_value, 3, 1);

	// Reduce label column to its minimum size
	MinimizeSizeOfColumn(layout, 0);

	setLayout(layout);

	ShowNote(false);
}

void MidiMessageSelection::SetMessage(const MidiMessage &m)
{
	_currentSelection = m;
	const QSignalBlocker b(this);
	if (m._typeIsOptional) {
		_type->setCurrentText(obs_module_text(
			"AdvSceneSwitcher.midi.message.type.optional"));
	} else {
		_type->setCurrentText(QString::fromStdString(
			MidiMessage::MidiTypeToString(m._type)));
	}
	_channel->SetValue(m._channel);
	_noteValue->SetValue(m._note);
	_noteString->setCurrentIndex(m._note + 1);
	_value->SetValue(m._value);
}

libremidi::message_type
MidiMessageSelection::TextToMidiType(const QString &text)
{
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::NOTE_OFF)) {
		return libremidi::message_type::NOTE_OFF;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::NOTE_ON)) {
		return libremidi::message_type::NOTE_ON;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::POLY_PRESSURE)) {
		return libremidi::message_type::POLY_PRESSURE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::CONTROL_CHANGE)) {
		return libremidi::message_type::CONTROL_CHANGE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::PROGRAM_CHANGE)) {
		return libremidi::message_type::PROGRAM_CHANGE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::AFTERTOUCH)) {
		return libremidi::message_type::AFTERTOUCH;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::PITCH_BEND)) {
		return libremidi::message_type::PITCH_BEND;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::SYSTEM_EXCLUSIVE)) {
		return libremidi::message_type::SYSTEM_EXCLUSIVE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::TIME_CODE)) {
		return libremidi::message_type::TIME_CODE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::SONG_POS_POINTER)) {
		return libremidi::message_type::SONG_POS_POINTER;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::SONG_SELECT)) {
		return libremidi::message_type::SONG_SELECT;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::RESERVED1)) {
		return libremidi::message_type::RESERVED1;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::RESERVED2)) {
		return libremidi::message_type::RESERVED2;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::TUNE_REQUEST)) {
		return libremidi::message_type::TUNE_REQUEST;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::EOX)) {
		return libremidi::message_type::EOX;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::TIME_CLOCK)) {
		return libremidi::message_type::TIME_CLOCK;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::RESERVED3)) {
		return libremidi::message_type::RESERVED3;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::START)) {
		return libremidi::message_type::START;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::CONTINUE)) {
		return libremidi::message_type::CONTINUE;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::STOP)) {
		return libremidi::message_type::STOP;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(libremidi::message_type::RESERVED4)) {
		return libremidi::message_type::RESERVED4;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::ACTIVE_SENSING)) {
		return libremidi::message_type::ACTIVE_SENSING;
	}
	if (text.toStdString() ==
	    MidiMessage::MidiTypeToString(
		    libremidi::message_type::SYSTEM_RESET)) {
		return libremidi::message_type::SYSTEM_RESET;
	}
	return libremidi::message_type::INVALID;
}

void MidiMessageSelection::TypeChanged(const QString &text)
{
	_currentSelection._typeIsOptional =
		text ==
		obs_module_text("AdvSceneSwitcher.midi.message.type.optional");
	if (_currentSelection._typeIsOptional) {
		emit MidiMessageChanged(_currentSelection);
		return;
	}

	_currentSelection._type = TextToMidiType(text);
	emit MidiMessageChanged(_currentSelection);
}

void MidiMessageSelection::ChannelChanged(const NumberVariable<int> &c)
{
	_currentSelection._channel = c;
	emit MidiMessageChanged(_currentSelection);
}

void MidiMessageSelection::NoteChanged(const NumberVariable<int> &n)
{
	const QSignalBlocker b(_noteString);
	_noteString->setCurrentIndex(n.GetFixedValue() + 1);
	_currentSelection._note = n;
	emit MidiMessageChanged(_currentSelection);
}

void MidiMessageSelection::NoteStringIdxChanged(int value)
{
	const QSignalBlocker b(_noteValue);
	_currentSelection._note = value;
	_noteValue->SetFixedValue(value - 1);
	emit MidiMessageChanged(_currentSelection);
}

void MidiMessageSelection::ShowNote(bool show)
{
	_noteValue->setVisible(!show);
	_noteString->setVisible(show);
}

void MidiMessageSelection::ValueChanged(const NumberVariable<int> &v)
{
	_currentSelection._value = v;
	emit MidiMessageChanged((_currentSelection));
}

QStringList GetAllNotes()
{
	QStringList result;
	QStringList notes = {"C",  "C#", "D",  "D#", "E",  "F",
			     "F#", "G",  "G#", "A",  "A#", "B"};
	for (int octave = -1; octave <= 9; octave++) {
		for (int noteIndex = 0; noteIndex < 12; noteIndex++) {
			result << notes[noteIndex] + QString::number(octave);
		}
	}
	return result;
}

} // namespace advss
