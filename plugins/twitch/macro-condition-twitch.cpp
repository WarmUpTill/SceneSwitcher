#include "macro-condition-twitch.hpp"
#include "twitch-helpers.hpp"

#include <layout-helpers.hpp>
#include <macro-helpers.hpp>
#include <log-helper.hpp>
#include <nlohmann/json.hpp>

namespace advss {

const std::string MacroConditionTwitch::id = "twitch";

bool MacroConditionTwitch::_registered = MacroConditionFactory::Register(
	MacroConditionTwitch::id,
	{MacroConditionTwitch::Create, MacroConditionTwitchEdit::Create,
	 "AdvSceneSwitcher.condition.twitch"});

std::string MacroConditionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

const static std::map<MacroConditionTwitch::Condition, std::string> conditionTypes = {
	{MacroConditionTwitch::Condition::STREAM_ONLINE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online"},
	{MacroConditionTwitch::Condition::STREAM_OFFLINE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.offline"},
	{MacroConditionTwitch::Condition::CHANNEL_INFO_UPDATE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.info.update"},
	{MacroConditionTwitch::Condition::FOLLOW_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.follow"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.subscription.start"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.subscription.end"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_GIFT_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.subscription.gift"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_MESSAGE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.subscription.message"},
	{MacroConditionTwitch::Condition::CHEER_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.cheer"},
	{MacroConditionTwitch::Condition::RAID_OUTBOUND_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.raid.outbound"},
	{MacroConditionTwitch::Condition::RAID_INBOUND_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.raid.inbound"},
	{MacroConditionTwitch::Condition::SHOUTOUT_OUTBOUND_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.shoutout.outbound"},
	{MacroConditionTwitch::Condition::SHOUTOUT_INBOUND_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.shoutout.inbound"},
	{MacroConditionTwitch::Condition::POLL_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.poll.start"},
	{MacroConditionTwitch::Condition::POLL_PROGRESS_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.poll.progress"},
	{MacroConditionTwitch::Condition::POLL_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.poll.end"},
	{MacroConditionTwitch::Condition::PREDICTION_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.prediction.start"},
	{MacroConditionTwitch::Condition::PREDICTION_PROGRESS_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.prediction.progress"},
	{MacroConditionTwitch::Condition::PREDICTION_LOCK_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.prediction.lock"},
	{MacroConditionTwitch::Condition::PREDICTION_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.prediction.end"},
	{MacroConditionTwitch::Condition::GOAL_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.goal.start"},
	{MacroConditionTwitch::Condition::GOAL_PROGRESS_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.goal.progress"},
	{MacroConditionTwitch::Condition::GOAL_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.goal.end"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.hypeTrain.start"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_PROGRESS_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.hypeTrain.progress"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.hypeTrain.end"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.charityCampaign.start"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.charityCampaign.progress"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_DONATION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.charityCampaign.donation"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.charityCampaign.end"},
	{MacroConditionTwitch::Condition::SHIELD_MODE_START_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.shieldMode.start"},
	{MacroConditionTwitch::Condition::SHIELD_MODE_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.shieldMode.end"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_ADDITION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.points.reward.addition"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_UPDATE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.points.reward.update"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_DELETION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.points.reward.deletion"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_REDEMPTION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.points.reward.redemption"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.points.reward.redemption.update"},
	{MacroConditionTwitch::Condition::USER_BAN_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.user.ban"},
	{MacroConditionTwitch::Condition::USER_UNBAN_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.user.unban"},
	{MacroConditionTwitch::Condition::USER_MODERATOR_ADDITION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.user.moderator.addition"},
	{MacroConditionTwitch::Condition::USER_MODERATOR_DELETION_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.user.moderator.deletion"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_LIVE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online.live"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_PLAYLIST_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online.playlist"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_WATCHPARTY_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online.watchParty"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_PREMIERE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online.premiere"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_RERUN_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.event.channel.stream.online.rerun"},
	{MacroConditionTwitch::Condition::LIVE_POLLING,
	 "AdvSceneSwitcher.condition.twitch.type.polling.channel.live"},
	{MacroConditionTwitch::Condition::TITLE_POLLING,
	 "AdvSceneSwitcher.condition.twitch.type.polling.channel.title"},
	{MacroConditionTwitch::Condition::CATEGORY_POLLING,
	 "AdvSceneSwitcher.condition.twitch.type.polling.channel.category"},
	{MacroConditionTwitch::Condition::CHAT_MESSAGE_RECEIVED,
	 "AdvSceneSwitcher.condition.twitch.type.chat.message"},
	{MacroConditionTwitch::Condition::CHAT_USER_JOINED,
	 "AdvSceneSwitcher.condition.twitch.type.chat.userJoined"},
	{MacroConditionTwitch::Condition::CHAT_USER_LEFT,
	 "AdvSceneSwitcher.condition.twitch.type.chat.userLeft"},
};

const static std::map<MacroConditionTwitch::Condition, std::string> eventIdentifiers = {
	{MacroConditionTwitch::Condition::STREAM_ONLINE_EVENT, "stream.online"},
	{MacroConditionTwitch::Condition::STREAM_OFFLINE_EVENT,
	 "stream.offline"},
	{MacroConditionTwitch::Condition::CHANNEL_INFO_UPDATE_EVENT,
	 "channel.update"},
	{MacroConditionTwitch::Condition::FOLLOW_EVENT, "channel.follow"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_START_EVENT,
	 "channel.subscribe"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_END_EVENT,
	 "channel.subscription.end"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_GIFT_EVENT,
	 "channel.subscription.gift"},
	{MacroConditionTwitch::Condition::SUBSCRIPTION_MESSAGE_EVENT,
	 "channel.subscription.message"},
	{MacroConditionTwitch::Condition::CHEER_EVENT, "channel.cheer"},
	{MacroConditionTwitch::Condition::RAID_OUTBOUND_EVENT, "channel.raid"},
	{MacroConditionTwitch::Condition::RAID_INBOUND_EVENT, "channel.raid"},
	{MacroConditionTwitch::Condition::SHOUTOUT_OUTBOUND_EVENT,
	 "channel.shoutout.create"},
	{MacroConditionTwitch::Condition::SHOUTOUT_INBOUND_EVENT,
	 "channel.shoutout.receive"},
	{MacroConditionTwitch::Condition::POLL_START_EVENT,
	 "channel.poll.begin"},
	{MacroConditionTwitch::Condition::POLL_PROGRESS_EVENT,
	 "channel.poll.progress"},
	{MacroConditionTwitch::Condition::POLL_END_EVENT, "channel.poll.end"},
	{MacroConditionTwitch::Condition::PREDICTION_START_EVENT,
	 "channel.prediction.begin"},
	{MacroConditionTwitch::Condition::PREDICTION_PROGRESS_EVENT,
	 "channel.prediction.progress"},
	{MacroConditionTwitch::Condition::PREDICTION_LOCK_EVENT,
	 "channel.prediction.lock"},
	{MacroConditionTwitch::Condition::PREDICTION_END_EVENT,
	 "channel.prediction.end"},
	{MacroConditionTwitch::Condition::GOAL_START_EVENT,
	 "channel.goal.begin"},
	{MacroConditionTwitch::Condition::GOAL_PROGRESS_EVENT,
	 "channel.goal.progress"},
	{MacroConditionTwitch::Condition::GOAL_END_EVENT, "channel.goal.end"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_START_EVENT,
	 "channel.hype_train.begin"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_PROGRESS_EVENT,
	 "channel.hype_train.progress"},
	{MacroConditionTwitch::Condition::HYPE_TRAIN_END_EVENT,
	 "channel.hype_train.end"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_START_EVENT,
	 "channel.charity_campaign.start"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT,
	 "channel.charity_campaign.progress"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_DONATION_EVENT,
	 "channel.charity_campaign.donate"},
	{MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_END_EVENT,
	 "channel.charity_campaign.stop"},
	{MacroConditionTwitch::Condition::SHIELD_MODE_START_EVENT,
	 "channel.shield_mode.begin"},
	{MacroConditionTwitch::Condition::SHIELD_MODE_END_EVENT,
	 "channel.shield_mode.end"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_ADDITION_EVENT,
	 "channel.channel_points_custom_reward.add"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_UPDATE_EVENT,
	 "channel.channel_points_custom_reward.update"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_DELETION_EVENT,
	 "channel.channel_points_custom_reward.remove"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_REDEMPTION_EVENT,
	 "channel.channel_points_custom_reward_redemption.add"},
	{MacroConditionTwitch::Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT,
	 "channel.channel_points_custom_reward_redemption.update"},
	{MacroConditionTwitch::Condition::USER_BAN_EVENT, "channel.ban"},
	{MacroConditionTwitch::Condition::USER_UNBAN_EVENT, "channel.unban"},
	{MacroConditionTwitch::Condition::USER_MODERATOR_ADDITION_EVENT,
	 "channel.moderator.add"},
	{MacroConditionTwitch::Condition::USER_MODERATOR_DELETION_EVENT,
	 "channel.moderator.remove"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_LIVE_EVENT,
	 "stream.online"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_PLAYLIST_EVENT,
	 "stream.online"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_WATCHPARTY_EVENT,
	 "stream.online"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_PREMIERE_EVENT,
	 "stream.online"},
	{MacroConditionTwitch::Condition::STREAM_ONLINE_RERUN_EVENT,
	 "stream.online"},
};

const static std::map<MacroConditionTwitch::Condition, std::string>
	liveEventIDs = {
		{MacroConditionTwitch::Condition::STREAM_ONLINE_LIVE_EVENT,
		 "live"},
		{MacroConditionTwitch::Condition::STREAM_ONLINE_PLAYLIST_EVENT,
		 "playlist"},
		{MacroConditionTwitch::Condition::STREAM_ONLINE_WATCHPARTY_EVENT,
		 "watch_party"},
		{MacroConditionTwitch::Condition::STREAM_ONLINE_PREMIERE_EVENT,
		 "premiere"},
		{MacroConditionTwitch::Condition::STREAM_ONLINE_RERUN_EVENT,
		 "rerun"},
};

void MacroConditionTwitch::SetCondition(const Condition &condition)
{
	_condition = condition;
	SetupTempVars();
	ResetSubscription();
}

void MacroConditionTwitch::SetToken(const std::weak_ptr<TwitchToken> &t)
{
	_token = t;
}

void MacroConditionTwitch::SetChannel(const TwitchChannel &channel)
{
	_channel = channel;
	ResetSubscription();
}

void MacroConditionTwitch::SetPointsReward(
	const TwitchPointsReward &pointsReward)
{
	_pointsReward = pointsReward;
	ResetSubscription();
}

void MacroConditionTwitch::ResetChatConnection()
{
	_chatConnection.reset();
}

bool MacroConditionTwitch::CheckChannelGenericEvents()
{
	return HandleMatchingSubscriptionEvents([this](const Event &event) {
		SetVariableValue(event.ToString());
		SetJsonTempVars(event.data,
				[this](const char *id, const char *value) {
					SetTempVarValue(id, value);
				});
	});
}

bool MacroConditionTwitch::CheckChannelLiveEvents()
{
	if (!_eventBuffer) {
		return false;
	}

	while (!_eventBuffer->Empty()) {
		auto event = _eventBuffer->ConsumeMessage();
		if (!event) {
			continue;
		}
		if (_subscriptionID != event->id) {
			continue;
		}

		auto it = liveEventIDs.find(_condition);
		if (it == liveEventIDs.end()) {
			continue;
		}

		auto type = obs_data_get_string(event->data, "type");
		const auto &typeId = it->second;
		if (type != typeId) {
			continue;
		}

		SetVariableValue(event->ToString());
		SetJsonTempVars(event->data,
				[this](const char *id, const char *value) {
					SetTempVarValue(id, value);
				});

		if (_clearBufferOnMatch) {
			_eventBuffer->Clear();
		}
		return true;
	}

	return false;
}

bool MacroConditionTwitch::CheckChannelRewardChangeEvents()
{
	return HandleMatchingSubscriptionEvents([this](const Event &event) {
		SetVariableValue(event.ToString());
		SetJsonTempVars(event.data,
				[this](const char *id, const char *value) {
					SetTempVarValue(id, value);
				});

		OBSDataAutoRelease image =
			obs_data_get_obj(event.data, "image");
		SetTempVarValue("image.url_4x",
				obs_data_get_string(image, "url_4x"));

		OBSDataAutoRelease default_image =
			obs_data_get_obj(event.data, "default_image");
		SetTempVarValue("default_image.url_4x",
				obs_data_get_string(default_image, "url_4x"));

		OBSDataAutoRelease max_per_stream =
			obs_data_get_obj(event.data, "max_per_stream");
		SetTempVarValue("max_per_stream.is_enabled",
				obs_data_get_bool(max_per_stream,
						  "is_enabled"));
		SetTempVarValue("max_per_stream.max_per_stream",
				std::to_string(obs_data_get_int(max_per_stream,
								"value")));

		OBSDataAutoRelease max_per_user_per_stream =
			obs_data_get_obj(event.data, "max_per_user_per_stream");
		SetTempVarValue("max_per_user_per_stream.is_enabled",
				obs_data_get_bool(max_per_user_per_stream,
						  "is_enabled"));
		SetTempVarValue(
			"max_per_user_per_stream.max_per_user_per_stream",
			std::to_string(obs_data_get_int(max_per_user_per_stream,
							"value")));

		OBSDataAutoRelease global_cooldown =
			obs_data_get_obj(event.data, "global_cooldown");
		SetTempVarValue("global_cooldown.is_enabled",
				obs_data_get_bool(global_cooldown,
						  "is_enabled"));
		SetTempVarValue(
			"max_per_user_per_stream.global_cooldown_seconds",
			std::to_string(obs_data_get_int(max_per_user_per_stream,
							"seconds")));
	});
}

bool MacroConditionTwitch::CheckChannelRewardRedemptionEvents()
{
	return HandleMatchingSubscriptionEvents([this](const Event &event) {
		SetVariableValue(event.ToString());
		SetJsonTempVars(event.data,
				[this](const char *id, const char *value) {
					SetTempVarValue(id, value);
				});
		OBSDataAutoRelease rewardInfo =
			obs_data_get_obj(event.data, "reward");
		SetJsonTempVars(rewardInfo, [this](const char *nestedId,
						   const char *value) {
			const std::string id =
				"reward." + std::string(nestedId);
			SetTempVarValue(id, value);
		});
	});
}

bool MacroConditionTwitch::HandleMatchingSubscriptionEvents(
	const std::function<void(const Event &)> &matchCb)
{
	if (!_eventBuffer) {
		return false;
	}

	while (!_eventBuffer->Empty()) {
		auto event = _eventBuffer->ConsumeMessage();
		if (!event) {
			continue;
		}
		if (_subscriptionID != event->id) {
			continue;
		}
		matchCb(*event);

		if (_clearBufferOnMatch) {
			_eventBuffer->Clear();
		}
		return true;
	}

	return false;
}

static bool stringMatches(const RegexConfig &regex, const std::string &string,
			  const std::string &expr)
{
	if (!regex.Enabled()) {
		return string == expr;
	}

	return regex.Matches(string, expr);
}

bool MacroConditionTwitch::CheckChatMessages(TwitchToken &token)
{
	if (!_chatConnection) {
		_chatConnection = TwitchChatConnection::GetChatConnection(
			token, _channel);
		if (!_chatConnection) {
			return false;
		}
		_chatBuffer = _chatConnection->RegisterForMessages();
		return false;
	}

	while (!_chatBuffer->Empty()) {
		auto message = _chatBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}

		// Join and leave message don't have any message data
		if (message->properties.leftChannel ||
		    message->properties.joinedChannel) {
			continue;
		}

		if (!_chatMessagePattern.Matches(*message)) {
			continue;
		}

		SetTempVarValue("id", message->properties.id);
		SetTempVarValue("chat_message", message->message);
		SetTempVarValue("user_id", message->properties.userId);
		SetTempVarValue("user_login", message->source.nick);
		SetTempVarValue("user_name", message->properties.displayName);
		SetTempVarValue("user_type", message->properties.userType);
		SetTempVarValue("reply_parent_id",
				message->properties.replyParentId);
		SetTempVarValue("reply_parent_message",
				message->properties.replyParentBody);
		SetTempVarValue("reply_parent_user_id",
				message->properties.replyParentUserId);
		SetTempVarValue("reply_parent_user_login",
				message->properties.replyParentUserLogin);
		SetTempVarValue("reply_parent_user_name",
				message->properties.replyParentDisplayName);
		SetTempVarValue("root_parent_id",
				message->properties.rootParentId);
		SetTempVarValue("root_parent_user_login",
				message->properties.rootParentUserLogin);
		SetTempVarValue("badge_info",
				message->properties.badgeInfoString);
		SetTempVarValue("badges", message->properties.badgesString);
		SetTempVarValue("bits",
				std::to_string(message->properties.bits));
		SetTempVarValue("color", message->properties.color);
		SetTempVarValue("emotes", message->properties.emotesString);
		SetTempVarValue("timestamp",
				std::to_string(message->properties.timestamp));
		SetTempVarValue("is_emotes_only",
				message->properties.isUsingOnlyEmotes
					? "true"
					: "false");
		SetTempVarValue("is_first_message",
				message->properties.isFirstMessage ? "true"
								   : "false");
		SetTempVarValue("is_mod",
				message->properties.isMod ? "true" : "false");
		SetTempVarValue("is_subscriber",
				message->properties.isSubscriber ? "true"
								 : "false");
		SetTempVarValue("is_turbo",
				message->properties.isTurbo ? "true" : "false");
		SetTempVarValue("is_vip",
				message->properties.isVIP ? "true" : "false");

		if (_clearBufferOnMatch) {
			_chatBuffer->Clear();
		}
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChatUserJoinOrLeave(TwitchToken &token)
{
	if (!_chatConnection) {
		_chatConnection = TwitchChatConnection::GetChatConnection(
			token, _channel);
		if (!_chatConnection) {
			return false;
		}
		_chatBuffer = _chatConnection->RegisterForMessages();
		return false;
	}

	while (!_chatBuffer->Empty()) {
		auto message = _chatBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}

		if ((_condition == Condition::CHAT_USER_JOINED &&
		     !message->properties.joinedChannel) ||
		    (_condition == Condition::CHAT_USER_LEFT &&
		     !message->properties.leftChannel)) {
			continue;
		}

		SetTempVarValue("user_login", message->source.nick);

		if (_clearBufferOnMatch) {
			_chatBuffer->Clear();
		}
		return true;
	}
	return false;
}

void MacroConditionTwitch::SetTempVarValues(const ChannelLiveInfo &info)
{
	SetTempVarValue("broadcaster_user_id", info.user_id);
	SetTempVarValue("broadcaster_user_login", info.user_login);
	SetTempVarValue("broadcaster_user_name", info.user_name);
	SetTempVarValue("id", info.id);
	SetTempVarValue("game_id", info.game_id);
	SetTempVarValue("game_name", info.game_name);
	SetTempVarValue("title", info.title);
	std::string tags;
	for (const auto &tag : info.tags) {
		tags += tag + " ";
	}
	if (!tags.empty()) {
		tags.pop_back();
	}
	SetTempVarValue("tags", tags);
	SetTempVarValue("viewer_count", std::to_string(info.viewer_count));
	SetTempVarValue("started_at", info.started_at.toString().toStdString());
	SetTempVarValue("language", info.language);
	SetTempVarValue("thumbnail_url", info.thumbnail_url);
	SetTempVarValue("is_mature", info.is_mature ? "true" : "false");
}

void MacroConditionTwitch::SetTempVarValues(const ChannelInfo &info)
{
	SetTempVarValue("broadcaster_user_id", info.broadcaster_id);
	SetTempVarValue("broadcaster_user_login", info.broadcaster_login);
	SetTempVarValue("broadcaster_user_name", info.broadcaster_name);
	SetTempVarValue("language", info.broadcaster_language);
	SetTempVarValue("game_id", info.game_id);
	SetTempVarValue("game_name", info.game_name);
	SetTempVarValue("title", info.title);
	SetTempVarValue("delay", std::to_string(info.delay));
	std::string tags;
	for (const auto &tag : info.tags) {
		tags += tag + " ";
	}
	if (!tags.empty()) {
		tags.pop_back();
	}
	SetTempVarValue("tags", tags);
	std::string classificationLabels;
	for (const auto &label : info.content_classification_labels) {
		classificationLabels += label + " ";
	}
	if (!classificationLabels.empty()) {
		classificationLabels.pop_back();
	}
	SetTempVarValue("content_classification_labels", tags);
	SetTempVarValue("is_branded_content",
			info.is_branded_content ? "true" : "false");
}

bool MacroConditionTwitch::EventSubscriptionIsSetup(
	const std::shared_ptr<EventSub> &eventSub)
{
	if (!eventSub) {
		return false;
	}
	SetupEventSubscription(*eventSub);
	if (_subscriptionIDFuture.valid()) {
		// Still waiting for the subscription to be registered
		return false;
	}
	return true;
}

void MacroConditionTwitch::HandleMacroPause()
{
	const bool macroWasPausedSinceLastCheck =
		MacroWasPausedSince(GetMacro(), _lastCheck);
	_lastCheck = std::chrono::high_resolution_clock::now();

	if (macroWasPausedSinceLastCheck) {
		if (_eventBuffer) {
			_eventBuffer->Clear();
		}
		if (_chatBuffer) {
			_chatBuffer->Clear();
		}
	}
}

bool MacroConditionTwitch::CheckCondition()
{
	SetVariableValue("");
	auto token = _token.lock();
	if (!token) {
		return false;
	}

	auto eventSub = token->GetEventSub();
	if (IsUsingEventSubCondition() && !EventSubscriptionIsSetup(eventSub)) {
		return false;
	}

	HandleMacroPause();

	switch (_condition) {
	case Condition::STREAM_ONLINE_EVENT:
	case Condition::STREAM_OFFLINE_EVENT:
	case Condition::CHANNEL_INFO_UPDATE_EVENT:
	case Condition::FOLLOW_EVENT:
	case Condition::SUBSCRIPTION_START_EVENT:
	case Condition::SUBSCRIPTION_END_EVENT:
	case Condition::SUBSCRIPTION_GIFT_EVENT:
	case Condition::SUBSCRIPTION_MESSAGE_EVENT:
	case Condition::CHEER_EVENT:
	case Condition::RAID_OUTBOUND_EVENT:
	case Condition::RAID_INBOUND_EVENT:
	case Condition::SHOUTOUT_OUTBOUND_EVENT:
	case Condition::SHOUTOUT_INBOUND_EVENT:
	case Condition::POLL_START_EVENT:
	case Condition::POLL_PROGRESS_EVENT:
	case Condition::POLL_END_EVENT:
	case Condition::PREDICTION_START_EVENT:
	case Condition::PREDICTION_PROGRESS_EVENT:
	case Condition::PREDICTION_LOCK_EVENT:
	case Condition::PREDICTION_END_EVENT:
	case Condition::GOAL_START_EVENT:
	case Condition::GOAL_PROGRESS_EVENT:
	case Condition::GOAL_END_EVENT:
	case Condition::HYPE_TRAIN_START_EVENT:
	case Condition::HYPE_TRAIN_PROGRESS_EVENT:
	case Condition::HYPE_TRAIN_END_EVENT:
	case Condition::CHARITY_CAMPAIGN_START_EVENT:
	case Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT:
	case Condition::CHARITY_CAMPAIGN_DONATION_EVENT:
	case Condition::CHARITY_CAMPAIGN_END_EVENT:
	case Condition::SHIELD_MODE_START_EVENT:
	case Condition::SHIELD_MODE_END_EVENT:
	case Condition::USER_BAN_EVENT:
	case Condition::USER_UNBAN_EVENT:
	case Condition::USER_MODERATOR_ADDITION_EVENT:
	case Condition::USER_MODERATOR_DELETION_EVENT:
		return CheckChannelGenericEvents();
	case Condition::POINTS_REWARD_ADDITION_EVENT:
	case Condition::POINTS_REWARD_UPDATE_EVENT:
	case Condition::POINTS_REWARD_DELETION_EVENT:
		return CheckChannelRewardChangeEvents();
	case Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT:
		return CheckChannelRewardRedemptionEvents();
	case Condition::STREAM_ONLINE_LIVE_EVENT:
	case Condition::STREAM_ONLINE_PLAYLIST_EVENT:
	case Condition::STREAM_ONLINE_WATCHPARTY_EVENT:
	case Condition::STREAM_ONLINE_PREMIERE_EVENT:
	case Condition::STREAM_ONLINE_RERUN_EVENT:
		return CheckChannelLiveEvents();
	case Condition::LIVE_POLLING: {
		auto info = _channel.GetLiveInfo(*token);
		if (!info) {
			return false;
		}

		SetTempVarValues(*info);
		return info->IsLive();
	}
	case Condition::TITLE_POLLING: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->title);
		SetTempVarValues(*info);
		return stringMatches(_regexTitle, info->title, _streamTitle);
	}
	case Condition::CATEGORY_POLLING: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->game_name);
		SetTempVarValues(*info);
		return info->game_id == std::to_string(_category.id);
	}
	case Condition::CHAT_MESSAGE_RECEIVED: {
		auto token = _token.lock();
		if (!token) {
			return false;
		}
		return CheckChatMessages(*token);
	}
	case Condition::CHAT_USER_JOINED:
	case Condition::CHAT_USER_LEFT: {
		auto token = _token.lock();
		if (!token) {
			return false;
		}
		return CheckChatUserJoinOrLeave(*token);
	}
	default:
		break;
	}

	return false;
}

bool MacroConditionTwitch::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);

	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "token",
			    GetWeakTwitchTokenName(_token).c_str());
	_channel.Save(obj);
	_pointsReward.Save(obj);
	_streamTitle.Save(obj, "streamTitle");
	_regexTitle.Save(obj, "regexTitle");
	_chatMessagePattern.Save(obj);
	_category.Save(obj);
	obs_data_set_bool(obj, "clearBufferOnMatch", _clearBufferOnMatch);
	obs_data_set_int(obj, "version", 1);

	return true;
}

bool MacroConditionTwitch::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);

	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_channel.Load(obj);
	_pointsReward.Load(obj);
	_streamTitle.Load(obj, "streamTitle");
	_regexTitle.Load(obj, "regexTitle");
	_chatMessagePattern.Load(obj);
	_category.Load(obj);
	_clearBufferOnMatch = obs_data_get_bool(obj, "clearBufferOnMatch");
	if (!obs_data_has_user_value(obj, "version")) {
		_clearBufferOnMatch = false;
	}

	_subscriptionID = "";
	ResetChatConnection();

	return true;
}

bool MacroConditionTwitch::ConditionIsSupportedByToken()
{
	static const std::unordered_map<Condition, std::vector<TokenOption>>
		requiredOption = {
			{Condition::STREAM_ONLINE_EVENT, {}},
			{Condition::STREAM_OFFLINE_EVENT, {}},
			{Condition::CHANNEL_INFO_UPDATE_EVENT, {}},
			{Condition::FOLLOW_EVENT,
			 {{"moderator:read:followers"}}},
			{Condition::SUBSCRIPTION_START_EVENT,
			 {{"channel:read:subscriptions"}}},
			{Condition::SUBSCRIPTION_END_EVENT,
			 {{"channel:read:subscriptions"}}},
			{Condition::SUBSCRIPTION_GIFT_EVENT,
			 {{"channel:read:subscriptions"}}},
			{Condition::SUBSCRIPTION_MESSAGE_EVENT,
			 {{"channel:read:subscriptions"}}},
			{Condition::CHEER_EVENT, {{"bits:read"}}},
			{Condition::RAID_OUTBOUND_EVENT, {}},
			{Condition::RAID_INBOUND_EVENT, {}},
			{Condition::SHOUTOUT_OUTBOUND_EVENT,
			 {{"moderator:read:shoutouts"},
			  {"moderator:manage:shoutouts"}}},
			{Condition::SHOUTOUT_INBOUND_EVENT,
			 {{"moderator:read:shoutouts"},
			  {"moderator:manage:shoutouts"}}},
			{Condition::POLL_START_EVENT,
			 {{"channel:read:polls"}, {"channel:manage:polls"}}},
			{Condition::POLL_PROGRESS_EVENT,
			 {{"channel:read:polls"}, {"channel:manage:polls"}}},
			{Condition::POLL_END_EVENT,
			 {{"channel:read:polls"}, {"channel:manage:polls"}}},
			{Condition::PREDICTION_START_EVENT,
			 {{"channel:read:predictions"},
			  {"channel:manage:predictions"}}},
			{Condition::PREDICTION_PROGRESS_EVENT,
			 {{"channel:read:predictions"},
			  {"channel:manage:predictions"}}},
			{Condition::PREDICTION_LOCK_EVENT,
			 {{"channel:read:predictions"},
			  {"channel:manage:predictions"}}},
			{Condition::PREDICTION_END_EVENT,
			 {{"channel:read:predictions"},
			  {"channel:manage:predictions"}}},
			{Condition::GOAL_START_EVENT, {{"channel:read:goals"}}},
			{Condition::GOAL_PROGRESS_EVENT,
			 {{"channel:read:goals"}}},
			{Condition::GOAL_END_EVENT, {{"channel:read:goals"}}},
			{Condition::HYPE_TRAIN_START_EVENT,
			 {{"channel:read:hype_train"}}},
			{Condition::HYPE_TRAIN_PROGRESS_EVENT,
			 {{"channel:read:hype_train"}}},
			{Condition::HYPE_TRAIN_END_EVENT,
			 {{"channel:read:hype_train"}}},
			{Condition::CHARITY_CAMPAIGN_START_EVENT,
			 {{"channel:read:charity"}}},
			{Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT,
			 {{"channel:read:charity"}}},
			{Condition::CHARITY_CAMPAIGN_DONATION_EVENT,
			 {{"channel:read:charity"}}},
			{Condition::CHARITY_CAMPAIGN_END_EVENT,
			 {{"channel:read:charity"}}},
			{Condition::SHIELD_MODE_START_EVENT,
			 {{"moderator:read:shield_mode"},
			  {"moderator:manage:shield_mode"}}},
			{Condition::SHIELD_MODE_END_EVENT,
			 {{"moderator:read:shield_mode"},
			  {"moderator:manage:shield_mode"}}},
			{Condition::POINTS_REWARD_ADDITION_EVENT,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}},
			{Condition::POINTS_REWARD_UPDATE_EVENT,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}},
			{Condition::POINTS_REWARD_DELETION_EVENT,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}},
			{Condition::POINTS_REWARD_REDEMPTION_EVENT,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}},
			{Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}},
			{Condition::USER_BAN_EVENT, {{"channel:moderate"}}},
			{Condition::USER_UNBAN_EVENT, {{"channel:moderate"}}},
			{Condition::USER_MODERATOR_ADDITION_EVENT,
			 {{"moderation:read"}}},
			{Condition::USER_MODERATOR_DELETION_EVENT,
			 {{"moderation:read"}}},
			{Condition::STREAM_ONLINE_LIVE_EVENT, {}},
			{Condition::STREAM_ONLINE_PLAYLIST_EVENT, {}},
			{Condition::STREAM_ONLINE_WATCHPARTY_EVENT, {}},
			{Condition::STREAM_ONLINE_PREMIERE_EVENT, {}},
			{Condition::STREAM_ONLINE_RERUN_EVENT, {}},
			{Condition::LIVE_POLLING, {}},
			{Condition::TITLE_POLLING, {}},
			{Condition::CATEGORY_POLLING, {}},
			{Condition::CHAT_MESSAGE_RECEIVED,
			 {{"chat:read"}, {"chat:edit"}}},
			{Condition::CHAT_USER_JOINED,
			 {{"chat:read"}, {"chat:edit"}}},
			{Condition::CHAT_USER_LEFT,
			 {{"chat:read"}, {"chat:edit"}}},
		};

	auto token = _token.lock();
	if (!token) {
		return false;
	}

	auto it = requiredOption.find(_condition);
	assert(it != requiredOption.end());
	if (it == requiredOption.end()) {
		return false;
	}

	const auto &[_, options] = *it;

	return token->AnyOptionIsEnabled(options);
}

void MacroConditionTwitch::RegisterEventSubscription()
{
	if (!IsUsingEventSubCondition()) {
		return;
	}

	switch (_condition) {
	case Condition::STREAM_ONLINE_EVENT:
	case Condition::STREAM_OFFLINE_EVENT:
	case Condition::SUBSCRIPTION_START_EVENT:
	case Condition::SUBSCRIPTION_END_EVENT:
	case Condition::SUBSCRIPTION_GIFT_EVENT:
	case Condition::SUBSCRIPTION_MESSAGE_EVENT:
	case Condition::CHEER_EVENT:
	case Condition::POLL_START_EVENT:
	case Condition::POLL_PROGRESS_EVENT:
	case Condition::POLL_END_EVENT:
	case Condition::PREDICTION_START_EVENT:
	case Condition::PREDICTION_PROGRESS_EVENT:
	case Condition::PREDICTION_LOCK_EVENT:
	case Condition::PREDICTION_END_EVENT:
	case Condition::GOAL_START_EVENT:
	case Condition::GOAL_PROGRESS_EVENT:
	case Condition::GOAL_END_EVENT:
	case Condition::HYPE_TRAIN_START_EVENT:
	case Condition::HYPE_TRAIN_PROGRESS_EVENT:
	case Condition::HYPE_TRAIN_END_EVENT:
	case Condition::CHARITY_CAMPAIGN_START_EVENT:
	case Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT:
	case Condition::CHARITY_CAMPAIGN_DONATION_EVENT:
	case Condition::CHARITY_CAMPAIGN_END_EVENT:
	case Condition::POINTS_REWARD_ADDITION_EVENT:
	case Condition::USER_BAN_EVENT:
	case Condition::USER_UNBAN_EVENT:
	case Condition::USER_MODERATOR_ADDITION_EVENT:
	case Condition::USER_MODERATOR_DELETION_EVENT:
	case Condition::STREAM_ONLINE_LIVE_EVENT:
	case Condition::STREAM_ONLINE_PLAYLIST_EVENT:
	case Condition::STREAM_ONLINE_WATCHPARTY_EVENT:
	case Condition::STREAM_ONLINE_PREMIERE_EVENT:
	case Condition::STREAM_ONLINE_RERUN_EVENT:
		AddChannelGenericEventSubscription("1");
		break;
	case Condition::CHANNEL_INFO_UPDATE_EVENT:
		AddChannelGenericEventSubscription("2");
		break;
	case Condition::SHOUTOUT_OUTBOUND_EVENT:
	case Condition::SHOUTOUT_INBOUND_EVENT:
	case Condition::SHIELD_MODE_START_EVENT:
	case Condition::SHIELD_MODE_END_EVENT:
		AddChannelGenericEventSubscription("1", true);
		break;
	case Condition::FOLLOW_EVENT:
		AddChannelGenericEventSubscription("2", true);
		break;
	case Condition::RAID_OUTBOUND_EVENT:
		AddChannelGenericEventSubscription("1", false,
						   "from_broadcaster_user_id");
		break;
	case Condition::RAID_INBOUND_EVENT:
		AddChannelGenericEventSubscription("1", false,
						   "to_broadcaster_user_id");
		break;
	case Condition::POINTS_REWARD_UPDATE_EVENT:
	case Condition::POINTS_REWARD_DELETION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT: {
		OBSDataAutoRelease extraConditions = obs_data_create();

		if (_pointsReward.id != "-") {
			obs_data_set_string(extraConditions, "reward_id",
					    _pointsReward.id.c_str());
		}

		AddChannelGenericEventSubscription(
			"1", false, "broadcaster_user_id", extraConditions);
		break;
	}
	default:
		break;
	}
}

void MacroConditionTwitch::ResetSubscription()
{
	_eventBuffer.reset();
	_subscriptionID = "";
}

void MacroConditionTwitch::SetupEventSubscription(EventSub &eventSub)
{
	if (_subscriptionIDFuture.valid()) {
		if (_subscriptionIDFuture.wait_for(std::chrono::seconds(0)) !=
		    std::future_status::ready)
			return;

		_subscriptionID = _subscriptionIDFuture.get();
	}
	if (eventSub.SubscriptionIsActive(_subscriptionID)) {
		return;
	}
	RegisterEventSubscription();
	_eventBuffer = eventSub.RegisterForEvents();
}

bool MacroConditionTwitch::IsUsingEventSubCondition()
{
	return eventIdentifiers.find(_condition) != eventIdentifiers.end();
}

std::future<std::string>
waitForSubscription(const std::shared_ptr<TwitchToken> &token,
		    const Subscription &subscription)
{
	return std::async(std::launch::async, [token, subscription]() {
		auto id = EventSub::AddEventSubscribtion(token, subscription);
		return id;
	});
}

void MacroConditionTwitch::AddChannelGenericEventSubscription(
	const char *version, bool includeModeratorId,
	const char *mainUserIdFieldName, obs_data_t *extraConditions)
{
	if (!IsUsingEventSubCondition()) {
		return;
	}

	auto token = _token.lock();
	if (!token) {
		return;
	}

	OBSDataAutoRelease temp = obs_data_create();
	Subscription subscription{temp.Get()};
	obs_data_set_string(subscription.data, "type",
			    eventIdentifiers.find(_condition)->second.c_str());
	obs_data_set_string(subscription.data, "version", version);
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, mainUserIdFieldName,
			    _channel.GetUserID(*token).c_str());

	if (includeModeratorId) {
		obs_data_set_string(condition, "moderator_user_id",
				    token->GetUserID().c_str());
	}

	obs_data_apply(condition, extraConditions);
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

static std::string tryTranslate(const std::string &testString)
{
	auto translated = obs_module_text(testString.c_str());
	if (testString != translated) {
		return translated;
	}
	return "";
}

void MacroConditionTwitch::SetupTempVars()
{
	MacroCondition::SetupTempVars();

	auto setupTempVarHelper = [&](const std::string &id,
				      const std::string &extra = "") {
		std::string name = tryTranslate(
			"AdvSceneSwitcher.tempVar.twitch." + id + extra);
		std::string description =
			tryTranslate("AdvSceneSwitcher.tempVar.twitch." + id +
				     extra + ".description");
		AddTempvar(id, name.empty() ? id : name, description);
	};

	if (_condition != Condition::CHAT_MESSAGE_RECEIVED &&
	    _condition != Condition::CHAT_USER_JOINED &&
	    _condition != Condition::CHAT_USER_LEFT &&
	    _condition != Condition::RAID_INBOUND_EVENT &&
	    _condition != Condition::RAID_OUTBOUND_EVENT) {
		setupTempVarHelper("broadcaster_user_id");
		setupTempVarHelper("broadcaster_user_login");
		setupTempVarHelper("broadcaster_user_name");
	}

	switch (_condition) {
	case Condition::STREAM_ONLINE_LIVE_EVENT:
	case Condition::STREAM_ONLINE_PLAYLIST_EVENT:
	case Condition::STREAM_ONLINE_WATCHPARTY_EVENT:
	case Condition::STREAM_ONLINE_PREMIERE_EVENT:
	case Condition::STREAM_ONLINE_RERUN_EVENT:
	case Condition::STREAM_ONLINE_EVENT:
		setupTempVarHelper("id", ".stream.online");
		setupTempVarHelper("type", ".stream.online");
		setupTempVarHelper("started_at", ".stream.online");
		break;
	case Condition::STREAM_OFFLINE_EVENT:
		break;
	case Condition::SUBSCRIPTION_START_EVENT:
		setupTempVarHelper("user_id", ".subscribe");
		setupTempVarHelper("user_login", ".subscribe");
		setupTempVarHelper("user_name", ".subscribe");
		setupTempVarHelper("tier", ".subscribe");
		setupTempVarHelper("is_gift", ".subscribe");
		break;
	case Condition::SUBSCRIPTION_END_EVENT:
		setupTempVarHelper("user_id", ".subscribe.end");
		setupTempVarHelper("user_login", ".subscribe.end");
		setupTempVarHelper("user_name", ".subscribe.end");
		setupTempVarHelper("tier", ".subscribe");
		setupTempVarHelper("is_gift", ".subscribe.end");
		break;
	case Condition::SUBSCRIPTION_GIFT_EVENT:
		setupTempVarHelper("user_id", ".subscribe.gift");
		setupTempVarHelper("user_login", ".subscribe.gift");
		setupTempVarHelper("user_name", ".subscribe.gift");
		setupTempVarHelper("tier", ".subscribe");
		setupTempVarHelper("total", ".subscribe");
		setupTempVarHelper("cumulative_total", ".subscribe");
		setupTempVarHelper("is_anonymous", ".subscribe");
		break;
	case Condition::SUBSCRIPTION_MESSAGE_EVENT:
		setupTempVarHelper("user_id", ".subscribe.message");
		setupTempVarHelper("user_login", ".subscribe.message");
		setupTempVarHelper("user_name", ".subscribe.message");
		setupTempVarHelper("tier", ".subscribe");
		setupTempVarHelper("message", ".subscribe");
		setupTempVarHelper("cumulative_months", ".subscribe");
		setupTempVarHelper("streak_months", ".subscribe");
		setupTempVarHelper("duration_months", ".subscribe");
		break;
	case Condition::CHEER_EVENT:
		setupTempVarHelper("user_id", ".cheer");
		setupTempVarHelper("user_login", ".cheer");
		setupTempVarHelper("user_name", ".cheer");
		setupTempVarHelper("is_anonymous", ".cheer");
		setupTempVarHelper("message", ".cheer");
		setupTempVarHelper("bits");
		break;
	case Condition::POLL_START_EVENT:
	case Condition::POLL_PROGRESS_EVENT:
		setupTempVarHelper("id", ".poll");
		setupTempVarHelper("title", ".poll");
		setupTempVarHelper("choices", ".poll");
		setupTempVarHelper("channel_points_voting");
		setupTempVarHelper("started_at", ".poll");
		setupTempVarHelper("ends_at", ".poll");
		break;
	case Condition::POLL_END_EVENT:
		setupTempVarHelper("id", ".poll");
		setupTempVarHelper("title", ".poll");
		setupTempVarHelper("choices", ".pollEnd");
		setupTempVarHelper("channel_points_voting");
		setupTempVarHelper("started_at", ".poll");
		setupTempVarHelper("ends_at", ".poll");
		setupTempVarHelper("status", ".poll");
		break;
	case Condition::PREDICTION_START_EVENT:
	case Condition::PREDICTION_PROGRESS_EVENT:
	case Condition::PREDICTION_LOCK_EVENT:
		setupTempVarHelper("id", ".prediction");
		setupTempVarHelper("title", ".prediction");
		setupTempVarHelper("outcomes", ".prediction");
		setupTempVarHelper("started_at", ".prediction");
		setupTempVarHelper("locks_at", ".prediction");
		break;
	case Condition::PREDICTION_END_EVENT:
		setupTempVarHelper("id", ".prediction");
		setupTempVarHelper("title", ".prediction");
		setupTempVarHelper("outcomes", ".prediction");
		setupTempVarHelper("started_at", ".prediction");
		setupTempVarHelper("locks_at", ".prediction");
		setupTempVarHelper("ended_at", ".prediction");
		setupTempVarHelper("status", ".prediction");
		break;
	case Condition::GOAL_START_EVENT:
	case Condition::GOAL_PROGRESS_EVENT:
	case Condition::GOAL_END_EVENT:
		setupTempVarHelper("id", ".goal");
		setupTempVarHelper("type", ".goal");
		setupTempVarHelper("description", ".goal");
		setupTempVarHelper("is_achieved", ".goal");
		setupTempVarHelper("current_amount", ".goal");
		setupTempVarHelper("target_amount", ".goal");
		setupTempVarHelper("started_at", ".goal");
		setupTempVarHelper("ended_at", ".goal");
		break;
	case Condition::HYPE_TRAIN_START_EVENT:
	case Condition::HYPE_TRAIN_PROGRESS_EVENT:
		setupTempVarHelper("id", ".hype");
		setupTempVarHelper("total", ".hype");
		setupTempVarHelper("progress", ".hype");
		setupTempVarHelper("goal", ".hype");
		setupTempVarHelper("top_contributions", ".hype");
		setupTempVarHelper("last_contribution", ".hype");
		setupTempVarHelper("level", ".hype");
		setupTempVarHelper("started_at", ".hype");
		setupTempVarHelper("expires_at", ".hype");
		break;
	case Condition::HYPE_TRAIN_END_EVENT:
		setupTempVarHelper("id", ".hype");
		setupTempVarHelper("total", ".hype");
		setupTempVarHelper("progress", ".hype");
		setupTempVarHelper("goal", ".hype");
		setupTempVarHelper("top_contributions", ".hype");
		setupTempVarHelper("last_contribution", ".hype");
		setupTempVarHelper("level", ".hype");
		setupTempVarHelper("started_at", ".hype");
		setupTempVarHelper("ended_at", ".hype");
		setupTempVarHelper("cooldown_ends_at", ".hype");
		break;
	case Condition::CHARITY_CAMPAIGN_START_EVENT:
		setupTempVarHelper("id", ".charity");
		setupTempVarHelper("charity_name");
		setupTempVarHelper("charity_description");
		setupTempVarHelper("charity_logo");
		setupTempVarHelper("current_amount", ".charity");
		setupTempVarHelper("target_amount", ".charity");
		setupTempVarHelper("started_at", ".charity");
		break;
	case Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT:
		setupTempVarHelper("id", ".charity");
		setupTempVarHelper("charity_name");
		setupTempVarHelper("charity_description");
		setupTempVarHelper("charity_logo");
		setupTempVarHelper("current_amount", ".charity");
		setupTempVarHelper("target_amount", ".charity");
		break;
	case Condition::CHARITY_CAMPAIGN_DONATION_EVENT:
		setupTempVarHelper("charity_name");
		setupTempVarHelper("charity_description");
		setupTempVarHelper("charity_logo");
		setupTempVarHelper("id", ".charity");
		setupTempVarHelper("user_id", ".charity");
		setupTempVarHelper("user_login", ".charity");
		setupTempVarHelper("user_name", ".charity");
		setupTempVarHelper("amount", ".charity");
		break;
	case Condition::CHARITY_CAMPAIGN_END_EVENT:
		setupTempVarHelper("id", ".charity");
		setupTempVarHelper("charity_name");
		setupTempVarHelper("charity_description");
		setupTempVarHelper("charity_logo");
		setupTempVarHelper("current_amount", ".charity");
		setupTempVarHelper("target_amount", ".charity");
		setupTempVarHelper("stopped_at", ".charity");
		break;
	case Condition::POINTS_REWARD_ADDITION_EVENT:
	case Condition::POINTS_REWARD_UPDATE_EVENT:
	case Condition::POINTS_REWARD_DELETION_EVENT:
		setupTempVarHelper("id", ".reward");
		setupTempVarHelper("is_enabled", ".reward");
		setupTempVarHelper("is_paused", ".reward");
		setupTempVarHelper("is_in_stock", ".reward");
		setupTempVarHelper("title", ".reward");
		setupTempVarHelper("cost", ".reward");
		setupTempVarHelper("prompt", ".reward");
		setupTempVarHelper("is_user_input_required", ".reward");
		setupTempVarHelper("should_redemptions_skip_request_queue",
				   ".reward");
		setupTempVarHelper("background_color", ".reward");
		setupTempVarHelper("image", ".reward");
		setupTempVarHelper("image.url_4x", ".reward");
		setupTempVarHelper("default_image", ".reward");
		setupTempVarHelper("default_image.url_4x", ".reward");
		setupTempVarHelper("cooldown_expires_at", ".reward");
		setupTempVarHelper("redemptions_redeemed_current_stream",
				   ".reward");
		setupTempVarHelper("max_per_stream", ".reward");
		setupTempVarHelper("max_per_stream.is_enabled", ".reward");
		setupTempVarHelper("max_per_stream.max_per_stream", ".reward");
		setupTempVarHelper("max_per_user_per_stream", ".reward");
		setupTempVarHelper("max_per_user_per_stream.is_enabled",
				   ".reward");
		setupTempVarHelper(
			"max_per_user_per_stream.max_per_user_per_stream",
			".reward");
		setupTempVarHelper("global_cooldown", ".reward");
		setupTempVarHelper("global_cooldown.is_enabled", ".reward");
		setupTempVarHelper("global_cooldown.global_cooldown_seconds",
				   ".reward");
		break;
	case Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT:
		setupTempVarHelper("id", ".redemption");
		setupTempVarHelper("user_id", ".reward");
		setupTempVarHelper("user_login", ".reward");
		setupTempVarHelper("user_name", ".reward");
		setupTempVarHelper("user_input", ".reward");
		setupTempVarHelper("status", ".reward");
		setupTempVarHelper("reward", ".reward");
		setupTempVarHelper("reward.id", ".reward");
		setupTempVarHelper("reward.title", ".reward");
		setupTempVarHelper("reward.prompt", ".reward");
		setupTempVarHelper("reward.cost", ".reward");
		setupTempVarHelper("redeemed_at", ".reward");
		break;
	case Condition::USER_BAN_EVENT:
		setupTempVarHelper("user_id", ".ban");
		setupTempVarHelper("user_login", ".ban");
		setupTempVarHelper("user_name", ".ban");
		setupTempVarHelper("moderator_user_id", ".ban");
		setupTempVarHelper("moderator_user_login", ".ban");
		setupTempVarHelper("moderator_user_name", ".ban");
		setupTempVarHelper("reason", ".ban");
		setupTempVarHelper("banned_at", ".ban");
		setupTempVarHelper("ends_at", ".ban");
		setupTempVarHelper("is_permanent", ".ban");
		break;
	case Condition::USER_UNBAN_EVENT:
		setupTempVarHelper("user_id", ".unban");
		setupTempVarHelper("user_login", ".unban");
		setupTempVarHelper("user_name", ".unban");
		setupTempVarHelper("moderator_user_id", ".unban");
		setupTempVarHelper("moderator_user_login", ".unban");
		setupTempVarHelper("moderator_user_name", ".unban");
		break;
	case Condition::USER_MODERATOR_ADDITION_EVENT:
		setupTempVarHelper("user_id", ".addMod");
		setupTempVarHelper("user_login", ".addMod");
		setupTempVarHelper("user_name", ".addMod");
		break;
	case Condition::USER_MODERATOR_DELETION_EVENT:
		setupTempVarHelper("user_id", ".removeMod");
		setupTempVarHelper("user_login", ".removeMod");
		setupTempVarHelper("user_name", ".removeMod");
		break;
	case Condition::CHANNEL_INFO_UPDATE_EVENT:
		setupTempVarHelper("title");
		setupTempVarHelper("language");
		setupTempVarHelper("category_id");
		setupTempVarHelper("category_name");
		setupTempVarHelper("content_classification_labels");
		break;
	case Condition::SHOUTOUT_OUTBOUND_EVENT:
		setupTempVarHelper("to_broadcaster_user_id", ".shoutout");
		setupTempVarHelper("to_broadcaster_user_login", ".shoutout");
		setupTempVarHelper("to_broadcaster_user_name", ".shoutout");
		setupTempVarHelper("moderator_user_id", ".shoutout");
		setupTempVarHelper("moderator_user_login", ".shoutout");
		setupTempVarHelper("moderator_user_name", ".shoutout");
		setupTempVarHelper("viewer_count", ".shoutout");
		setupTempVarHelper("started_at", ".shoutout");
		setupTempVarHelper("cooldown_ends_at", ".shoutout");
		setupTempVarHelper("target_cooldown_ends_at", ".shoutout");
		break;
	case Condition::SHOUTOUT_INBOUND_EVENT:
		setupTempVarHelper("from_broadcaster_user_id",
				   ".shoutoutReceived");
		setupTempVarHelper("from_broadcaster_user_login",
				   ".shoutoutReceived");
		setupTempVarHelper("from_broadcaster_user_name",
				   ".shoutoutReceived");
		setupTempVarHelper("viewer_count", ".shoutoutReceived");
		setupTempVarHelper("started_at", ".shoutoutReceived");
		break;
	case Condition::SHIELD_MODE_START_EVENT:
	case Condition::SHIELD_MODE_END_EVENT:
		setupTempVarHelper("moderator_user_id", ".shieldMode");
		setupTempVarHelper("moderator_user_login", ".shieldMode");
		setupTempVarHelper("moderator_user_name", ".shieldMode");
		setupTempVarHelper("started_at", ".shieldMode");
		setupTempVarHelper("ended_at", ".shieldMode");
		break;
	case Condition::FOLLOW_EVENT:
		setupTempVarHelper("user_id", ".follow");
		setupTempVarHelper("user_login", ".follow");
		setupTempVarHelper("user_name", ".follow");
		setupTempVarHelper("followed_at");
		break;
	case Condition::RAID_INBOUND_EVENT:
	case Condition::RAID_OUTBOUND_EVENT:
		setupTempVarHelper("from_broadcaster_user_id", ".raid");
		setupTempVarHelper("from_broadcaster_user_login", ".raid");
		setupTempVarHelper("from_broadcaster_user_name", ".raid");
		setupTempVarHelper("to_broadcaster_user_id", ".raid");
		setupTempVarHelper("to_broadcaster_user_login", ".raid");
		setupTempVarHelper("to_broadcaster_user_name", ".raid");
		setupTempVarHelper("viewers", ".raid");
		break;
	case Condition::LIVE_POLLING:
		setupTempVarHelper("id", ".stream.online");
		setupTempVarHelper("game_id");
		setupTempVarHelper("game_name");
		setupTempVarHelper("title");
		setupTempVarHelper("tags");
		setupTempVarHelper("viewer_count", ".live");
		setupTempVarHelper("started_at", ".stream.online");
		setupTempVarHelper("language");
		setupTempVarHelper("thumbnail_url");
		setupTempVarHelper("is_mature");
		break;
	case Condition::TITLE_POLLING:
	case Condition::CATEGORY_POLLING:
		setupTempVarHelper("language");
		setupTempVarHelper("game_id");
		setupTempVarHelper("game_name");
		setupTempVarHelper("title");
		setupTempVarHelper("delay");
		setupTempVarHelper("tags");
		setupTempVarHelper("content_classification_labels");
		setupTempVarHelper("is_branded_content");
		break;
	case Condition::CHAT_MESSAGE_RECEIVED:
		setupTempVarHelper("id", ".chatReceive");
		setupTempVarHelper("chat_message", ".chatReceive");
		setupTempVarHelper("user_id", ".chatReceive");
		setupTempVarHelper("user_login", ".chatReceive");
		setupTempVarHelper("user_name", ".chatReceive");
		setupTempVarHelper("user_type", ".chatReceive");
		setupTempVarHelper("reply_parent_id", ".chatReceive");
		setupTempVarHelper("reply_parent_message", ".chatReceive");
		setupTempVarHelper("reply_parent_user_id", ".chatReceive");
		setupTempVarHelper("reply_parent_user_login", ".chatReceive");
		setupTempVarHelper("reply_parent_user_name", ".chatReceive");
		setupTempVarHelper("root_parent_id", ".chatReceive");
		setupTempVarHelper("root_parent_user_login", ".chatReceive");
		setupTempVarHelper("badge_info", ".chatReceive");
		setupTempVarHelper("badges", ".chatReceive");
		setupTempVarHelper("bits", ".chatReceive");
		setupTempVarHelper("color", ".chatReceive");
		setupTempVarHelper("emotes", ".chatReceive");
		setupTempVarHelper("timestamp", ".chatReceive");
		setupTempVarHelper("is_emotes_only", ".chatReceive");
		setupTempVarHelper("is_first_message", ".chatReceive");
		setupTempVarHelper("is_mod", ".chatReceive");
		setupTempVarHelper("is_subscriber", ".chatReceive");
		setupTempVarHelper("is_turbo", ".chatReceive");
		setupTempVarHelper("is_vip", ".chatReceive");
		break;
	case Condition::CHAT_USER_JOINED:
		setupTempVarHelper("user_login", ".chatJoin");
		break;
	case Condition::CHAT_USER_LEFT:
		setupTempVarHelper("user_login", ".chatLeave");
		break;
	default:
		break;
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[condition, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(condition));
	}
}

MacroConditionTwitchEdit::MacroConditionTwitchEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTwitch> entryData)
	: QWidget(parent),
	  _layout(new QHBoxLayout()),
	  _conditions(new FilterComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _tokenWarning(new QLabel()),
	  _channel(new TwitchChannelSelection(this)),
	  _pointsReward(new TwitchPointsRewardWidget(this)),
	  _streamTitle(new VariableLineEdit(this)),
	  _regexTitle(new RegexConfigWidget(parent)),
	  _chatMesageEdit(new ChatMessageEdit(this)),
	  _category(new TwitchCategoryWidget(this)),
	  _clearBufferOnMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.clearBufferOnMatch")))
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(&_tokenCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckToken()));
	QWidget::connect(_channel,
			 SIGNAL(ChannelChanged(const TwitchChannel &)), this,
			 SLOT(ChannelChanged(const TwitchChannel &)));
	QWidget::connect(
		_pointsReward,
		SIGNAL(PointsRewardChanged(const TwitchPointsReward &)), this,
		SLOT(PointsRewardChanged(const TwitchPointsReward &)));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_regexTitle,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexTitleChanged(const RegexConfig &)));
	QWidget::connect(
		_chatMesageEdit,
		SIGNAL(ChatMessagePatternChanged(const ChatMessagePattern &)),
		this,
		SLOT(ChatMessagePatternChanged(const ChatMessagePattern &)));
	QWidget::connect(_category,
			 SIGNAL(CategoryChanged(const TwitchCategory &)), this,
			 SLOT(CategoryChanged(const TwitchCategory &)));
	QWidget::connect(_clearBufferOnMatch, SIGNAL(stateChanged(int)), this,
			 SLOT(ClearBufferOnMatchChanged(int)));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.twitch.entry"),
		     _layout,
		     {{"{{conditions}}", _conditions},
		      {"{{channel}}", _channel},
		      {"{{pointsReward}}", _pointsReward},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{regex}}", _regexTitle},
		      {"{{category}}", _category}});
	_layout->setContentsMargins(0, 0, 0, 0);

	auto accountLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.twitch.entry.account"),
		     accountLayout, {{"{{account}}", _tokens}});
	accountLayout->setContentsMargins(0, 0, 0, 0);

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_layout);
	mainLayout->addWidget(_chatMesageEdit);
	mainLayout->addLayout(accountLayout);
	mainLayout->addWidget(_tokenWarning);
	mainLayout->addWidget(_clearBufferOnMatch);
	setLayout(mainLayout);

	_tokenCheckTimer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionTwitchEdit::ConditionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	if (idx == -1) { // Reset to previous selection
		const QSignalBlocker b(_conditions);
		_conditions->setCurrentIndex(_conditions->findData(
			static_cast<int>(_entryData->GetCondition())));
		return;
	}

	auto lock = LockContext();
	_entryData->SetCondition(static_cast<MacroConditionTwitch::Condition>(
		_conditions->itemData(idx).toInt()));
	SetWidgetVisibility();
}

void MacroConditionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetToken(GetWeakTwitchTokenByQString(token));
	_category->SetToken(_entryData->GetToken());
	_channel->SetToken(_entryData->GetToken());
	_pointsReward->SetToken(_entryData->GetToken());
	_entryData->ResetChatConnection();

	SetWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroConditionTwitchEdit::SetTokenWarning(bool visible,
					       const QString &text)
{
	_tokenWarning->setText(text);
	_tokenWarning->setVisible(visible);
	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::CheckToken()
{
	if (!_entryData) {
		return;
	}
	if (_entryData->GetToken().expired()) {
		SetTokenWarning(
			true,
			obs_module_text(
				"AdvSceneSwitcher.twitchToken.noSelection"));
		return;
	}
	if (!TokenIsValid(_entryData->GetToken())) {
		SetTokenWarning(
			true, obs_module_text(
				      "AdvSceneSwitcher.twitchToken.notValid"));
		return;
	}
	if (!_entryData->ConditionIsSupportedByToken()) {
		SetTokenWarning(
			true,
			obs_module_text(
				"AdvSceneSwitcher.twitchToken.permissionsInsufficient"));
		return;
	}
	SetTokenWarning(false);
}

void MacroConditionTwitchEdit::ChannelChanged(const TwitchChannel &channel)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetChannel(channel);
	_pointsReward->SetChannel(channel);
	_entryData->ResetChatConnection();
}

void MacroConditionTwitchEdit::PointsRewardChanged(
	const TwitchPointsReward &pointsReward)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetPointsReward(pointsReward);
}

void MacroConditionTwitchEdit::StreamTitleChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
}

void MacroConditionTwitchEdit::ChatMessagePatternChanged(
	const ChatMessagePattern &chatMessagePattern)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_chatMessagePattern = chatMessagePattern;
	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::RegexTitleChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regexTitle = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::CategoryChanged(const TwitchCategory &category)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_category = category;
}

void MacroConditionTwitchEdit::ClearBufferOnMatchChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_clearBufferOnMatch = value;
}

void MacroConditionTwitchEdit::SetWidgetVisibility()
{
	auto condition = _entryData->GetCondition();

	_pointsReward->setVisible(
		condition == MacroConditionTwitch::Condition::
				     POINTS_REWARD_UPDATE_EVENT ||
		condition == MacroConditionTwitch::Condition::
				     POINTS_REWARD_DELETION_EVENT ||
		condition == MacroConditionTwitch::Condition::
				     POINTS_REWARD_REDEMPTION_EVENT ||
		condition == MacroConditionTwitch::Condition::
				     POINTS_REWARD_REDEMPTION_UPDATE_EVENT);
	_streamTitle->setVisible(
		condition == MacroConditionTwitch::Condition::TITLE_POLLING);
	_regexTitle->setVisible(condition ==
				MacroConditionTwitch::Condition::TITLE_POLLING);
	_chatMesageEdit->setVisible(
		condition ==
		MacroConditionTwitch::Condition::CHAT_MESSAGE_RECEIVED);
	_category->setVisible(
		condition == MacroConditionTwitch::Condition::CATEGORY_POLLING);
	_clearBufferOnMatch->setVisible(
		_entryData->IsUsingEventSubCondition() ||
		_entryData->GetCondition() ==
			MacroConditionTwitch::Condition::CHAT_MESSAGE_RECEIVED ||
		_entryData->GetCondition() ==
			MacroConditionTwitch::Condition::CHAT_USER_JOINED ||
		_entryData->GetCondition() ==
			MacroConditionTwitch::Condition::CHAT_USER_LEFT);

	if (condition == MacroConditionTwitch::Condition::TITLE_POLLING) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}

	CheckToken();

	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->GetCondition())));
	_tokens->SetToken(_entryData->GetToken());
	_channel->SetToken(_entryData->GetToken());
	_channel->SetChannel(_entryData->_channel);
	_pointsReward->SetToken(_entryData->GetToken());
	_pointsReward->SetChannel(_entryData->_channel);
	_pointsReward->SetPointsReward(_entryData->_pointsReward);
	_streamTitle->setText(_entryData->_streamTitle);
	_regexTitle->SetRegexConfig(_entryData->_regexTitle);
	_chatMesageEdit->SetMessagePattern(_entryData->_chatMessagePattern);
	_category->SetToken(_entryData->GetToken());
	_category->SetCategory(_entryData->_category);
	_clearBufferOnMatch->setChecked(_entryData->_clearBufferOnMatch);

	SetWidgetVisibility();
}

} // namespace advss
