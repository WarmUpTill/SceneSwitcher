#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

#include <QSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>

enum class AudioAction {
	MUTE,
	UNMUTE,
	SOURCE_VOLUME,
	MASTER_VOLUME,
};

class MacroActionAudio : public MacroAction {
public:
	MacroActionAudio(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionAudio>(m);
	}

	OBSWeakSource _audioSource;
	AudioAction _action = AudioAction::MUTE;
	int _volume = 0;
	bool _fade = false;
	Duration _duration;
	bool _wait = false;

private:
	void StartSourceFade();
	void StartMasterFade();
	void FadeSourceVolume();
	void FadeMasterVolume();

	static bool _registered;
	static const std::string id;
};

class MacroActionAudioEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionAudioEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionAudio> entryData = nullptr);
	void SetWidgetVisibility();
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
	void FadeChanged(int value);
	void DurationChanged(double seconds);
	void WaitChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_audioSources;
	QComboBox *_actions;
	QSpinBox *_volumePercent;
	QCheckBox *_fade;
	DurationSelection *_duration;
	QCheckBox *_wait;
	QHBoxLayout *_fadeLayout;
	std::shared_ptr<MacroActionAudio> _entryData;

private:
	bool _loading = true;
};
