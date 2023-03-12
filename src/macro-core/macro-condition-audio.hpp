#pragma once
#include "macro.hpp"
#include "volume-control.hpp"
#include "slider-spinbox.hpp"
#include "source-selection.hpp"

#include <limits>
#include <QWidget>
#include <QComboBox>
#include <chrono>

class MacroConditionAudio : public MacroCondition {
public:
	MacroConditionAudio(Macro *m) : MacroCondition(m, true) {}
	~MacroConditionAudio();
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
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
		MONITOR,
		BALANCE,
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

	SourceSelection _audioSource;
	NumberVariable<int> _volume = 0;
	NumberVariable<int> _syncOffset = 0;
	obs_monitoring_type _monitorType = OBS_MONITORING_TYPE_NONE;
	NumberVariable<double> _balance = 0.5;
	Type _checkType = Type::OUTPUT_VOLUME;
	OutputCondition _outputCondition = OutputCondition::ABOVE;
	VolumeCondition _volumeCondition = VolumeCondition::ABOVE;
	obs_volmeter_t *_volmeter = nullptr;

private:
	bool CheckOutputCondition();
	bool CheckVolumeCondition();
	bool CheckSyncOffset();
	bool CheckMonitor();
	bool CheckBalance();

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
	void SourceChanged(const SourceSelection &);
	void VolumeThresholdChanged(const NumberVariable<int> &vol);
	void ConditionChanged(int cond);
	void CheckTypeChanged(int cond);
	void SyncOffsetChanged(const NumberVariable<int> &value);
	void MonitorTypeChanged(int value);
	void BalanceChanged(const NumberVariable<double> &value);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_checkTypes;
	SourceSelectionWidget *_sources;
	QComboBox *_condition;
	VariableSpinBox *_volume;
	VariableSpinBox *_syncOffset;
	QComboBox *_monitorTypes;
	SliderSpinBox *_balance;
	VolControl *_volMeter = nullptr;
	std::shared_ptr<MacroConditionAudio> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
