#pragma once
#include "macro-condition-edit.hpp"
#include "volume-control.hpp"
#include "slider-spinbox.hpp"
#include "source-selection.hpp"

#include <limits>
#include <QWidget>
#include <QComboBox>
#include <chrono>

namespace advss {

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
	void SetType(const Type &);
	Type GetType() const { return _checkType; }

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
	bool _useDb = false;
	DoubleVariable _volumePercent = 0;
	DoubleVariable _volumeDB = 0.0;
	IntVariable _syncOffset = 0;
	obs_monitoring_type _monitorType = OBS_MONITORING_TYPE_NONE;
	DoubleVariable _balance = 0.5;
	OutputCondition _outputCondition = OutputCondition::ABOVE;
	VolumeCondition _volumeCondition = VolumeCondition::ABOVE;
	obs_volmeter_t *_volmeter = nullptr;

private:
	bool CheckOutputCondition();
	bool CheckVolumeCondition();
	bool CheckSyncOffset();
	bool CheckMonitor();
	bool CheckBalance();
	void SetupTempVars();
	float GetVolumePeak();

	Type _checkType = Type::OUTPUT_VOLUME;
	std::mutex _peakMutex;
	float _peak = -std::numeric_limits<float>::infinity();
	float _previousPeak = -std::numeric_limits<float>::infinity();
	bool _peakUpdated = false;
	std::chrono::high_resolution_clock::time_point _lastPeakUpdate = {};
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
	void VolumePercentChanged(const NumberVariable<double> &vol);
	void ConditionChanged(int cond);
	void CheckTypeChanged(int cond);
	void SyncOffsetChanged(const NumberVariable<int> &value);
	void MonitorTypeChanged(int value);
	void BalanceChanged(const NumberVariable<double> &value);
	void VolumeDBChanged(const NumberVariable<double> &value);
	void PercentDBClicked();
	void SyncSliderAndValueSelection(bool sliderMoved);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	bool HasVolumeControl() const;

	QComboBox *_checkTypes;
	SourceSelectionWidget *_sources;
	QComboBox *_condition;
	VariableDoubleSpinBox *_volumePercent;
	VariableDoubleSpinBox *_volumeDB;
	QPushButton *_percentDBToggle;
	VariableSpinBox *_syncOffset;
	QComboBox *_monitorTypes;
	SliderSpinBox *_balance;
	VolControl *_volMeter = nullptr;

	std::shared_ptr<MacroConditionAudio> _entryData;
	bool _loading = true;
};

} // namespace advss
