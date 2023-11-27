#pragma once
#include <variable-spinbox.hpp>
#include <variable-number.hpp>
#include <variable-string.hpp>
#include <QComboBox>
#include <obs-data.h>

#define LIBREMIDI_HEADER_ONLY 1
#include <libremidi/libremidi.hpp>

namespace advss {

// Based on https://github.com/nhielost/obs-midi-mg MMGMessage
class MidiMessage {
public:
	MidiMessage() = default;
	MidiMessage(const libremidi::message &message);

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	bool Matches(const MidiMessage &) const;

	static std::string ToString(const libremidi::message &msg);
	static std::string MidiTypeToString(libremidi::message_type type);
	static std::string GetMidiType(const libremidi::message &msg);
	static int GetMidiNote(const libremidi::message &msg);
	static int GetMidiValue(const libremidi::message &msg);

	std::string ToString() const;
	libremidi::message_type Type() const { return _type; }
	int Channel() const { return _channel; }
	int Note() const { return _note; }
	int Value() const { return _value; }

private:
	// Values which don't appear for channel, note, and value will be used
	// to indicate whether this part of the message is optional and can be
	// disregarded (e.g.during  comparison using Matches())
	static const int optionalChannelIndicator = 0;
	static const int optionalNoteIndicator = -1;
	static const int optionalValueIndicator = -1;

	bool _typeIsOptional = true;
	libremidi::message_type _type = libremidi::message_type::INVALID;
	NumberVariable<int> _channel = optionalChannelIndicator;
	NumberVariable<int> _note = optionalNoteIndicator;
	NumberVariable<int> _value = optionalValueIndicator;

	friend class MidiMessageSelection;
};

enum class MidiDeviceType {
	INPUT,
	OUTPUT,
};

class MidiDeviceInstance {
public:
	static MidiDeviceInstance *GetDevice(MidiDeviceType type,
					     const std::string &);
	static MidiDeviceInstance *GetDevice(MidiDeviceType type, int port);
	static void ClearMessageBuffersOfAllDevices();
	static void ResetAllDevices();

private:
	MidiDeviceInstance() = default;
	~MidiDeviceInstance() = default;
	bool OpenPort();
	void ClosePort();
	bool SendMessge(const MidiMessage &);
	const std::vector<MidiMessage> &GetMessages();
	void ReceiveMidiMessage(libremidi::message &&);
	void ClearMessageBuffer();

	static std::map<std::pair<MidiDeviceType, std::string>,
			MidiDeviceInstance *>
		devices;

	bool _skipBufferClear = false;

	MidiDeviceType _type = MidiDeviceType::INPUT;
	std::string _name;
	libremidi::midi_in in =
		libremidi::midi_in(libremidi::input_configuration{
			[this](libremidi::message &&message) {
				ReceiveMidiMessage(std::move(message));
			}});
	libremidi::midi_out out =
		libremidi::midi_out(libremidi::output_configuration());
	std::vector<MidiMessage> _messages;

	friend class MidiDevice;
};

class MidiDevice {
public:
	MidiDevice() = default;

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	bool SendMessge(const MidiMessage &);

	const std::vector<MidiMessage> *            // Might resize! Only call
	GetMessages(bool ignoreListenMode = false); // while holding switcher
						    // lock!
	std::string Name() const;

	// Used for "listen" mode of message selection
	// Listen mode disables automatic clearing of buffers
	void UseForMessageSelection(bool);
	bool IsUsedForMessageSelection();
	void ClearMessageBuffer();
	bool DeviceSelected() { return !!_dev; }

private:
	MidiDeviceType _type = MidiDeviceType::INPUT;
	std::string _name;
	MidiDeviceInstance *_dev = nullptr;

	friend class MidiDeviceSelection;
};

class MidiMessageSelection : public QWidget {
	Q_OBJECT

public:
	MidiMessageSelection(QWidget *parent);
	void SetMessage(const MidiMessage &);

private slots:
	void TypeChanged(const QString &);
	void ChannelChanged(const NumberVariable<int> &);
	void NoteChanged(const NumberVariable<int> &);
	void NoteStringIdxChanged(int);
	void ShowNote(bool);
	void ValueChanged(const NumberVariable<int> &);

signals:
	void MidiMessageChanged(const MidiMessage &);

private:
	static libremidi::message_type TextToMidiType(const QString &);

	QComboBox *_type;
	VariableSpinBox *_channel;
	VariableSpinBox *_noteValue;
	QComboBox *_noteString;
	QPushButton *_noteValueStringToggle;
	VariableSpinBox *_value;

	MidiMessage _currentSelection;
};

class MidiDeviceSelection : public QComboBox {
	Q_OBJECT

public:
	MidiDeviceSelection(QWidget *parent, MidiDeviceType);
	void SetDevice(const MidiDevice &);

private slots:
	void IdxChangedHelper(int);
signals:
	void DeviceSelectionChanged(const MidiDevice &);

private:
	const MidiDeviceType _type;
};

QStringList GetAllNotes();

} // namespace advss
