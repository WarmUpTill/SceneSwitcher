#pragma once
#include <QSpinBox>

#include "switch-generic.hpp"
#include "volume-control.hpp"

constexpr auto audio_func = 8;
constexpr auto default_priority_8 = audio_func;

typedef enum {
	ABOVE,
	BELOW,
} audioCondition;

struct AudioSwitch : virtual SceneSwitcherEntry {
	OBSWeakSource audioSource = nullptr;
	int volumeThreshold = 0;
	audioCondition condition = ABOVE;
	double duration = 0;
	unsigned int matchCount = 0;
	float peak = -1;
	obs_volmeter_t *volmeter = nullptr;

	const char *getType() { return "audio"; }
	bool initialized();
	bool valid();
	static void setVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	void resetVolmeter();

	AudioSwitch(){};
	AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
		    OBSWeakSource audioSource_, int volumeThreshold_,
		    audioCondition condition_, double duration_,
		    bool usePreviousScene_);
	AudioSwitch(const AudioSwitch &other);
	AudioSwitch(AudioSwitch &&other);
	~AudioSwitch();
	AudioSwitch &operator=(const AudioSwitch &other);
	AudioSwitch &operator=(AudioSwitch &&other) noexcept;
	friend void swap(AudioSwitch &first, AudioSwitch &second);
};

class AudioSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	AudioSwitchWidget(AudioSwitch *s);
	void UpdateVolmeterSource();
	AudioSwitch *getSwitchData();
	void setSwitchData(AudioSwitch *s);

	static void swapSwitchData(AudioSwitchWidget *as1,
				   AudioSwitchWidget *as2);

private slots:
	void SourceChanged(const QString &text);
	void VolumeThresholdChanged(int vol);
	void ConditionChanged(int cond);
	void DurationChanged(double dur);

private:
	QComboBox *audioSources;
	QComboBox *condition;
	QSpinBox *audioVolumeThreshold;
	QDoubleSpinBox *duration;
	VolControl *volMeter;

	AudioSwitch *switchData;
};
