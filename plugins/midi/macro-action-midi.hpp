#pragma once
#include "macro-action-edit.hpp"
#include "midi-helpers.hpp"

#include <QPushButton>
#include <QTimer>

namespace advss {

class MacroActionMidi : public MacroAction {
public:
	MacroActionMidi(Macro *m) : MacroAction(m, true) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionMidi>(m);
	}

	MidiDevice _device;
	MidiMessage _message;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionMidiEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionMidiEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionMidi> entryData = nullptr);
	virtual ~MacroActionMidiEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionMidiEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionMidi>(action));
	}

private slots:
	void DeviceSelectionChanged(const MidiDevice &);
	void ListenDeviceSelectionChanged(const MidiDevice &);
	void MidiMessageChanged(const MidiMessage &);
	void ResetMidiDevices();
	void ToggleListen();
	void SetMessageSelectionToLastReceived();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void EnableListening(bool);

	std::shared_ptr<MacroActionMidi> _entryData;

	MidiDeviceSelection *_devices;
	MidiMessageSelection *_message;
	MidiDeviceSelection *_listenDevices;
	QPushButton *_resetMidiDevices;
	QPushButton *_listen;
	MidiDevice _listenDevice;
	QTimer _listenTimer;
	bool _currentlyListening = false;
	bool _loading = true;
};

} // namespace advss
