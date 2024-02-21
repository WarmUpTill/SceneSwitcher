#pragma once
#include "macro-condition-edit.hpp"
#include "midi-helpers.hpp"

#include <QPushButton>
#include <QTimer>

namespace advss {

class MacroConditionMidi : public MacroCondition {
public:
	MacroConditionMidi(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionMidi>(m);
	}

	void SetDevice(const MidiDevice &dev);
	const MidiDevice &GetDevice() const { return _device; }
	MidiMessage _message;

private:
	void SetupTempVars();
	void SetVariableValues(const MidiMessage &);

	MidiDevice _device;
	MidiMessageBuffer _messageBuffer;
	std::chrono::high_resolution_clock::time_point _lastCheck{};
	static bool _registered;
	static const std::string id;
};

class MacroConditionMidiEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionMidiEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionMidi> cond = nullptr);
	virtual ~MacroConditionMidiEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionMidiEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionMidi>(cond));
	}

private slots:
	void DeviceSelectionChanged(const MidiDevice &);
	void MidiMessageChanged(const MidiMessage &);
	void ResetMidiDevices();
	void ToggleListen();
	void SetMessageSelectionToLastReceived();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void EnableListening(bool);

	std::shared_ptr<MacroConditionMidi> _entryData;

	MidiDeviceSelection *_devices;
	MidiMessageSelection *_message;
	QPushButton *_resetMidiDevices;
	QPushButton *_listen;
	QTimer _listenTimer;
	MidiMessageBuffer _messageBuffer;
	bool _currentlyListening = false;
	bool _loading = true;
};

} // namespace advss
