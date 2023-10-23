#include "macro-condition-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <utility.hpp>

namespace advss {

const std::string MacroConditionTwitch::id = "twitch";

bool MacroConditionTwitch::_registered = MacroConditionFactory::Register(
	MacroConditionTwitch::id,
	{MacroConditionTwitch::Create, MacroConditionTwitchEdit::Create,
	 "AdvSceneSwitcher.condition.twitch"});

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

bool MacroConditionTwitch::CheckChannelGenericEvents(
	TwitchToken &token, const char *mainUserIdFieldName)
{
	auto eventSub = token.GetEventSub();
	if (!eventSub) {
		return false;
	}
	auto events = eventSub->Events();
	for (const auto &event : events) {
		if (event.type != eventIdentifiers.find(_condition)->second) {
			continue;
		}
		auto id = obs_data_get_string(event.data, mainUserIdFieldName);
		if (id != _channel.GetUserID(token)) {
			continue;
		}
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChannelLiveEvents(TwitchToken &token)
{
	auto eventSub = token.GetEventSub();
	if (!eventSub) {
		return false;
	}
	auto events = eventSub->Events();
	for (const auto &event : events) {
		if (event.type != eventIdentifiers.find(_condition)->second) {
			continue;
		}
		auto id =
			obs_data_get_string(event.data, "broadcaster_user_id");
		if (id != _channel.GetUserID(token)) {
			continue;
		}
		auto type = obs_data_get_string(event.data, "type");
		auto it = liveEventIDs.find(_condition);
		if (it == liveEventIDs.end()) {
			continue;
		}
		const auto &typeId = it->second;
		if (type != typeId) {
			continue;
		}
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

static bool titleMatches(const RegexConfig &conf, const std::string &title,
			 const std::string &expr)
{
	if (!conf.Enabled()) {
		return title == expr;
	}

	auto regex = conf.GetRegularExpression(expr);
	if (!regex.isValid()) {
		return false;
	}
	auto match = regex.match(QString::fromStdString(title));
	return match.hasMatch();
}

bool MacroConditionTwitch::CheckCondition()
{
	SetVariableValue("");
	auto token = _token.lock();
	if (!token) {
		return false;
	}

	auto eventSub = token->GetEventSub();

	if (IsUsingEventSubCondition()) {
		if (!eventSub) {
			return false;
		}
		CheckEventSubscription(*eventSub);
		if (_subscriptionIDFuture.valid()) {
			// Still waiting for the subscription to be registered
			return false;
		}
	}

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
	case Condition::POINTS_REWARD_ADDITION_EVENT:
	case Condition::POINTS_REWARD_UPDATE_EVENT:
	case Condition::POINTS_REWARD_DELETION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT:
	case Condition::USER_BAN_EVENT:
	case Condition::USER_UNBAN_EVENT:
	case Condition::USER_MODERATOR_ADDITION_EVENT:
	case Condition::USER_MODERATOR_DELETION_EVENT:
		return CheckChannelGenericEvents(*token);
	case Condition::RAID_OUTBOUND_EVENT:
		return CheckChannelGenericEvents(*token,
						 "from_broadcaster_user_id");
	case Condition::RAID_INBOUND_EVENT:
		return CheckChannelGenericEvents(*token,
						 "to_broadcaster_user_id");
	case Condition::STREAM_ONLINE_LIVE_EVENT:
	case Condition::STREAM_ONLINE_PLAYLIST_EVENT:
	case Condition::STREAM_ONLINE_WATCHPARTY_EVENT:
	case Condition::STREAM_ONLINE_PREMIERE_EVENT:
	case Condition::STREAM_ONLINE_RERUN_EVENT:
		return CheckChannelLiveEvents(*token);
	case Condition::LIVE_POLLING: {
		auto info = _channel.GetLiveInfo(*token);
		if (!info) {
			return false;
		}
		return info->IsLive();
	}
	case Condition::TITLE_POLLING: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->title);
		return titleMatches(_regex, info->title, _streamTitle);
	}
	case Condition::CATEGORY_POLLING: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->game_name);
		return info->game_id == std::to_string(_category.id);
	}
	default:
		break;
	}

	return false;
}

void MacroConditionTwitch::CheckEventSubscription(EventSub &eventSub)
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
	SetupEventSubscriptions();
}

void MacroConditionTwitch::SetCondition(const Condition &cond)
{
	_condition = cond;
	_subscriptionID = "";
}

bool MacroConditionTwitch::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "token",
			    GetWeakTwitchTokenName(_token).c_str());
	_streamTitle.Save(obj, "streamTitle");
	_category.Save(obj);
	_channel.Save(obj);
	_regex.Save(obj);
	return true;
}

bool MacroConditionTwitch::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_category.Load(obj);
	_channel.Load(obj);
	_regex.Load(obj);
	_subscriptionID = "";
	return true;
}

std::string MacroConditionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
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
	for (const auto &tokenOption : options) {
		if (!token->OptionIsEnabled(tokenOption)) {
			return false;
		}
	}

	return true;
}

void MacroConditionTwitch::SetupEventSubscriptions()
{
	if (!IsUsingEventSubCondition()) {
		return;
	}

	switch (_condition) {
	case MacroConditionTwitch::Condition::STREAM_ONLINE_EVENT:
	case MacroConditionTwitch::Condition::STREAM_OFFLINE_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIPTION_START_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIPTION_END_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIPTION_GIFT_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIPTION_MESSAGE_EVENT:
	case MacroConditionTwitch::Condition::CHEER_EVENT:
	case MacroConditionTwitch::Condition::POLL_START_EVENT:
	case MacroConditionTwitch::Condition::POLL_PROGRESS_EVENT:
	case MacroConditionTwitch::Condition::POLL_END_EVENT:
	case MacroConditionTwitch::Condition::PREDICTION_START_EVENT:
	case MacroConditionTwitch::Condition::PREDICTION_PROGRESS_EVENT:
	case MacroConditionTwitch::Condition::PREDICTION_LOCK_EVENT:
	case MacroConditionTwitch::Condition::PREDICTION_END_EVENT:
	case MacroConditionTwitch::Condition::GOAL_START_EVENT:
	case MacroConditionTwitch::Condition::GOAL_PROGRESS_EVENT:
	case MacroConditionTwitch::Condition::GOAL_END_EVENT:
	case MacroConditionTwitch::Condition::HYPE_TRAIN_START_EVENT:
	case MacroConditionTwitch::Condition::HYPE_TRAIN_PROGRESS_EVENT:
	case MacroConditionTwitch::Condition::HYPE_TRAIN_END_EVENT:
	case MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_START_EVENT:
	case MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_PROGRESS_EVENT:
	case MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_DONATION_EVENT:
	case MacroConditionTwitch::Condition::CHARITY_CAMPAIGN_END_EVENT:
	case MacroConditionTwitch::Condition::POINTS_REWARD_ADDITION_EVENT:
	case MacroConditionTwitch::Condition::POINTS_REWARD_UPDATE_EVENT:
	case MacroConditionTwitch::Condition::POINTS_REWARD_DELETION_EVENT:
	case MacroConditionTwitch::Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case MacroConditionTwitch::Condition::USER_BAN_EVENT:
	case MacroConditionTwitch::Condition::USER_UNBAN_EVENT:
	case MacroConditionTwitch::Condition::USER_MODERATOR_ADDITION_EVENT:
	case MacroConditionTwitch::Condition::USER_MODERATOR_DELETION_EVENT:
	case MacroConditionTwitch::Condition::
		POINTS_REWARD_REDEMPTION_UPDATE_EVENT:
	case MacroConditionTwitch::Condition::STREAM_ONLINE_LIVE_EVENT:
	case MacroConditionTwitch::Condition::STREAM_ONLINE_PLAYLIST_EVENT:
	case MacroConditionTwitch::Condition::STREAM_ONLINE_WATCHPARTY_EVENT:
	case MacroConditionTwitch::Condition::STREAM_ONLINE_PREMIERE_EVENT:
	case MacroConditionTwitch::Condition::STREAM_ONLINE_RERUN_EVENT:
		AddChannelGenericEventSubscription("1");
		break;
	case MacroConditionTwitch::Condition::CHANNEL_INFO_UPDATE_EVENT:
		AddChannelGenericEventSubscription("2");
		break;
	case MacroConditionTwitch::Condition::SHOUTOUT_OUTBOUND_EVENT:
	case MacroConditionTwitch::Condition::SHOUTOUT_INBOUND_EVENT:
	case MacroConditionTwitch::Condition::SHIELD_MODE_START_EVENT:
	case MacroConditionTwitch::Condition::SHIELD_MODE_END_EVENT:
		AddChannelGenericEventSubscription("1", true);
		break;
	case MacroConditionTwitch::Condition::FOLLOW_EVENT:
		AddChannelGenericEventSubscription("2", true);
		break;
	case MacroConditionTwitch::Condition::RAID_OUTBOUND_EVENT:
		AddChannelGenericEventSubscription("1", false,
						   "from_broadcaster_user_id");
		break;
	case MacroConditionTwitch::Condition::RAID_INBOUND_EVENT:
		AddChannelGenericEventSubscription("1", false,
						   "to_broadcaster_user_id");
		break;
	default:
		break;
	}
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
	const char *mainUserIdFieldName)
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

	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
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
	  _conditions(new FilterComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _streamTitle(new VariableLineEdit(this)),
	  _category(new TwitchCategoryWidget(this)),
	  _channel(new TwitchChannelSelection(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _layout(new QHBoxLayout()),
	  _tokenPermissionWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.twitchToken.permissionsInsufficient")))
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_category,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QWidget::connect(_channel,
			 SIGNAL(ChannelChanged(const TwitchChannel &)), this,
			 SLOT(ChannelChanged(const TwitchChannel &)));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(&_tokenPermissionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckTokenPermissions()));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.twitch.entry"),
		     _layout,
		     {{"{{conditions}}", _conditions},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{category}}", _category},
		      {"{{regex}}", _regex},
		      {"{{channel}}", _channel}});
	_layout->setContentsMargins(0, 0, 0, 0);

	auto accountLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.twitch.entry.account"),
		     accountLayout, {{"{{account}}", _tokens}});
	accountLayout->setContentsMargins(0, 0, 0, 0);

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_layout);
	mainLayout->addLayout(accountLayout);
	mainLayout->addWidget(_tokenPermissionWarning);
	setLayout(mainLayout);

	_tokenPermissionCheckTimer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	SetupWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroConditionTwitchEdit::StreamTitleChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
}

void MacroConditionTwitchEdit::CategoreyChanged(const TwitchCategory &category)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_category = category;
}

void MacroConditionTwitchEdit::CheckTokenPermissions()
{
	_tokenPermissionWarning->setVisible(
		_entryData && !_entryData->ConditionIsSupportedByToken());
	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::SetupWidgetVisibility()
{
	auto condition = _entryData->GetCondition();
	_streamTitle->setVisible(
		condition == MacroConditionTwitch::Condition::TITLE_POLLING);
	_regex->setVisible(condition ==
			   MacroConditionTwitch::Condition::TITLE_POLLING);
	_category->setVisible(
		condition == MacroConditionTwitch::Condition::CATEGORY_POLLING);
	if (condition == MacroConditionTwitch::Condition::TITLE_POLLING) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}

	_tokenPermissionWarning->setVisible(
		!_entryData->ConditionIsSupportedByToken());

	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::ChannelChanged(const TwitchChannel &channel)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_channel = channel;
}

void MacroConditionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->GetCondition())));
	_tokens->SetToken(_entryData->_token);
	_streamTitle->setText(_entryData->_streamTitle);
	_category->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);
	_channel->SetChannel(_entryData->_channel);
	_regex->SetRegexConfig(_entryData->_regex);
	SetupWidgetVisibility();
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
	SetupWidgetVisibility();
}

void MacroConditionTwitchEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

} // namespace advss
