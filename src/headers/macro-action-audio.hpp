#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>

enum class AudioAction {
	MUTE,
	UNMUTE,
	SOURCE_VOLUME,
	MASTER_VOLUME,
};

enum class FadeType {
	DURATION,
	RATE,
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
	FadeType _fadeType = FadeType::DURATION;
	int _volume = 0;
	bool _fade = false;
	Duration _duration;
	double _rate = 100.;
	bool _wait = false;
	bool _abortActiveFade = false;

private:
	void StartFade();
	void FadeVolume();
	void SetVolume(float vol);
	float GetVolume();
	void SetFadeActive(bool value);
	bool FadeActive();
	std::atomic_int *GetFadeIdPtr();

	static bool _registered;
	static const std::string id;
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
	void FadeChanged(int value);
	void DurationChanged(double seconds);
	void RateChanged(double value);
	void WaitChanged(int value);
	void AbortActiveFadeChanged(int value);
	void FadeTypeChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_audioSources;
	QComboBox *_actions;
	QComboBox *_fadeTypes;
	QSpinBox *_volumePercent;
	QCheckBox *_fade;
	DurationSelection *_duration;
	QDoubleSpinBox *_rate;
	QCheckBox *_wait;
	QCheckBox *_abortActiveFade;
	QHBoxLayout *_fadeTypeLayout;
	QVBoxLayout *_fadeOptionsLayout;
	std::shared_ptr<MacroActionAudio> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
