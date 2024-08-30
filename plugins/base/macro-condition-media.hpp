#pragma once
#include "macro-condition-edit.hpp"
#include "duration-control.hpp"
#include "scene-selection.hpp"
#include "source-selection.hpp"

#include <limits>
#include <obs.hpp>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>

namespace advss {

class MacroConditionMedia : public MacroCondition {
public:
	MacroConditionMedia(Macro *m) : MacroCondition(m) {}
	MacroConditionMedia(const MacroConditionMedia &);
	MacroConditionMedia &operator=(const MacroConditionMedia &);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionMedia>(m);
	}
	void ResetSignalHandler();
	void UpdateMediaSourcesOfSceneList();
	static void MediaStopped(void *data, calldata_t *);
	static void MediaEnded(void *data, calldata_t *);
	static void MediaNext(void *data, calldata_t *);

	enum class SourceType { SOURCE, ANY, ALL };
	SourceType _sourceType = SourceType::SOURCE;

	enum class CheckType { STATE, TIME, LEGACY = 1000 };
	CheckType _checkType = CheckType::STATE;

	enum class State {
		// OBS's internal states
		OBS_MEDIA_STATE_NONE,
		OBS_MEDIA_STATE_PLAYING,
		OBS_MEDIA_STATE_OPENING,
		OBS_MEDIA_STATE_BUFFERING,
		OBS_MEDIA_STATE_PAUSED,
		OBS_MEDIA_STATE_STOPPED,
		OBS_MEDIA_STATE_ENDED,
		OBS_MEDIA_STATE_ERROR,
		// Just a marker
		LAST_OBS_MEDIA_STATE,
		// states added for use in the plugin
		PLAYLIST_ENDED = 100,
		ANY
	};
	State _state = State::OBS_MEDIA_STATE_PLAYING;

	enum class Time {
		TIME_RESTRICTION_NONE,
		TIME_RESTRICTION_SHORTER,
		TIME_RESTRICTION_LONGER,
		TIME_RESTRICTION_REMAINING_SHORTER,
		TIME_RESTRICTION_REMAINING_LONGER
	};
	Time _timeRestriction = Time::TIME_RESTRICTION_REMAINING_SHORTER;

	SceneSelection _scene;
	SourceSelection _source;
	OBSWeakSource _rawSource = nullptr;
	std::vector<MacroConditionMedia> _sourceGroup;
	Duration _time;

private:
	bool IsUsingLegacyCheck() const;
	bool CheckTime();
	bool CheckState();
	bool CheckPlaylistEnd(const obs_media_state);
	bool CheckMediaMatch();
	void HandleSceneChange();

	std::vector<OBSSignal> _signals;

	bool _stopped = false;
	bool _ended = false;
	bool _next = false;

	// Workaround to enable use of "ended" to specify end of VLC playlist
	bool _previousStateEnded = false;
	// Used to keep track of scene changes
	OBSWeakSource _lastConfigureScene;

	static bool _registered;
	static const std::string id;
};

class MacroConditionMediaEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionMediaEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionMedia> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionMediaEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionMedia>(cond));
	}

private slots:
	void CheckTypeChanged(int);
	void SourceTypeChanged(int);
	void SourceChanged(const SourceSelection &);
	void SceneChanged(const SceneSelection &);
	void StateChanged(int index);
	void TimeRestrictionChanged(int index);
	void TimeChanged(const Duration &seconds);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sourceTypes;
	QComboBox *_checkTypes;
	SceneSelectionWidget *_scenes;
	SourceSelectionWidget *_sources;
	QComboBox *_states;
	QComboBox *_timeRestrictions;
	DurationSelection *_time;
	std::shared_ptr<MacroConditionMedia> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
