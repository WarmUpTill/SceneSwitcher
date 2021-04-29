#pragma once
#include "macro.hpp"
#include "volume-control.hpp"
#include <limits>
#include <QWidget>
#include <QComboBox>
#include <chrono>

enum class AudioCondition {
	ABOVE,
	BELOW,
};

class MacroConditionAudio : public MacroCondition {
public:
	~MacroConditionAudio();
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionAudio>();
	}
	static void SetVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	void ResetVolmeter();

	OBSWeakSource _audioSource;
	int _volume = 0;
	AudioCondition _condition = AudioCondition::ABOVE;
	double _duration = 0;
	obs_volmeter_t *_volmeter = nullptr;

private:
	float _peak = -std::numeric_limits<float>::infinity();
	std::chrono::high_resolution_clock::time_point _startTime;
	static bool _registered;
	static const int id;
};

class MacroConditionAudioEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionAudioEdit(
		std::shared_ptr<MacroConditionAudio> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionAudioEdit(
			std::dynamic_pointer_cast<MacroConditionAudio>(cond));
	}
	void UpdateVolmeterSource();

private slots:
	void SourceChanged(const QString &text);
	void VolumeThresholdChanged(int vol);
	void ConditionChanged(int cond);
	void DurationChanged(double dur);

protected:
	QComboBox *_audioSources;
	QComboBox *_condition;
	QSpinBox *_volume;
	QDoubleSpinBox *_duration;
	VolControl *_volMeter = nullptr;
	std::shared_ptr<MacroConditionAudio> _entryData;

private:
	bool _loading = true;
};
