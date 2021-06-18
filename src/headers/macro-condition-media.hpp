#pragma once
#include "macro.hpp"
#include <limits>
#include <QWidget>
#include <QComboBox>

#include "duration-control.hpp"

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
	PLAYED_TO_END = custom_media_states_offset,
	ANY,
};

class MacroConditionMedia : public MacroCondition {
public:
	~MacroConditionMedia();
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionMedia>();
	}
	void ClearSignalHandler();
	void ResetSignalHandler();
	static void MediaStopped(void *data, calldata_t *);
	static void MediaEnded(void *data, calldata_t *);

	OBSWeakSource _source = nullptr;
	MediaState _state = MediaState::OBS_MEDIA_STATE_NONE;
	MediaTimeRestriction _restriction =
		MediaTimeRestriction::TIME_RESTRICTION_NONE;
	Duration _time;

private:
	std::atomic_bool _stopped = {false};
	std::atomic_bool _ended = {false};
	// Trigger scene change only once even if media state might trigger repeatedly
	bool _alreadyMatched = false;
	// Workaround to enable use of "ended" to specify end of VLC playlist
	bool _previousStateEnded = false;
	bool _playedToEnd = false;

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
	void StateChanged(int index);
	void TimeRestrictionChanged(int index);
	void TimeChanged(double seconds);
	void TimeUnitChanged(DurationUnit unit);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_mediaSources;
	QComboBox *_states;
	QComboBox *_timeRestrictions;
	DurationSelection *_time;
	std::shared_ptr<MacroConditionMedia> _entryData;

private:
	bool _loading = true;
};
