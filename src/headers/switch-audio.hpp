#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto audio_func = 8;
constexpr auto default_priority_8 = audio_func;

struct AudioSwitch : virtual SceneSwitcherEntry {
	OBSWeakSource audioSource;
	int volumeThreshold;
	float peak;
	std::string audioSwitchStr;
	obs_volmeter_t *volmeter;

	const char *getType() { return "audio"; }

	static void setVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS])
	{
		UNUSED_PARAMETER(magnitude);
		UNUSED_PARAMETER(inputPeak);
		AudioSwitch *s = static_cast<AudioSwitch *>(data);

		s->peak = peak[0];
		for (int i = 1; i < MAX_AUDIO_CHANNELS; i++)
			if (peak[i] > s->peak)
				s->peak = peak[i];
	}

	bool valid()
	{
		return (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(audioSource) &&
		       WeakSourceValid(transition);
	}

	inline AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			   OBSWeakSource audioSource_, int volumeThreshold_,
			   bool usePreviousScene_, std::string audioSwitchStr_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  audioSource(audioSource_),
		  volumeThreshold(volumeThreshold_),
		  audioSwitchStr(audioSwitchStr_)
	{
		volmeter = obs_volmeter_create(OBS_FADER_LOG);
		obs_volmeter_add_callback(volmeter, setVolumeLevel, this);
		obs_source_t *as = obs_weak_source_get_source(audioSource);
		if (!obs_volmeter_attach_source(volmeter, as)) {
			const char *name = obs_source_get_name(as);
			blog(LOG_WARNING,
			     "failed to attach volmeter to source %s", name);
		}
		obs_source_release(as);
	}

	AudioSwitch(const AudioSwitch &other)
		: SceneSwitcherEntry(other.scene, other.transition,
				     other.usePreviousScene),
		  audioSource(other.audioSource),
		  volumeThreshold(other.volumeThreshold),
		  audioSwitchStr(other.audioSwitchStr)
	{
		volmeter = obs_volmeter_create(OBS_FADER_LOG);
		obs_volmeter_add_callback(volmeter, setVolumeLevel, this);
		obs_source_t *as =
			obs_weak_source_get_source(other.audioSource);
		if (!obs_volmeter_attach_source(volmeter, as)) {
			const char *name = obs_source_get_name(as);
			blog(LOG_WARNING,
			     "failed to attach volmeter to source %s", name);
		}
		obs_source_release(as);
	}

	AudioSwitch(AudioSwitch &&other)
		: SceneSwitcherEntry(other.scene, other.transition,
				     other.usePreviousScene),
		  audioSource(other.audioSource),
		  volumeThreshold(other.volumeThreshold),
		  audioSwitchStr(other.audioSwitchStr),
		  volmeter(other.volmeter)
	{
		other.volmeter = nullptr;
	}

	inline ~AudioSwitch()
	{
		obs_volmeter_remove_callback(volmeter, setVolumeLevel, this);
		obs_volmeter_destroy(volmeter);
	}

	AudioSwitch &operator=(const AudioSwitch &other)
	{
		return *this = AudioSwitch(other);
	}

	AudioSwitch &operator=(AudioSwitch &&other) noexcept
	{
		if (this == &other) {
			return *this;
		}
		obs_volmeter_destroy(volmeter);
		volmeter = other.volmeter;
		other.volmeter = nullptr;
		return *this;
	}
};

static inline QString MakeAudioSwitchName(const QString &scene,
					  const QString &transition,
					  const QString &audioSrouce,
					  const int &volume);
