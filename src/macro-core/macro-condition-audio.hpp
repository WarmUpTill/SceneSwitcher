#pragma once
#include "macro.hpp"
#include "volume-control.hpp"
#include <limits>
#include <QWidget>
#include <QComboBox>
#include <chrono>

class MacroConditionAudio : public MacroCondition {
public:
	MacroConditionAudio(Macro *m) : MacroCondition(m) {}
	~MacroConditionAudio();
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionAudio>(m);
	}
	static void SetVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	void ResetVolmeter();

	enum class Type {
		OUTPUT_VOLUME,
		CONFIGURED_VOLUME,
		SYNC_OFFSET,
	};

	enum class OutputCondition {
		ABOVE,
		BELOW,
	};

	enum class VolumeCondition {
		ABOVE,
		EXACT,
		BELOW,
		MUTE,
		UNMUTE,
	};

	OBSWeakSource _audioSource;
	int _volume = 0;
	int64_t _syncOffset = 0;
	Type _checkType = Type::OUTPUT_VOLUME;
	OutputCondition _outputCondition = OutputCondition::ABOVE;
	VolumeCondition _volumeCondition = VolumeCondition::ABOVE;
	obs_volmeter_t *_volmeter = nullptr;

private:
	bool CheckOutputCondition();
	bool CheckVolumeCondition();
	bool CheckSyncOffset();

	float _peak = -std::numeric_limits<float>::infinity();
	static bool _registered;
	static const std::string id;
};

class MacroConditionAudioEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionAudioEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionAudio> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionAudioEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionAudio>(cond));
	}
	void UpdateVolmeterSource();

private slots:
	void SourceChanged(const QString &text);
	void VolumeThresholdChanged(int vol);
	void ConditionChanged(int cond);
	void CheckTypeChanged(int cond);
	void SyncOffsetChanged(int value);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_checkTypes;
	QComboBox *_audioSources;
	QComboBox *_condition;
	QSpinBox *_volume;
	QSpinBox *_syncOffset;
	VolControl *_volMeter = nullptr;
	std::shared_ptr<MacroConditionAudio> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
