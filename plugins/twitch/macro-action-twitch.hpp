#pragma once
#include "macro-action-edit.hpp"
#include "token.hpp"
#include "category-selection.hpp"
#include "channel-selection.hpp"
#include "chat-connection.hpp"
#include "points-reward-selection.hpp"

#include <variable-line-edit.hpp>
#include <variable-text-edit.hpp>
#include <duration-control.hpp>

namespace advss {

class MacroActionTwitch : public MacroAction {
public:
	MacroActionTwitch(Macro *m) : MacroAction(m) {}
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	std::string GetId() const { return id; };
	std::string GetShortDesc() const;

	enum class Action {
		// Channel info update
		CHANNEL_INFO_TITLE_SET = 10,
		CHANNEL_INFO_CATEGORY_SET = 20,
		CHANNEL_INFO_LANGUAGE_SET = 30,
		CHANNEL_INFO_DELAY_SET = 40,
		CHANNEL_INFO_TAGS_SET = 50,           // TODO
		CHANNEL_INFO_CONTENT_LABELS_SET = 60, // TODO
		CHANNEL_INFO_BRANDED_CONTENT_ENABLE = 70,
		CHANNEL_INFO_BRANDED_CONTENT_DISABLE = 71,

		// Raid
		RAID_START = 110,
		RAID_END = 120,

		// Shoutout
		SHOUTOUT_SEND = 200,

		// Poll
		POLL_START = 300, // TODO
		POLL_END = 310,

		// Prediction
		PREDICTION_START = 400, // TODO
		PREDICTION_END = 410,

		// Shield mode
		SHIELD_MODE_START = 500,
		SHIELD_MODE_END = 501,

		// Reward add
		POINTS_REWARD_ADD = 600, // TODO

		// Get reward information
		POINTS_REWARD_GET_INFO = 650,

		// Reward update
		POINTS_REWARD_ENABLE = 710,
		POINTS_REWARD_DISABLE = 711,
		POINTS_REWARD_PAUSE = 712,
		POINTS_REWARD_UNPAUSE = 713,
		POINTS_REWARD_TITLE_SET = 720,
		POINTS_REWARD_PROMPT_SET = 730,
		POINTS_REWARD_COST_SET = 740,
		POINTS_REWARD_BACKGROUND_COLOR_SET = 750, // TODO
		POINTS_REWARD_USER_INPUT_REQUIRE = 760,
		POINTS_REWARD_USER_INPUT_UNREQUIRE = 761,
		POINTS_REWARD_COOLDOWN_ENABLE = 770,
		POINTS_REWARD_COOLDOWN_DISABLE = 771,
		POINTS_REWARD_QUEUE_SKIP_ENABLE = 780,
		POINTS_REWARD_QUEUE_SKIP_DISABLE = 781,
		POINTS_REWARD_MAX_PER_STREAM_ENABLE = 790,
		POINTS_REWARD_MAX_PER_STREAM_DISABLE = 791,
		POINTS_REWARD_MAX_PER_USER_ENABLE = 792,
		POINTS_REWARD_MAX_PER_USER_DISABLE = 793,

		// Reward delete
		POINTS_REWARD_DELETE = 800,

		// Reward redemption update
		POINTS_REWARD_REDEMPTION_FULFILL = 900,
		POINTS_REWARD_REDEMPTION_CANCEL = 910,

		// User state
		USER_BAN = 1000,
		USER_UNBAN = 1100,
		USER_BLOCK = 1200,
		USER_UNBLOCK = 1300,
		USER_MODERATOR_ADD = 1400,
		USER_MODERATOR_DELETE = 1500,
		USER_VIP_ADD = 1600,
		USER_VIP_DELETE = 1700,
		USER_CHAT_COLOR_UPDATE = 1800, // TODO

		// Commercial
		COMMERCIAL_START = 1900,
		COMMERCIAL_SNOOZE = 2000,

		// Marker
		MARKER_CREATE = 2100,

		// Clips
		CLIP_CREATE = 2200,

		// Videos
		VIDEOS_DELETE = 2300, // TODO

		// Chat
		CHAT_MESSAGES_DELETE = 2400, // TODO
		CHAT_ANNOUNCEMENT_SEND = 2500,
		CHAT_EMOTE_ONLY_ENABLE = 2600,
		CHAT_EMOTE_ONLY_DISABLE = 2601,
		CHAT_FOLLOWER_ONLY_ENABLE = 2602,
		CHAT_FOLLOWER_ONLY_DISABLE = 2603,
		CHAT_SUBSCRIBER_ONLY_ENABLE = 2604,
		CHAT_SUBSCRIBER_ONLY_DISABLE = 2605,
		CHAT_SLOW_MODE_ENABLE = 2640,
		CHAT_SLOW_MODE_DISABLE = 2641,
		CHAT_NON_MODERATOR_DELAY_ENABLE = 2650,
		CHAT_NON_MODERATOR_DELAY_DISABLE = 2651,
		CHAT_UNIQUE_MODE_ENABLE = 2960,
		CHAT_UNIQUE_MODE_DISABLE = 2661,
		CHAT_AUTOMOD_UPDATE = 2700,        // TODO
		CHAT_AUTOMOD_MESSAGE_ALLOW = 2800, // TODO
		CHAT_AUTOMOD_MESSAGE_DENY = 2801,  // TODO
		CHAT_BLOCKED_TERM_ADD = 2900,      // TODO
		CHAT_BLOCKED_TERM_DELETE = 3000,   // TODO
		WHISPER_SEND = 3100,

		// Schedule
		CHANNEL_SCHEDULE_UPDATE = 3200,         // TODO
		CHANNEL_SCHEDULE_SEGMENT_ADD = 3300,    // TODO
		CHANNEL_SCHEDULE_SEGMENT_UPDATE = 3400, // TODO
		CHANNEL_SCHEDULE_SEGMENT_DELETE = 3500, // TODO

		SEND_CHAT_MESSAGE = 5000,

		// Get user info
		USER_GET_INFO = 6000
	};

	enum class AnnouncementColor {
		PRIMARY,
		BLUE,
		GREEN,
		ORANGE,
		PURPLE,
	};

	enum class RedemptionStatus { CANCELED, FULFILLED };

	enum class UserInfoQueryType { ID, LOGIN };

	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	void ResolveVariablesToFixedValues();
	void SetAction(Action);
	Action GetAction() const { return _action; }
	bool ActionIsSupportedByToken();
	void ResetChatConnection();

	std::weak_ptr<TwitchToken> _token;
	StringVariable _streamTitle =
		obs_module_text("AdvSceneSwitcher.action.twitch.title.title");
	TwitchCategory _category;
	StringVariable _markerDescription = obs_module_text(
		"AdvSceneSwitcher.action.twitch.marker.description");
	bool _clipHasDelay = false;
	Duration _duration = 60;
	StringVariable _announcementMessage = obs_module_text(
		"AdvSceneSwitcher.action.twitch.announcement.message");
	AnnouncementColor _announcementColor = AnnouncementColor::PRIMARY;
	TwitchChannel _channel;
	StringVariable _chatMessage;
	UserInfoQueryType _userInfoQueryType = UserInfoQueryType::LOGIN;
	StringVariable _userLogin = "user login";
	DoubleVariable _userId = 0;
	TwitchPointsReward _pointsReward;
	std::weak_ptr<Variable> _rewardVariable;
	bool _useVariableForRewardSelection = false;

private:
	void SetStreamTitle(const std::shared_ptr<TwitchToken> &) const;
	void SetStreamCategory(const std::shared_ptr<TwitchToken> &) const;
	void CreateStreamMarker(const std::shared_ptr<TwitchToken> &) const;
	void CreateStreamClip(const std::shared_ptr<TwitchToken> &) const;
	void StartCommercial(const std::shared_ptr<TwitchToken> &) const;
	void SendChatAnnouncement(const std::shared_ptr<TwitchToken> &) const;
	void SetChatEmoteOnlyMode(const std::shared_ptr<TwitchToken> &,
				  bool enable) const;
	void StartRaid(const std::shared_ptr<TwitchToken> &);
	void SendChatMessage(const std::shared_ptr<TwitchToken> &);
	void GetUserInfo(const std::shared_ptr<TwitchToken> &);
	void GetRewardInfo(const std::shared_ptr<TwitchToken> &);

	bool ResolveVariableSelectionToRewardId(
		const std::shared_ptr<TwitchToken> &);

	void SetupTempVars();

	Action _action = Action::CHANNEL_INFO_TITLE_SET;
	std::shared_ptr<TwitchChatConnection> _chatConnection;

	std::string _lastResolvedRewardTitle;
	std::string _lastResolvedRewardId;

	static bool _registered;
	static const std::string id;
};

class MacroActionTwitchEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTwitchEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTwitch> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTwitchEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTwitch>(action));
	}
	void UpdateEntryData();

private slots:
	void ActionChanged(int);
	void TwitchTokenChanged(const QString &);
	void CheckToken();
	void StreamTitleChanged();
	void CategoryChanged(const TwitchCategory &);
	void MarkerDescriptionChanged();
	void ClipHasDelayChanged(int state);
	void DurationChanged(const Duration &);
	void AnnouncementMessageChanged();
	void AnnouncementColorChanged(int index);
	void ChannelChanged(const TwitchChannel &);
	void ChatMessageChanged();
	void UserInfoQueryTypeChanged(int);
	void UserLoginChanged();
	void UserIdChanged(const NumberVariable<double> &);
	void PointsRewardChanged(const TwitchPointsReward &);
	void RewardVariableChanged(const QString &);
	void ToggleRewardSelection(bool);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionTwitch> _entryData;

private:
	void SetWidgetProperties();
	void SetWidgetSignalConnections();
	void SetWidgetLayout();
	void SetWidgetVisibility();
	void SetTokenWarning(bool visible, const QString &text = "");

	QHBoxLayout *_layout;
	FilterComboBox *_actions;
	TwitchConnectionSelection *_tokens;
	QLabel *_tokenWarning;
	QTimer _tokenCheckTimer;
	VariableLineEdit *_streamTitle;
	TwitchCategoryWidget *_category;
	VariableLineEdit *_markerDescription;
	QCheckBox *_clipHasDelay;
	DurationSelection *_duration;
	VariableTextEdit *_announcementMessage;
	QComboBox *_announcementColor;
	TwitchChannelSelection *_channel;
	VariableTextEdit *_chatMessage;
	QComboBox *_userInfoQueryType;
	VariableLineEdit *_userLogin;
	// QSpinBox uses int internally, which is too small for Twitch IDs, so
	// we use QDoubleSpinBox instead
	VariableDoubleSpinBox *_userId;
	TwitchPointsRewardWidget *_pointsReward;
	VariableSelection *_rewardVariable;
	QPushButton *_toggleRewardSelection;

	bool _loading = true;
};

} // namespace advss
