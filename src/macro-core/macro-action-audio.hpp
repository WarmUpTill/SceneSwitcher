#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "slider-spinbox.hpp"
#include "source-selection.hpp"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>

class MacroActionAudio : public MacroAction {
public:
	MacroActionAudio(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionAudio>(m);
	}

	SourceSelection _audioSource;

	enum class Action {
		MUTE,
		UNMUTE,
		SOURCE_VOLUME,
		MASTER_VOLUME,
		SYNC_OFFSET,
		MONITOR,
		BALANCE,
	};

	enum class FadeType {
		DURATION,
		RATE,
	};

	Action _action = Action::MUTE;
	FadeType _fadeType = FadeType::DURATION;
	int64_t _syncOffset = 0;
	obs_monitoring_type _monitorType = OBS_MONITORING_TYPE_NONE;
	double _balance = 0.5;
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
	void SourceChanged(const SourceSelection &);
	void ActionChanged(int value);
	void SyncOffsetChanged(int value);
	void MonitorTypeChanged(int value);
	void BalanceChanged(double value);
	void VolumeChanged(int value);
	void FadeChanged(int value);
	void DurationChanged(const Duration &seconds);
	void RateChanged(double value);
	void WaitChanged(int value);
	void AbortActiveFadeChanged(int value);
	void FadeTypeChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	QComboBox *_actions;
	QComboBox *_fadeTypes;
	QSpinBox *_syncOffset;
	QComboBox *_monitorTypes;
	SliderSpinBox *_balance;
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
