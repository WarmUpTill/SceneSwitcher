#pragma once
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <string>

#include "utility.hpp"
#include "switch-generic.hpp"
#include "volume-control.hpp"

constexpr auto audio_func = 8;
constexpr auto default_priority_8 = audio_func;

struct AudioSwitch : virtual SceneSwitcherEntry {
	OBSWeakSource audioSource = nullptr;
	int volumeThreshold = 0;
	float peak = -1;
	std::string audioSwitchStr;
	obs_volmeter_t *volmeter = nullptr;

	const char *getType() { return "audio"; }
	bool valid();
	static void setVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	void resetVolmeter();

	inline AudioSwitch(){};
	inline AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			   OBSWeakSource audioSource_, int volumeThreshold_,
			   bool usePreviousScene_, std::string audioSwitchStr_);
	AudioSwitch(const AudioSwitch &other);
	AudioSwitch(AudioSwitch &&other);
	inline ~AudioSwitch();
	AudioSwitch &operator=(const AudioSwitch &other);
	AudioSwitch &operator=(AudioSwitch &&other) noexcept;
};

static inline QString MakeAudioSwitchName(const QString &scene,
					  const QString &transition,
					  const QString &audioSrouce,
					  const int &volume);

class AudioSwitchWidget : public QWidget {
	Q_OBJECT

public:
	AudioSwitchWidget(AudioSwitch *s);
	void UpdateVolmeterSource();

private slots:
	void SceneChanged(const QString &text);
	void TransitionChanged(const QString &text);
	void SourceChanged(const QString &text);
	void VolumeThresholdChanged(int vol);

private:
	bool loading = true;

	QComboBox *audioScenes;
	QComboBox *audioTransitions;
	QComboBox *audioSources;
	QSpinBox *audioVolumeThreshold;

	VolControl *volMeter;

	QLabel *whenLabel;
	QLabel *aboveLabel;
	QLabel *switchLabel;
	QLabel *usingLabel;

	AudioSwitch *switchData;
};
