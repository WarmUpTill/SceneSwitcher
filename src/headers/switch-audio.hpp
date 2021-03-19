#pragma once
#include <QSpinBox>
#include <limits>

#include "switch-generic.hpp"
#include "volume-control.hpp"

constexpr auto audio_func = 8;
constexpr auto default_priority_8 = audio_func;

typedef enum {
	ABOVE,
	BELOW,
} audioCondition;

struct AudioSwitch : virtual SceneSwitcherEntry {
	static bool pause;
	OBSWeakSource audioSource = nullptr;
	int volumeThreshold = 0;
	audioCondition condition = ABOVE;
	double duration = 0;
	bool ignoreInactiveSource = true;
	unsigned int matchCount = 0;
	float peak = -std::numeric_limits<float>::infinity();
	obs_volmeter_t *volmeter = nullptr;

	const char *getType() { return "audio"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
	static void setVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	void resetVolmeter();

	AudioSwitch(){};
	AudioSwitch(const AudioSwitch &other);
	AudioSwitch(AudioSwitch &&other);
	~AudioSwitch();
	AudioSwitch &operator=(const AudioSwitch &other);
	AudioSwitch &operator=(AudioSwitch &&other) noexcept;
	friend void swap(AudioSwitch &first, AudioSwitch &second);
};

struct AudioSwitchFallback : virtual SceneSwitcherEntry {
	const char *getType() { return "audio_fallback"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	bool enable = false;
	double duration = 0;
	unsigned int matchCount = 0;
};

class AudioSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	AudioSwitchWidget(QWidget *parent, AudioSwitch *s);
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
	void IgnoreInactiveChanged(int state);

private:
	QComboBox *audioSources;
	QComboBox *condition;
	QSpinBox *audioVolumeThreshold;
	QDoubleSpinBox *duration;
	QCheckBox *ignoreInactiveSource;
	VolControl *volMeter;

	AudioSwitch *switchData;
};

class AudioSwitchFallbackWidget : public SwitchWidget {
	Q_OBJECT

public:
	AudioSwitchFallbackWidget(QWidget *parent, AudioSwitchFallback *s);

private slots:
	void DurationChanged(double dur);

private:
	QDoubleSpinBox *duration;

	AudioSwitchFallback *switchData;
};
