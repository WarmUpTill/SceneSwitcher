#include "macro-condition-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <utility.hpp>
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

static void
setTempVarsHelper(const std::string &jsonStr,
		  std::function<void(const char *, const char *)> setVar)
{
	try {
		auto json = nlohmann::json::parse(jsonStr);
		for (auto it = json.begin(); it != json.end(); ++it) {
			if (it->is_string()) {
				setVar(it.key().c_str(),
				       it->get<std::string>().c_str());
				continue;
			}
			setVar(it.key().c_str(), it->dump().c_str());
		}
	} catch (const nlohmann::json::exception &e) {
		vblog(LOG_INFO, "%s", jsonStr.c_str());
		vblog(LOG_INFO, "%s", e.what());
	}
}

static void
setTempVarsHelper(obs_data_t *data,
		  std::function<void(const char *, const char *)> setVar)
{
	auto jsonStr = obs_data_get_json(data);
	if (!jsonStr) {
		return;
	}

	setTempVarsHelper(jsonStr, setVar);
}

bool MacroConditionTwitch::CheckChannelGenericEvents(TwitchToken &token)
{
	auto eventSub = token.GetEventSub();
	if (!eventSub) {
		return false;
	}

	auto events = eventSub->Events();
	for (const auto &event : events) {
		if (_subscriptionID != event.id) {
			continue;
		}
		SetVariableValue(event.ToString());
		setTempVarsHelper(
			event.data,
			std::bind(&MacroConditionTwitch::SetTempVarValue, this,
				  std::placeholders::_1,
				  std::placeholders::_2));
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
		if (_subscriptionID != event.id) {
			continue;
		}

		auto it = liveEventIDs.find(_condition);
		if (it == liveEventIDs.end()) {
			continue;
		}

		auto type = obs_data_get_string(event.data, "type");
		const auto &typeId = it->second;
		if (type != typeId) {
			continue;
		}

		SetVariableValue(event.ToString());
		setTempVarsHelper(
			event.data,
			std::bind(&MacroConditionTwitch::SetTempVarValue, this,
				  std::placeholders::_1,
				  std::placeholders::_2));
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
		return titleMatches(_regex, info->title, _streamTitle);
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
	_regex.Save(obj);
	_category.Save(obj);

	return true;
}

bool MacroConditionTwitch::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);

	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_channel.Load(obj);
	_pointsReward.Load(obj);
	_regex.Load(obj);
	_category.Load(obj);
	_subscriptionID = "";

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

void MacroConditionTwitch::SetupEventSubscriptions()
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
	_subscriptionID = "";
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

	setupTempVarHelper("broadcaster_user_id");
	setupTempVarHelper("broadcaster_user_login");
	setupTempVarHelper("broadcaster_user_name");

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
		setupTempVarHelper("max_per_stream", ".reward");
		setupTempVarHelper("max_per_user_per_stream", ".reward");
		setupTempVarHelper("background_color", ".reward");
		setupTempVarHelper("image", ".reward");
		setupTempVarHelper("default_image", ".reward");
		setupTempVarHelper("global_cooldown", ".reward");
		setupTempVarHelper("cooldown_expires_at", ".reward");
		setupTempVarHelper("redemptions_redeemed_current_stream",
				   ".reward");
		break;
	case Condition::POINTS_REWARD_REDEMPTION_EVENT:
	case Condition::POINTS_REWARD_REDEMPTION_UPDATE_EVENT:
		setupTempVarHelper("id", ".reward");
		setupTempVarHelper("user_id", ".reward");
		setupTempVarHelper("user_login", ".reward");
		setupTempVarHelper("user_name", ".reward");
		setupTempVarHelper("user_input", ".reward");
		setupTempVarHelper("status", ".reward");
		setupTempVarHelper("reward", ".reward");
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
	case Condition::RAID_OUTBOUND_EVENT:
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
	  _tokenPermissionWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.twitchToken.permissionsInsufficient"))),
	  _channel(new TwitchChannelSelection(this)),
	  _pointsReward(new TwitchPointsRewardWidget(this)),
	  _streamTitle(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _category(new TwitchCategoryWidget(this))
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(&_tokenPermissionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckTokenPermissions()));
	QWidget::connect(_channel,
			 SIGNAL(ChannelChanged(const TwitchChannel &)), this,
			 SLOT(ChannelChanged(const TwitchChannel &)));
	QWidget::connect(
		_pointsReward,
		SIGNAL(PointsRewardChanged(const TwitchPointsReward &)), this,
		SLOT(PointsRewardChanged(const TwitchPointsReward &)));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(_category,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QWidget::connect(this, SIGNAL(TempVarsChanged()), window(),
			 SIGNAL(SegmentTempVarsChanged()));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.twitch.entry"),
		     _layout,
		     {{"{{conditions}}", _conditions},
		      {"{{channel}}", _channel},
		      {"{{pointsReward}}", _pointsReward},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{regex}}", _regex},
		      {"{{category}}", _category}});
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
	emit TempVarsChanged();
}

void MacroConditionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	_channel->SetToken(_entryData->_token);
	_pointsReward->SetToken(_entryData->_token);

	SetupWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroConditionTwitchEdit::CheckTokenPermissions()
{
	_tokenPermissionWarning->setVisible(
		_entryData && !_entryData->ConditionIsSupportedByToken());
	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::ChannelChanged(const TwitchChannel &channel)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetChannel(channel);
	_pointsReward->SetChannel(channel);
}

void MacroConditionTwitchEdit::PointsRewardChanged(
	const TwitchPointsReward &pointsReward)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetPointsReward(pointsReward);
}

void MacroConditionTwitchEdit::StreamTitleChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
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

void MacroConditionTwitchEdit::CategoreyChanged(const TwitchCategory &category)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_category = category;
}

void MacroConditionTwitchEdit::SetupWidgetVisibility()
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

void MacroConditionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(_conditions->findData(
		static_cast<int>(_entryData->GetCondition())));
	_tokens->SetToken(_entryData->_token);
	_channel->SetToken(_entryData->_token);
	_channel->SetChannel(_entryData->_channel);
	_pointsReward->SetToken(_entryData->_token);
	_pointsReward->SetChannel(_entryData->_channel);
	_pointsReward->SetPointsReward(_entryData->_pointsReward);
	_streamTitle->setText(_entryData->_streamTitle);
	_regex->SetRegexConfig(_entryData->_regex);
	_category->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);

	SetupWidgetVisibility();
}

} // namespace advss
