#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "slider-spinbox.hpp"
#include "source-selection.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

namespace advss {

class MacroActionAudio : public MacroAction {
public:
	MacroActionAudio(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	SourceSelection _audioSource;

	enum class Action {
		MUTE,
		UNMUTE,
		SOURCE_VOLUME,
		MASTER_VOLUME,
		SYNC_OFFSET,
		MONITOR,
		BALANCE,
		ENABLE_ON_TRACK,
		DISABLE_ON_TRACK,
		TOGGLE_MUTE,
	};

	enum class FadeType {
		DURATION,
		RATE,
	};

	Action _action = Action::MUTE;
	FadeType _fadeType = FadeType::DURATION;
	IntVariable _syncOffset = 0;
	obs_monitoring_type _monitorType = OBS_MONITORING_TYPE_NONE;
	DoubleVariable _balance = 0.5;
	IntVariable _track = 1;
	bool _useDb = false;
	IntVariable _volume = 0;
	DoubleVariable _volumeDB = 0.0;
	bool _fade = false;
	Duration _duration;
	DoubleVariable _rate = 100.;
	bool _wait = false;
	bool _abortActiveFade = false;

private:
	void StartFade() const;
	void FadeVolume() const;
	void SetVolume(float vol) const;
	float GetCurrentVolume() const;
	void SetFadeActive(bool value) const;
	bool FadeActive() const;
	std::atomic_int *GetFadeIdPtr() const;
	float GetVolume() const;

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
	void SourceChanged(const SourceSelection &);
	void ActionChanged(int value);
	void SyncOffsetChanged(const NumberVariable<int> &value);
	void MonitorTypeChanged(int value);
	void BalanceChanged(const NumberVariable<double> &value);
	void TrackChanged(const NumberVariable<int> &value);
	void VolumeChanged(const NumberVariable<int> &value);
	void VolumeDBChanged(const NumberVariable<double> &value);
	void PercentDBClicked();
	void FadeChanged(int value);
	void DurationChanged(const Duration &seconds);
	void RateChanged(const NumberVariable<double> &value);
	void WaitChanged(int value);
	void AbortActiveFadeChanged(int value);
	void FadeTypeChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	QComboBox *_actions;
	QComboBox *_fadeTypes;
	VariableSpinBox *_syncOffset;
	QComboBox *_monitorTypes;
	SliderSpinBox *_balance;
	VariableSpinBox *_track;
	VariableSpinBox *_volumePercent;
	VariableDoubleSpinBox *_volumeDB;
	QPushButton *_percentDBToggle;
	QCheckBox *_fade;
	DurationSelection *_duration;
	VariableDoubleSpinBox *_rate;
	QCheckBox *_wait;
	QCheckBox *_abortActiveFade;
	QHBoxLayout *_fadeTypeLayout;
	QVBoxLayout *_fadeOptionsLayout;
	std::shared_ptr<MacroActionAudio> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
