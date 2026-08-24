#pragma once
#include "macro-condition-edit.hpp"
#include "regex-config.hpp"
#include "variable-line-edit.hpp"

#include <QComboBox>

namespace advss {

class MacroConditionNowPlaying : public MacroCondition {
public:
	enum class CheckType {
		PLAYBACK_STATE,
		TITLE,
		ARTIST,
		ALBUM,
		APP_NAME,
	};

	enum class PlaybackState {
		PLAYING,
		PAUSED,
		STOPPED,
		OPENING,
		CHANGING,
	};

	MacroConditionNowPlaying(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; }
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionNowPlaying>(m);
	}

	CheckType _checkType = CheckType::PLAYBACK_STATE;
	PlaybackState _playbackState = PlaybackState::PLAYING;
	StringVariable _matchText;
	RegexConfig _regex;

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroConditionNowPlayingEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionNowPlayingEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionNowPlaying> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionNowPlayingEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionNowPlaying>(
				cond));
	}

private slots:
	void CheckTypeChanged(int);
	void PlaybackStateChanged(int);
	void MatchTextChanged(const QString &);
	void RegexChanged(const RegexConfig &);

private:
	void SetWidgetVisibility();

	QHBoxLayout *_layout;
	QComboBox *_checkType;
	QComboBox *_playbackState;
	VariableLineEdit *_matchText;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionNowPlaying> _entryData;
	bool _loading = true;
};

} // namespace advss
