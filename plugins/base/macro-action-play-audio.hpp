#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "file-selection.hpp"
#include "variable-spinbox.hpp"

#include <obs.hpp>

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QWidget>

namespace advss {

class MacroActionPlayAudio : public MacroAction {
public:
	MacroActionPlayAudio(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; }
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	StringVariable _filePath =
		obs_module_text("AdvSceneSwitcher.enterPath");
	DoubleVariable _volumeDB = 0.0;
	obs_monitoring_type _monitorType = OBS_MONITORING_TYPE_MONITOR_ONLY;
	uint32_t _audioMixers = 0x3F; // tracks 1-6, all on by default
	bool _useStartOffset = false;
	Duration _startOffset;
	bool _useDuration = false;
	Duration _playbackDuration;
	bool _waitForCompletion = false;

	static bool _registered;
	static const std::string id;
};

class MacroActionPlayAudioEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionPlayAudioEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionPlayAudio> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionPlayAudioEdit(
			parent, std::dynamic_pointer_cast<MacroActionPlayAudio>(
					action));
	}

private slots:
	void FilePathChanged(const QString &path);
	void VolumeDBChanged(const NumberVariable<double> &value);
	void MonitorTypeChanged(int value);
	void TrackChanged();
	void UseStartOffsetChanged(int);
	void StartOffsetChanged(const Duration &);
	void UseDurationChanged(int);
	void PlaybackDurationChanged(const Duration &);
	void WaitChanged(int value);

signals:
	void HeaderInfoChanged(const QString &);

private:
	FileSelection *_filePath;
	VariableDoubleSpinBox *_volumeDB;
	QComboBox *_monitorTypes;
	QWidget *_tracksContainer;
	std::array<QCheckBox *, 6> _tracks;
	QCheckBox *_useStartOffset;
	DurationSelection *_startOffset;
	QCheckBox *_useDuration;
	DurationSelection *_playbackDuration;
	QCheckBox *_waitForCompletion;

	std::shared_ptr<MacroActionPlayAudio> _entryData;
	bool _loading = true;
};

} // namespace advss
