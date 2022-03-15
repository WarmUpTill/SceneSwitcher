#pragma once
#include "macro.hpp"
#include <limits>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>

#include "duration-control.hpp"
#include "scene-selection.hpp"

enum class MediaTimeRestriction {
	TIME_RESTRICTION_NONE,
	TIME_RESTRICTION_SHORTER,
	TIME_RESTRICTION_LONGER,
	TIME_RESTRICTION_REMAINING_SHORTER,
	TIME_RESTRICTION_REMAINING_LONGER,
};

constexpr auto custom_media_states_offset = 100;

enum class MediaState {
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
	PLAYLIST_ENDED = custom_media_states_offset,
	ANY,
};

enum class MediaSourceType {
	SOURCE,
	ANY,
	ALL,
};

class MacroConditionMedia : public MacroCondition {
public:
	MacroConditionMedia(Macro *m) : MacroCondition(m) {}
	~MacroConditionMedia();
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionMedia>(m);
	}
	void ClearSignalHandler();
	void ResetSignalHandler();
	static void MediaStopped(void *data, calldata_t *);
	static void MediaEnded(void *data, calldata_t *);
	static void MediaNext(void *data, calldata_t *);

	MediaSourceType _sourceType = MediaSourceType::SOURCE;
	SceneSelection _scene;
	OBSWeakSource _source = nullptr;
	std::vector<MacroConditionMedia> _sources;
	MediaState _state = MediaState::OBS_MEDIA_STATE_NONE;
	MediaTimeRestriction _restriction =
		MediaTimeRestriction::TIME_RESTRICTION_NONE;
	Duration _time;
	bool _onlyMatchonChagne = false;

private:
	bool CheckTime();
	bool CheckState();
	bool CheckPlaylistEnd(const obs_media_state);
	bool CheckMediaMatch();

	bool _stopped = false;
	bool _ended = false;
	bool _next = false;
	// TODO: Remove _alreadyMatched as it does not make much sense when
	// time restrictions for macro conditions are available.
	bool _alreadyMatched = false;
	// Workaround to enable use of "ended" to specify end of VLC playlist
	bool _previousStateEnded = false;

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
	void SourceChanged(const QString &text);
	void SceneChanged(const SceneSelection &);
	void StateChanged(int index);
	void TimeRestrictionChanged(int index);
	void TimeChanged(double seconds);
	void TimeUnitChanged(DurationUnit unit);
	void OnChangeChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_mediaSources;
	QComboBox *_states;
	QComboBox *_timeRestrictions;
	DurationSelection *_time;
	QCheckBox *_onChange;
	std::shared_ptr<MacroConditionMedia> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
