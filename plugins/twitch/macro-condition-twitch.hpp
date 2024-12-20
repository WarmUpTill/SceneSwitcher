#pragma once
#include "macro-condition-edit.hpp"
#include "token.hpp"
#include "category-selection.hpp"
#include "channel-selection.hpp"
#include "points-reward-selection.hpp"
#include "chat-connection.hpp"
#include "chat-message-pattern.hpp"

#include <variable-line-edit.hpp>
#include <variable-text-edit.hpp>
#include <regex-config.hpp>

namespace advss {

class MacroConditionTwitch : public MacroCondition {
public:
	MacroConditionTwitch(Macro *m) : MacroCondition(m, true) {}
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionTwitch>(m);
	}
	std::string GetId() const { return id; };
	std::string GetShortDesc() const;

	enum class Condition {
		// Event based
		STREAM_ONLINE_EVENT = 0,
		STREAM_ONLINE_LIVE_EVENT = 1,
		STREAM_ONLINE_PLAYLIST_EVENT = 2,
		STREAM_ONLINE_WATCHPARTY_EVENT = 3,
		STREAM_ONLINE_PREMIERE_EVENT = 4,
		STREAM_ONLINE_RERUN_EVENT = 5,
		STREAM_OFFLINE_EVENT = 100,
		CHANNEL_INFO_UPDATE_EVENT = 200,
		FOLLOW_EVENT = 300,
		SUBSCRIPTION_START_EVENT = 400,
		SUBSCRIPTION_END_EVENT = 500,
		SUBSCRIPTION_GIFT_EVENT = 600,
		SUBSCRIPTION_MESSAGE_EVENT = 700,
		CHEER_EVENT = 800,
		RAID_OUTBOUND_EVENT = 900,
		RAID_INBOUND_EVENT = 1000,
		SHOUTOUT_OUTBOUND_EVENT = 1100,
		SHOUTOUT_INBOUND_EVENT = 1200,
		POLL_START_EVENT = 1300,
		POLL_PROGRESS_EVENT = 1400,
		POLL_END_EVENT = 1500,
		PREDICTION_START_EVENT = 1600,
		PREDICTION_PROGRESS_EVENT = 1700,
		PREDICTION_LOCK_EVENT = 1800,
		PREDICTION_END_EVENT = 1900,
		GOAL_START_EVENT = 2000,
		GOAL_PROGRESS_EVENT = 2100,
		GOAL_END_EVENT = 2200,
		HYPE_TRAIN_START_EVENT = 2300,
		HYPE_TRAIN_PROGRESS_EVENT = 2400,
		HYPE_TRAIN_END_EVENT = 2500,
		CHARITY_CAMPAIGN_START_EVENT = 2600,
		CHARITY_CAMPAIGN_PROGRESS_EVENT = 2700,
		CHARITY_CAMPAIGN_DONATION_EVENT = 2800,
		CHARITY_CAMPAIGN_END_EVENT = 2900,
		SHIELD_MODE_START_EVENT = 3000,
		SHIELD_MODE_END_EVENT = 3100,
		POINTS_REWARD_ADDITION_EVENT = 3200,
		POINTS_REWARD_UPDATE_EVENT = 3300,
		POINTS_REWARD_DELETION_EVENT = 3400,
		POINTS_REWARD_REDEMPTION_EVENT = 3500,
		POINTS_REWARD_REDEMPTION_UPDATE_EVENT = 3600,
		USER_BAN_EVENT = 3700,
		USER_UNBAN_EVENT = 3800,
		USER_MODERATOR_ADDITION_EVENT = 3900,
		USER_MODERATOR_DELETION_EVENT = 4000,

		// Chat
		CHAT_MESSAGE_RECEIVED = 500000,
		CHAT_USER_JOINED = 500100,
		CHAT_USER_LEFT = 500200,

		// Polling
		LIVE_POLLING = 1000000,
		TITLE_POLLING = 1000100,
		CATEGORY_POLLING = 1000200,
	};

	void SetCondition(const Condition &condition);
	Condition GetCondition() const { return _condition; }
	void SetToken(const std::weak_ptr<TwitchToken> &);
	std::weak_ptr<TwitchToken> GetToken() const { return _token; }
	void SetChannel(const TwitchChannel &channel);
	TwitchChannel GetChannel() const { return _channel; }
	void SetPointsReward(const TwitchPointsReward &pointsReward);
	TwitchPointsReward GetPointsReward() const { return _pointsReward; }
	void ResetChatConnection();
	bool IsUsingEventSubCondition();

	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool ConditionIsSupportedByToken();

	TwitchChannel _channel;
	TwitchPointsReward _pointsReward;
	StringVariable _streamTitle = obs_module_text(
		"AdvSceneSwitcher.condition.twitch.title.title");
	RegexConfig _regexTitle = RegexConfig::PartialMatchRegexConfig();
	ChatMessagePattern _chatMessagePattern;
	TwitchCategory _category;
	bool _clearBufferOnMatch = false;

private:
	bool CheckChannelGenericEvents();
	bool CheckChannelLiveEvents();
	bool CheckChatMessages(TwitchToken &token);
	bool CheckChatUserJoinOrLeave(TwitchToken &token);

	void RegisterEventSubscription();
	void ResetSubscription();
	void SetupEventSubscription(EventSub &);
	bool EventSubscriptionIsSetup(const std::shared_ptr<EventSub> &);
	void AddChannelGenericEventSubscription(
		const char *version, bool includeModeratorId = false,
		const char *mainUserIdFieldName = "broadcaster_user_id",
		obs_data_t *extraConditions = nullptr);

	void HandleMacroPause();

	void SetupTempVars();
	void SetTempVarValues(const ChannelLiveInfo &);
	void SetTempVarValues(const ChannelInfo &);

	Condition _condition = Condition::LIVE_POLLING;

	std::weak_ptr<TwitchToken> _token;

	EventSubMessageBuffer _eventBuffer;
	std::future<std::string> _subscriptionIDFuture;
	std::string _subscriptionID;

	ChatMessageBuffer _chatBuffer;
	std::shared_ptr<TwitchChatConnection> _chatConnection;

	std::chrono::high_resolution_clock::time_point _lastCheck{};

	static bool _registered;
	static const std::string id;
};

class MacroConditionTwitchEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionTwitchEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionTwitch> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionTwitchEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionTwitch>(cond));
	}
	void UpdateEntryData();

private slots:
	void ConditionChanged(int);
	void TwitchTokenChanged(const QString &);
	void CheckToken();
	void ChannelChanged(const TwitchChannel &);
	void PointsRewardChanged(const TwitchPointsReward &);
	void StreamTitleChanged();
	void RegexTitleChanged(const RegexConfig &);
	void ChatMessagePatternChanged(const ChatMessagePattern &);
	void CategoreyChanged(const TwitchCategory &);
	void ClearBufferOnMatchChanged(int);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	void SetTokenWarning(bool visible, const QString &text = "");

	QHBoxLayout *_layout;
	FilterComboBox *_conditions;
	TwitchConnectionSelection *_tokens;
	QLabel *_tokenWarning;
	QTimer _tokenCheckTimer;
	TwitchChannelSelection *_channel;
	TwitchPointsRewardWidget *_pointsReward;
	VariableLineEdit *_streamTitle;
	RegexConfigWidget *_regexTitle;
	ChatMessageEdit *_chatMesageEdit;
	TwitchCategoryWidget *_category;
	QCheckBox *_clearBufferOnMatch;

	std::shared_ptr<MacroConditionTwitch> _entryData;
	bool _loading = true;
};

} // namespace advss
