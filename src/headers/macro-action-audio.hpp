#pragma once
#include <QSpinBox>
#include "macro-action-edit.hpp"

enum class AudioAction {
	MUTE,
	UNMUTE,
	SOURCE_VOLUME,
	MASTER_VOLUME,
};

class MacroActionAudio : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionAudio>();
	}

	OBSWeakSource _audioSource;
	AudioAction _action = AudioAction::MUTE;
	int _volume = 0;

private:
	static bool _registered;
	static const int id;
};

class MacroActionAudioEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionAudioEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionAudio> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionAudioEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionAudio>(action));
	}

private slots:
	void SourceChanged(const QString &text);
	void ActionChanged(int value);
	void VolumeChanged(int value);

protected:
	QComboBox *_audioSources;
	QComboBox *_actions;
	QSpinBox *_volumePercent;
	std::shared_ptr<MacroActionAudio> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
