#include "macro-action-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <layout-helpers.hpp>
#include <ui-helpers.hpp>

namespace advss {

const std::string MacroActionTwitch::id = "twitch";

bool MacroActionTwitch::_registered = MacroActionFactory::Register(
	MacroActionTwitch::id,
	{MacroActionTwitch::Create, MacroActionTwitchEdit::Create,
	 "AdvSceneSwitcher.action.twitch"});

void MacroActionTwitch::ResolveVariablesToFixedValues()
{
	_streamTitle.ResolveVariables();
	_tags.ResolveVariables();
	_markerDescription.ResolveVariables();
	_duration.ResolveVariables();
	_announcementMessage.ResolveVariables();
	_channel.ResolveVariables();
	_chatMessage.ResolveVariables();
	_userLogin.ResolveVariables();
	_userId.ResolveVariables();
	_useVariableForRewardSelection = false;
	auto token = _token.lock();
	if (token) {
		ResolveVariableSelectionToRewardId(token);
		_pointsReward.title = _lastResolvedRewardTitle;
		_pointsReward.id = _lastResolvedRewardId;
	} else {
		_pointsReward.id = "";
		_pointsReward.id = "";
	}
}

std::shared_ptr<MacroAction> MacroActionTwitch::Create(Macro *m)
{
	return std::make_shared<MacroActionTwitch>(m);
}

std::shared_ptr<MacroAction> MacroActionTwitch::Copy() const
{
	return std::make_shared<MacroActionTwitch>(*this);
}

std::string MacroActionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

const static std::map<MacroActionTwitch::Action, std::string> actionTypes = {
	{MacroActionTwitch::Action::CHANNEL_INFO_TITLE_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.title.set"},
	{MacroActionTwitch::Action::CHANNEL_INFO_CATEGORY_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.category.set"},
	{MacroActionTwitch::Action::CHANNEL_INFO_TAGS_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.tags.set"},
	{MacroActionTwitch::Action::CHANNEL_INFO_LANGUAGE_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.language.set"},
	{MacroActionTwitch::Action::CHANNEL_INFO_CONTENT_LABELS_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.contentClassification.set"},
	{MacroActionTwitch::Action::RAID_START,
	 "AdvSceneSwitcher.action.twitch.type.raid.start"},
	{MacroActionTwitch::Action::COMMERCIAL_START,
	 "AdvSceneSwitcher.action.twitch.type.commercial.start"},
	{MacroActionTwitch::Action::MARKER_CREATE,
	 "AdvSceneSwitcher.action.twitch.type.marker.create"},
	{MacroActionTwitch::Action::CLIP_CREATE,
	 "AdvSceneSwitcher.action.twitch.type.clip.create"},
	{MacroActionTwitch::Action::CHAT_ANNOUNCEMENT_SEND,
	 "AdvSceneSwitcher.action.twitch.type.chat.announcement.send"},
	{MacroActionTwitch::Action::CHAT_EMOTE_ONLY_ENABLE,
	 "AdvSceneSwitcher.action.twitch.type.chat.emoteOnly.enable"},
	{MacroActionTwitch::Action::CHAT_EMOTE_ONLY_DISABLE,
	 "AdvSceneSwitcher.action.twitch.type.chat.emoteOnly.disable"},
	{MacroActionTwitch::Action::SEND_CHAT_MESSAGE,
	 "AdvSceneSwitcher.action.twitch.type.chat.sendMessage"},
	{MacroActionTwitch::Action::USER_GET_INFO,
	 "AdvSceneSwitcher.action.twitch.type.user.getInfo"},
	{MacroActionTwitch::Action::POINTS_REWARD_GET_INFO,
	 "AdvSceneSwitcher.action.twitch.type.reward.getInfo"},
};

const static std::map<MacroActionTwitch::AnnouncementColor, std::string>
	announcementColors = {
		{MacroActionTwitch::AnnouncementColor::PRIMARY,
		 "AdvSceneSwitcher.action.twitch.announcement.primary"},
		{MacroActionTwitch::AnnouncementColor::BLUE,
		 "AdvSceneSwitcher.action.twitch.announcement.blue"},
		{MacroActionTwitch::AnnouncementColor::GREEN,
		 "AdvSceneSwitcher.action.twitch.announcement.green"},
		{MacroActionTwitch::AnnouncementColor::ORANGE,
		 "AdvSceneSwitcher.action.twitch.announcement.orange"},
		{MacroActionTwitch::AnnouncementColor::PURPLE,
		 "AdvSceneSwitcher.action.twitch.announcement.purple"},
};

const static std::map<MacroActionTwitch::AnnouncementColor, std::string>
	announcementColorsTwitch = {
		{MacroActionTwitch::AnnouncementColor::PRIMARY, "primary"},
		{MacroActionTwitch::AnnouncementColor::BLUE, "blue"},
		{MacroActionTwitch::AnnouncementColor::GREEN, "green"},
		{MacroActionTwitch::AnnouncementColor::ORANGE, "orange"},
		{MacroActionTwitch::AnnouncementColor::PURPLE, "purple"},
};

const static std::map<MacroActionTwitch::RedemptionStatus, std::string>
	redemptionStatusesTwitch = {
		{MacroActionTwitch::RedemptionStatus::CANCELED, "CANCELED"},
		{MacroActionTwitch::RedemptionStatus::FULFILLED, "FULFILLED"},
};

void MacroActionTwitch::SetStreamTitle(
	const std::shared_ptr<TwitchToken> &token) const
{
	if (std::string(_streamTitle).empty()) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "title", _streamTitle.c_str());
	auto result = SendPatchRequest(*token, "https://api.twitch.tv",
				       "/helix/channels",
				       {{"broadcaster_id", token->GetUserID()}},
				       data.Get());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream title! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::SetStreamCategory(
	const std::shared_ptr<TwitchToken> &token) const
{
	if (_category.id == -1) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "game_id",
			    std::to_string(_category.id).c_str());
	auto result = SendPatchRequest(*token, "https://api.twitch.tv",
				       "/helix/channels",
				       {{"broadcaster_id", token->GetUserID()}},
				       data.Get());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream category! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::CreateStreamMarker(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "user_id", token->GetUserID().c_str());

	if (!std::string(_markerDescription).empty()) {
		obs_data_set_string(data, "description",
				    _markerDescription.c_str());
	}

	auto result = SendPostRequest(*token, "https://api.twitch.tv",
				      "/helix/streams/markers", {}, data.Get());

	if (result.status != 200) {
		blog(LOG_INFO, "Failed to create marker! (%d)", result.status);
	}
}

void MacroActionTwitch::CreateStreamClip(
	const std::shared_ptr<TwitchToken> &token) const
{
	auto hasDelay = _clipHasDelay ? "true" : "false";

	auto result = SendPostRequest(*token, "https://api.twitch.tv",
				      "/helix/clips",
				      {{"broadcaster_id", token->GetUserID()},
				       {"has_delay", hasDelay}});

	if (result.status != 202) {
		blog(LOG_INFO, "Failed to create clip! (%d)", result.status);
	}
}

void MacroActionTwitch::StartCommercial(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "broadcaster_id", token->GetUserID().c_str());
	obs_data_set_int(data, "length", _duration.Seconds());
	auto result = SendPostRequest(*token, "https://api.twitch.tv",
				      "/helix/channels/commercial", {},
				      data.Get());

	if (result.status == 200) {
		OBSDataArrayAutoRelease replyArray =
			obs_data_get_array(result.data, "data");
		OBSDataAutoRelease replyData =
			obs_data_array_item(replyArray, 0);
		vblog(LOG_INFO,
		      "Commercial started! (%d)\n"
		      "length: %lld\n"
		      "message: %s\n"
		      "retry_after: %lld\n",
		      result.status, obs_data_get_int(replyData, "length"),
		      obs_data_get_string(replyData, "message"),
		      obs_data_get_int(replyData, "retry_after"));
	} else {
		blog(LOG_INFO,
		     "Failed to start commercial! (%d)\n"
		     "error: %s\n"
		     "message: %s\n",
		     result.status, obs_data_get_string(result.data, "error"),
		     obs_data_get_string(result.data, "message"));
	}
}

void MacroActionTwitch::SendChatAnnouncement(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "message", _announcementMessage.c_str());
	obs_data_set_string(
		data, "color",
		announcementColorsTwitch.at(_announcementColor).c_str());
	auto userId = token->GetUserID();

	auto result = SendPostRequest(
		*token, "https://api.twitch.tv", "/helix/chat/announcements",
		{{"broadcaster_id", userId}, {"moderator_id", userId}},
		data.Get());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to send chat announcement! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::SetChatEmoteOnlyMode(
	const std::shared_ptr<TwitchToken> &token, bool enable) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "emote_mode", enable);
	auto userId = token->GetUserID();

	auto result = SendPatchRequest(
		*token, "https://api.twitch.tv", "/helix/chat/settings",
		{{"broadcaster_id", userId}, {"moderator_id", userId}},
		data.Get());

	if (result.status != 200) {
		blog(LOG_INFO, "Failed to %s chat's emote-only mode! (%d)",
		     enable ? "enable" : "disable", result.status);
	}
}

void MacroActionTwitch::StartRaid(const std::shared_ptr<TwitchToken> &token)
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "from_broadcaster_id",
			    token->GetUserID().c_str());
	obs_data_set_string(data, "to_broadcaster_id",
			    _channel.GetUserID(*token).c_str());
	auto result = SendPostRequest(*token, "https://api.twitch.tv",
				      "/helix/raids", {}, data.Get());

	if (result.status != 200) {
		blog(LOG_INFO, "Failed to start raid! (%d)\n", result.status);
	}
}

void MacroActionTwitch::SendChatMessage(
	const std::shared_ptr<TwitchToken> &token)
{
	if (!_chatConnection) {
		_chatConnection = TwitchChatConnection::GetChatConnection(
			*token, _channel);
		return;
	}

	_chatConnection->SendChatMessage(_chatMessage);
}

void MacroActionTwitch::GetUserInfo(const std::shared_ptr<TwitchToken> &token)
{
	httplib::Params params;
	switch (_userInfoQueryType) {
	case UserInfoQueryType::ID:
		params.insert({"id", std::to_string((uint64_t)_userId)});
		break;
	case UserInfoQueryType::LOGIN:
		params.insert({"login", _userLogin});
		break;
	default:
		break;
	}
	auto result = SendGetRequest(*token, "https://api.twitch.tv",
				     "/helix/users", params, true);

	if (result.status != 200) {
		blog(LOG_INFO, "Failed get user info! (%d)\n", result.status);
		return;
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(result.data, "data");
	size_t count = obs_data_array_count(array);
	if (count == 0) {
		blog(LOG_WARNING, "%s did not return any data!", __func__);
		return;
	}

	OBSDataAutoRelease data = obs_data_array_item(array, 0);
	SetJsonTempVars(data, [this](const char *id, const char *value) {
		SetTempVarValue(id, value);
	});
}

bool MacroActionTwitch::ResolveVariableSelectionToRewardId(
	const std::shared_ptr<TwitchToken> &token)
{
	if (!token) {
		return false;
	}

	auto variable = _rewardVariable.lock();
	if (!variable) {
		return false;
	}

	const auto rewardTitle = variable->Value();
	if (rewardTitle.empty()) {
		return false;
	}

	if (_lastResolvedRewardTitle == rewardTitle) {
		return true;
	}

	const auto rewards = GetPointsRewardsForChannel(token, _channel);
	if (!rewards) {
		return false;
	}

	for (const auto &reward : *rewards) {
		if (reward.title == rewardTitle) {
			_lastResolvedRewardId = reward.id;
			_lastResolvedRewardTitle = reward.title;
			return true;
		}
	}

	return false;
}

void MacroActionTwitch::GetRewardInfo(const std::shared_ptr<TwitchToken> &token)
{
	if (_useVariableForRewardSelection &&
	    !ResolveVariableSelectionToRewardId(token)) {
		if (VerboseLoggingEnabled()) {
			auto variable = _rewardVariable.lock();
			if (!variable) {
				blog(LOG_WARNING,
				     "failed to resolve variable reward name to reward id (invalid selection)");
				return;
			}
			blog(LOG_WARNING,
			     "failed to resolve variable reward name '%s' to reward id",
			     variable->Value().c_str());
			return;
		}
		return;
	}

	httplib::Params params = {
		{"broadcaster_id", token->GetUserID()},
		{"id", _useVariableForRewardSelection ? _lastResolvedRewardId
						      : _pointsReward.id},
	};
	auto result = SendGetRequest(*token, "https://api.twitch.tv",
				     "/helix/channel_points/custom_rewards",
				     params, true);

	if (result.status != 200) {
		blog(LOG_INFO, "Failed get reward info! (%d)\n", result.status);
		return;
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(result.data, "data");
	size_t count = obs_data_array_count(array);
	if (count == 0) {
		blog(LOG_WARNING, "%s did not return any data!", __func__);
		return;
	}

	OBSDataAutoRelease data = obs_data_array_item(array, 0);

	SetJsonTempVars(data, [this](const char *id, const char *value) {
		SetTempVarValue(id, value);
	});

	OBSDataAutoRelease image = obs_data_get_obj(data, "image");
	SetTempVarValue("image.url_4x", obs_data_get_string(image, "url_4x"));

	OBSDataAutoRelease default_image =
		obs_data_get_obj(data, "default_image");
	SetTempVarValue("default_image.url_4x",
			obs_data_get_string(default_image, "url_4x"));

	OBSDataAutoRelease max_per_stream_setting =
		obs_data_get_obj(data, "max_per_stream_setting");
	SetTempVarValue("max_per_stream.is_enabled",
			obs_data_get_bool(max_per_stream_setting,
					  "is_enabled"));
	SetTempVarValue("max_per_stream.max_per_stream",
			std::to_string(obs_data_get_int(max_per_stream_setting,
							"max_per_stream")));

	OBSDataAutoRelease max_per_user_per_stream_setting =
		obs_data_get_obj(data, "max_per_user_per_stream_setting");
	SetTempVarValue("max_per_user_per_stream.is_enabled",
			obs_data_get_bool(max_per_user_per_stream_setting,
					  "is_enabled"));
	SetTempVarValue(
		"max_per_user_per_stream.max_per_user_per_stream",
		std::to_string(obs_data_get_int(max_per_user_per_stream_setting,
						"max_per_user_per_stream")));

	OBSDataAutoRelease global_cooldown_setting =
		obs_data_get_obj(data, "global_cooldown_setting");
	SetTempVarValue("global_cooldown.is_enabled",
			obs_data_get_bool(global_cooldown_setting,
					  "is_enabled"));
	SetTempVarValue(
		"max_per_user_per_stream.global_cooldown_seconds",
		std::to_string(obs_data_get_int(max_per_user_per_stream_setting,
						"global_cooldown_seconds")));
}

static std::string tryTranslate(const std::string &testString)
{
	auto translated = obs_module_text(testString.c_str());
	if (testString != translated) {
		return translated;
	}
	return "";
}

void MacroActionTwitch::SetupTempVars()
{
	MacroAction::SetupTempVars();

	auto setupTempVarHelper = [&](const std::string &id,
				      const std::string &extra = "") {
		std::string name = tryTranslate(
			"AdvSceneSwitcher.tempVar.twitch." + id + extra);
		std::string description =
			tryTranslate("AdvSceneSwitcher.tempVar.twitch." + id +
				     extra + ".description");
		AddTempvar(id, name.empty() ? id : name, description);
	};

	switch (_action) {
	case Action::POINTS_REWARD_GET_INFO:
		setupTempVarHelper("title", ".reward");
		setupTempVarHelper("prompt", ".reward");
		setupTempVarHelper("cost", ".reward");
		setupTempVarHelper("background_color", ".reward");
		setupTempVarHelper("is_enabled", ".reward");
		setupTempVarHelper("is_user_input_required", ".reward");
		setupTempVarHelper("is_paused", ".reward");
		setupTempVarHelper("is_in_stock", ".reward");
		setupTempVarHelper("should_redemptions_skip_request_queue",
				   ".reward");
		setupTempVarHelper("redemptions_redeemed_current_stream",
				   ".reward");
		setupTempVarHelper("cooldown_expires_at", ".reward");
		setupTempVarHelper("max_per_stream.is_enabled", ".reward");
		setupTempVarHelper("max_per_stream.max_per_stream", ".reward");
		setupTempVarHelper("max_per_user_per_stream.is_enabled",
				   ".reward");
		setupTempVarHelper(
			"max_per_user_per_stream.max_per_user_per_stream",
			".reward");
		setupTempVarHelper("global_cooldown.is_enabled", ".reward");
		setupTempVarHelper("global_cooldown.global_cooldown_seconds",
				   ".reward");
		setupTempVarHelper("image.url_4x", ".reward");
		setupTempVarHelper("default_image.url_4x", ".reward");
		break;
	case Action::USER_GET_INFO:
		setupTempVarHelper("id", ".user.getInfo");
		setupTempVarHelper("login", ".user.getInfo");
		setupTempVarHelper("display_name", ".user.getInfo");
		setupTempVarHelper("type", ".user.getInfo");
		setupTempVarHelper("broadcaster_type", ".user.getInfo");
		setupTempVarHelper("description", ".user.getInfo");
		setupTempVarHelper("profile_image_url", ".user.getInfo");
		setupTempVarHelper("offline_image_url", ".user.getInfo");
		setupTempVarHelper("created_at", ".user.getInfo");
		break;
	default:
		break;
	}
}

bool MacroActionTwitch::PerformAction()
{
	auto token = _token.lock();
	if (!token) {
		return true;
	}

	switch (_action) {
	case Action::CHANNEL_INFO_TITLE_SET:
		SetStreamTitle(token);
		break;
	case Action::CHANNEL_INFO_CATEGORY_SET:
		SetStreamCategory(token);
		break;
	case Action::CHANNEL_INFO_TAGS_SET:
		_tags.SetStreamTags(*token);
		break;
	case Action::CHANNEL_INFO_LANGUAGE_SET:
		_language.SetStreamLanguage(*token);
		break;
	case Action::CHANNEL_INFO_CONTENT_LABELS_SET:
		_contentClassification.SetContentClassification(*token);
		break;
	case Action::RAID_START:
		StartRaid(token);
		break;
	case Action::COMMERCIAL_START:
		StartCommercial(token);
		break;
	case Action::MARKER_CREATE:
		CreateStreamMarker(token);
		break;
	case Action::CLIP_CREATE:
		CreateStreamClip(token);
		break;
	case Action::CHAT_ANNOUNCEMENT_SEND:
		SendChatAnnouncement(token);
		break;
	case Action::CHAT_EMOTE_ONLY_ENABLE:
		SetChatEmoteOnlyMode(token, true);
		break;
	case Action::CHAT_EMOTE_ONLY_DISABLE:
		SetChatEmoteOnlyMode(token, false);
		break;
	case MacroActionTwitch::Action::SEND_CHAT_MESSAGE:
		SendChatMessage(token);
		break;
	case MacroActionTwitch::Action::USER_GET_INFO:
		GetUserInfo(token);
		break;
	case MacroActionTwitch::Action::POINTS_REWARD_GET_INFO:
		GetRewardInfo(token);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionTwitch::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\" with token for \"%s\"",
		      it->second.c_str(),
		      GetWeakTwitchTokenName(_token).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown twitch action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionTwitch::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);

	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_string(obj, "token",
			    GetWeakTwitchTokenName(_token).c_str());
	_streamTitle.Save(obj, "streamTitle");
	_category.Save(obj);
	_tags.Save(obj);
	_language.Save(obj);
	_contentClassification.Save(obj);
	_markerDescription.Save(obj, "markerDescription");
	obs_data_set_bool(obj, "clipHasDelay", _clipHasDelay);
	_duration.Save(obj);
	_announcementMessage.Save(obj, "announcementMessage");
	obs_data_set_int(obj, "announcementColor",
			 static_cast<int>(_announcementColor));
	_channel.Save(obj);
	_chatMessage.Save(obj, "chatMessage");
	obs_data_set_int(obj, "userInfoQueryType",
			 static_cast<int>(_userInfoQueryType));
	_userLogin.Save(obj, "userLogin");
	_userId.Save(obj, "userId");
	_pointsReward.Save(obj);
	obs_data_set_string(obj, "rewardVariable",
			    GetWeakVariableName(_rewardVariable).c_str());
	obs_data_set_bool(obj, "useVariableForRewardSelection",
			  _useVariableForRewardSelection);

	return true;
}

bool MacroActionTwitch::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);

	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_category.Load(obj);
	_tags.Load(obj);
	_language.Load(obj);
	_contentClassification.Load(obj);
	_markerDescription.Load(obj, "markerDescription");
	_clipHasDelay = obs_data_get_bool(obj, "clipHasDelay");
	_duration.Load(obj);
	_announcementMessage.Load(obj, "announcementMessage");
	_announcementColor = static_cast<AnnouncementColor>(
		obs_data_get_int(obj, "announcementColor"));
	_channel.Load(obj);
	_chatMessage.Load(obj, "chatMessage");
	_userInfoQueryType = static_cast<UserInfoQueryType>(
		obs_data_get_int(obj, "userInfoQueryType"));
	_userLogin.Load(obj, "userLogin");
	_userId.Load(obj, "userId");
	_pointsReward.Load(obj);
	_rewardVariable = GetWeakVariableByName(
		obs_data_get_string(obj, "rewardVariable"));
	_useVariableForRewardSelection =
		obs_data_get_bool(obj, "useVariableForRewardSelection");

	SetAction(static_cast<Action>(obs_data_get_int(obj, "action")));

	return true;
}

void MacroActionTwitch::SetAction(Action action)
{
	_action = action;
	ResetChatConnection();
	SetupTempVars();
}

bool MacroActionTwitch::ActionIsSupportedByToken()
{
	static const std::unordered_map<Action, std::vector<TokenOption>>
		requiredOption = {
			{Action::CHANNEL_INFO_TITLE_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_CATEGORY_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_TAGS_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_CONTENT_LABELS_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_LANGUAGE_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_DELAY_SET,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_BRANDED_CONTENT_ENABLE,
			 {{"channel:manage:broadcast"}}},
			{Action::CHANNEL_INFO_BRANDED_CONTENT_DISABLE,
			 {{"channel:manage:broadcast"}}},
			{Action::RAID_START, {{"channel:manage:raids"}}},
			{Action::RAID_END, {{"channel:manage:raids"}}},
			{Action::SHOUTOUT_SEND,
			 {{"moderator:manage:shoutouts"}}},
			{Action::POLL_END, {{"channel:manage:polls"}}},
			{Action::PREDICTION_END,
			 {{"channel:manage:predictions"}}},
			{Action::SHIELD_MODE_START,
			 {{"moderator:manage:shield_mode"}}},
			{Action::SHIELD_MODE_END,
			 {{"moderator:manage:shield_mode"}}},
			{Action::POINTS_REWARD_ENABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_DISABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_PAUSE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_UNPAUSE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_TITLE_SET,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_PROMPT_SET,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_COST_SET,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_USER_INPUT_REQUIRE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_USER_INPUT_UNREQUIRE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_COOLDOWN_ENABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_COOLDOWN_DISABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_QUEUE_SKIP_ENABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_QUEUE_SKIP_DISABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_MAX_PER_STREAM_ENABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_MAX_PER_STREAM_DISABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_MAX_PER_USER_ENABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_MAX_PER_USER_DISABLE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_DELETE,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_REDEMPTION_CANCEL,
			 {{"channel:manage:redemptions"}}},
			{Action::POINTS_REWARD_REDEMPTION_FULFILL,
			 {{"channel:manage:redemptions"}}},
			{Action::USER_BAN, {{"moderator:manage:banned_users"}}},
			{Action::USER_UNBAN,
			 {{"moderator:manage:banned_users"}}},
			{Action::USER_BLOCK, {{"user:manage:blocked_users"}}},
			{Action::USER_UNBLOCK, {{"user:manage:blocked_users"}}},
			{Action::USER_MODERATOR_ADD,
			 {{"channel:manage:moderators"}}},
			{Action::USER_MODERATOR_DELETE,
			 {{"channel:manage:moderators"}}},
			{Action::USER_VIP_ADD, {{"channel:manage:vips"}}},
			{Action::USER_VIP_DELETE, {{"channel:manage:vips"}}},
			{Action::COMMERCIAL_START,
			 {{"channel:edit:commercial"}}},
			{Action::COMMERCIAL_SNOOZE, {{"channel:manage:ads"}}},
			{Action::MARKER_CREATE, {{"channel:manage:broadcast"}}},
			{Action::CLIP_CREATE, {{"clips:edit"}}},
			{Action::CHAT_ANNOUNCEMENT_SEND,
			 {{"moderator:manage:announcements"}}},
			{Action::CHAT_EMOTE_ONLY_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_EMOTE_ONLY_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_FOLLOWER_ONLY_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_FOLLOWER_ONLY_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_SUBSCRIBER_ONLY_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_SUBSCRIBER_ONLY_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_SLOW_MODE_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_SLOW_MODE_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_NON_MODERATOR_DELAY_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_NON_MODERATOR_DELAY_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_UNIQUE_MODE_ENABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::CHAT_UNIQUE_MODE_DISABLE,
			 {{"moderator:manage:chat_settings"}}},
			{Action::WHISPER_SEND, {{"user:manage:whispers"}}},
			{Action::SEND_CHAT_MESSAGE, {{"chat:edit"}}},
			{Action::USER_GET_INFO, {}},
			{Action::POINTS_REWARD_GET_INFO,
			 {{"channel:read:redemptions"},
			  {"channel:manage:redemptions"}}}};

	auto token = _token.lock();
	if (!token) {
		return false;
	}

	auto it = requiredOption.find(_action);
	assert(it != requiredOption.end());
	if (it == requiredOption.end()) {
		return false;
	}

	const auto &[_, options] = *it;

	return token->AnyOptionIsEnabled(options);
}

void MacroActionTwitch::ResetChatConnection()
{
	_chatConnection.reset();
	if (_action == Action::SEND_CHAT_MESSAGE) {
		auto token = _token.lock();
		if (!token) {
			return;
		}
		_chatConnection = TwitchChatConnection::GetChatConnection(
			*token, _channel);
	}
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
	}
}

static inline void populateAnnouncementColorSelection(QComboBox *list)
{
	for (const auto &[_, name] : announcementColors) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static void populateUserQueryInfoTypeSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.twitch.user.getInfo.queryType.id"),
		static_cast<int>(MacroActionTwitch::UserInfoQueryType::ID));
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.action.twitch.user.getInfo.queryType.login"),
		static_cast<int>(MacroActionTwitch::UserInfoQueryType::LOGIN));
}

MacroActionTwitchEdit::MacroActionTwitchEdit(
	QWidget *parent, std::shared_ptr<MacroActionTwitch> entryData)
	: QWidget(parent),
	  _layout(new QHBoxLayout()),
	  _actions(new FilterComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _tokenWarning(new QLabel()),
	  _streamTitle(new VariableLineEdit(this)),
	  _category(new TwitchCategoryWidget(this)),
	  _tags(new TagListWidget(this)),
	  _language(new LanguageSelectionWidget(this)),
	  _contentClassification(new ContentClassificationEdit(this)),
	  _markerDescription(new VariableLineEdit(this)),
	  _clipHasDelay(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.twitch.clip.hasDelay"))),
	  _duration(new DurationSelection(this, false, 0)),
	  _announcementMessage(new VariableTextEdit(this)),
	  _announcementColor(new QComboBox(this)),
	  _channel(new TwitchChannelSelection(this)),
	  _chatMessage(new VariableTextEdit(this)),
	  _userInfoQueryType(new QComboBox(this)),
	  _userLogin(new VariableLineEdit(this)),
	  _userId(new VariableDoubleSpinBox(this)),
	  _pointsReward(new TwitchPointsRewardWidget(this, false)),
	  _rewardVariable(new VariableSelection(this)),
	  _toggleRewardSelection(new QPushButton())
{
	SetWidgetProperties();
	SetWidgetSignalConnections();

	_entryData = entryData;
	SetWidgetLayout();

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_layout);
	mainLayout->addWidget(_announcementMessage);
	mainLayout->addWidget(_chatMessage);
	mainLayout->addWidget(_tags);
	mainLayout->addWidget(_language);
	mainLayout->addWidget(_contentClassification);
	mainLayout->addWidget(_tokenWarning);
	setLayout(mainLayout);

	_tokenCheckTimer.start(1000);

	UpdateEntryData();
	_loading = false;
}

void MacroActionTwitchEdit::ActionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	if (idx == -1) { // Reset to previous selection
		const QSignalBlocker b(_actions);
		_actions->setCurrentIndex(_actions->findData(
			static_cast<int>(_entryData->GetAction())));
		return;
	}

	auto lock = LockContext();
	_entryData->SetAction(static_cast<MacroActionTwitch::Action>(
		_actions->itemData(idx).toInt()));
	SetWidgetLayout();
	SetWidgetVisibility();
}

void MacroActionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	_channel->SetToken(_entryData->_token);
	_pointsReward->SetToken(_entryData->_token);
	_tags->SetToken(_entryData->_token);
	_language->SetToken(_entryData->_token);
	_contentClassification->SetToken(_entryData->_token);
	_entryData->ResetChatConnection();

	SetWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroActionTwitchEdit::SetTokenWarning(bool visible, const QString &text)
{
	_tokenWarning->setText(text);
	_tokenWarning->setVisible(visible);
	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::CheckToken()
{
	if (!_entryData) {
		return;
	}
	if (_entryData->_token.expired()) {
		SetTokenWarning(
			true,
			obs_module_text(
				"AdvSceneSwitcher.twitchToken.noSelection"));
		return;
	}
	if (!TokenIsValid(_entryData->_token)) {
		SetTokenWarning(
			true, obs_module_text(
				      "AdvSceneSwitcher.twitchToken.notValid"));
		return;
	}
	if (!_entryData->ActionIsSupportedByToken()) {
		SetTokenWarning(
			true,
			obs_module_text(
				"AdvSceneSwitcher.twitchToken.permissionsInsufficient"));
		return;
	}
	SetTokenWarning(false);
}

void MacroActionTwitchEdit::StreamTitleChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
}

void MacroActionTwitchEdit::CategoryChanged(const TwitchCategory &category)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_category = category;
}

void MacroActionTwitchEdit::TagsChanged(const TwitchTagList &tags)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_tags = tags;
}

void MacroActionTwitchEdit::LanguageChanged(const LanguageSelection &language)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_language = language;
}

void MacroActionTwitchEdit::ContentClassificationChanged(
	const ContentClassification &ccl)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_contentClassification = ccl;
}

void MacroActionTwitchEdit::MarkerDescriptionChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_markerDescription =
		_markerDescription->text().toStdString();
}

void MacroActionTwitchEdit::ClipHasDelayChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_clipHasDelay = state;
}

void MacroActionTwitchEdit::DurationChanged(const Duration &duration)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = duration;
}

void MacroActionTwitchEdit::AnnouncementMessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_announcementMessage =
		_announcementMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::AnnouncementColorChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_announcementColor =
		static_cast<MacroActionTwitch::AnnouncementColor>(index);
}

void MacroActionTwitchEdit::SetWidgetProperties()
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);
	_markerDescription->setSizePolicy(QSizePolicy::MinimumExpanding,
					  QSizePolicy::Preferred);
	_markerDescription->setMaxLength(140);
	_announcementMessage->setMaxLength(500);

	auto durationSpinBox = _duration->SpinBox();
	durationSpinBox->setMaximum(180);
	durationSpinBox->setSuffix("s");

	populateActionSelection(_actions);
	populateAnnouncementColorSelection(_announcementColor);
	populateUserQueryInfoTypeSelection(_userInfoQueryType);

	_userId->setMaximum(999999999999999);
	_userId->setDecimals(0);

	_toggleRewardSelection->setCheckable(true);
	_toggleRewardSelection->setMaximumWidth(11);
	SetButtonIcon(_toggleRewardSelection,
		      GetThemeTypeName() == "Light"
			      ? ":/res/images/dots-vert.svg"
			      : "theme:Dark/dots-vert.svg");
	_toggleRewardSelection->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.twitch.reward.toggleControl"));
}

void MacroActionTwitchEdit::SetWidgetSignalConnections()
{
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(&_tokenCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckToken()));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_category,
			 SIGNAL(CategoryChanged(const TwitchCategory &)), this,
			 SLOT(CategoryChanged(const TwitchCategory &)));
	QWidget::connect(_tags, SIGNAL(TagListChanged(const TwitchTagList &)),
			 this, SLOT(TagsChanged(const TwitchTagList &)));
	QWidget::connect(_language,
			 SIGNAL(LanguageChanged(const LanguageSelection &)),
			 this,
			 SLOT(LanguageChanged(const LanguageSelection &)));
	QWidget::connect(_contentClassification,
			 SIGNAL(ContentClassificationChanged(
				 const ContentClassification &)),
			 this,
			 SLOT(ContentClassificationChanged(
				 const ContentClassification &)));
	QWidget::connect(_markerDescription, SIGNAL(editingFinished()), this,
			 SLOT(MarkerDescriptionChanged()));
	QObject::connect(_clipHasDelay, SIGNAL(stateChanged(int)), this,
			 SLOT(ClipHasDelayChanged(int)));
	QObject::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_announcementMessage, SIGNAL(textChanged()), this,
			 SLOT(AnnouncementMessageChanged()));
	QWidget::connect(_announcementColor, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(AnnouncementColorChanged(int)));
	QWidget::connect(_channel,
			 SIGNAL(ChannelChanged(const TwitchChannel &)), this,
			 SLOT(ChannelChanged(const TwitchChannel &)));
	QWidget::connect(_chatMessage, SIGNAL(textChanged()), this,
			 SLOT(ChatMessageChanged()));
	QWidget::connect(_userInfoQueryType, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(UserInfoQueryTypeChanged(int)));
	QWidget::connect(_userLogin, SIGNAL(editingFinished()), this,
			 SLOT(UserLoginChanged()));
	QWidget::connect(
		_userId,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(UserIdChanged(const NumberVariable<double> &)));
	QWidget::connect(
		_pointsReward,
		SIGNAL(PointsRewardChanged(const TwitchPointsReward &)), this,
		SLOT(PointsRewardChanged(const TwitchPointsReward &)));
	QWidget::connect(_rewardVariable,
			 SIGNAL(SelectionChanged(const QString &)), this,
			 SLOT(RewardVariableChanged(const QString &)));
	QWidget::connect(_toggleRewardSelection, SIGNAL(toggled(bool)), this,
			 SLOT(ToggleRewardSelection(bool)));
}

void MacroActionTwitchEdit::SetWidgetVisibility()
{
	_streamTitle->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_TITLE_SET);
	_category->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_CATEGORY_SET);
	_tags->setVisible(_entryData->GetAction() ==
			  MacroActionTwitch::Action::CHANNEL_INFO_TAGS_SET);
	_language->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_LANGUAGE_SET);
	_contentClassification->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_CONTENT_LABELS_SET);
	_channel->setVisible(
		_entryData->GetAction() ==
			MacroActionTwitch::Action::RAID_START ||
		_entryData->GetAction() ==
			MacroActionTwitch::Action::RAID_END ||
		_entryData->GetAction() ==
			MacroActionTwitch::Action::SEND_CHAT_MESSAGE ||
		_entryData->GetAction() ==
			MacroActionTwitch::Action::POINTS_REWARD_GET_INFO);
	_duration->setVisible(_entryData->GetAction() ==
			      MacroActionTwitch::Action::COMMERCIAL_START);
	_markerDescription->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::MARKER_CREATE);
	_clipHasDelay->setVisible(_entryData->GetAction() ==
				  MacroActionTwitch::Action::CLIP_CREATE);
	_announcementMessage->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHAT_ANNOUNCEMENT_SEND);
	_announcementColor->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHAT_ANNOUNCEMENT_SEND);
	_chatMessage->setVisible(_entryData->GetAction() ==
				 MacroActionTwitch::Action::SEND_CHAT_MESSAGE);
	_userInfoQueryType->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::USER_GET_INFO);
	_userLogin->setVisible(
		_entryData->GetAction() ==
			MacroActionTwitch::Action::USER_GET_INFO &&
		_entryData->_userInfoQueryType ==
			MacroActionTwitch::UserInfoQueryType::LOGIN);
	_userId->setVisible(_entryData->GetAction() ==
				    MacroActionTwitch::Action::USER_GET_INFO &&
			    _entryData->_userInfoQueryType ==
				    MacroActionTwitch::UserInfoQueryType::ID);
	_pointsReward->setVisible(
		_entryData->GetAction() ==
			MacroActionTwitch::Action::POINTS_REWARD_GET_INFO &&
		!_entryData->_useVariableForRewardSelection);
	_rewardVariable->setVisible(
		_entryData->GetAction() ==
			MacroActionTwitch::Action::POINTS_REWARD_GET_INFO &&
		_entryData->_useVariableForRewardSelection);
	_toggleRewardSelection->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::POINTS_REWARD_GET_INFO);

	if (_entryData->GetAction() ==
		    MacroActionTwitch::Action::CHANNEL_INFO_TITLE_SET ||
	    _entryData->GetAction() ==
		    MacroActionTwitch::Action::MARKER_CREATE) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}

	CheckToken();

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::ChannelChanged(const TwitchChannel &channel)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_channel = channel;
	_pointsReward->SetChannel(channel);
	_entryData->ResetChatConnection();
}

void MacroActionTwitchEdit::ChatMessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_chatMessage = _chatMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::UserInfoQueryTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_userInfoQueryType =
		static_cast<MacroActionTwitch::UserInfoQueryType>(
			_userInfoQueryType->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionTwitchEdit::UserLoginChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_userLogin = _userLogin->text().toStdString();
}

void MacroActionTwitchEdit::RewardVariableChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK()
	_entryData->_rewardVariable = GetWeakVariableByQString(text);
}

void MacroActionTwitchEdit::ToggleRewardSelection(bool)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useVariableForRewardSelection =
		!_entryData->_useVariableForRewardSelection;
	SetWidgetVisibility();
}

void MacroActionTwitchEdit::UserIdChanged(const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_userId = value;
}

void MacroActionTwitchEdit::PointsRewardChanged(const TwitchPointsReward &reward)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pointsReward = reward;
}

void MacroActionTwitchEdit::SetWidgetLayout()
{
	const std::vector<QWidget *> widgets{_tokens,
					     _actions,
					     _streamTitle,
					     _category,
					     _markerDescription,
					     _clipHasDelay,
					     _duration,
					     _announcementColor,
					     _channel,
					     _userInfoQueryType,
					     _userLogin,
					     _userId,
					     _pointsReward,
					     _rewardVariable,
					     _toggleRewardSelection};
	for (auto widget : widgets) {
		_layout->removeWidget(widget);
	}
	ClearLayout(_layout);

	const char *layoutText;
	switch (_entryData->GetAction()) {
	case MacroActionTwitch::Action::SEND_CHAT_MESSAGE:
		layoutText = obs_module_text(
			"AdvSceneSwitcher.action.twitch.entry.chat");
		break;
	case MacroActionTwitch::Action::USER_GET_INFO:
		layoutText = obs_module_text(
			"AdvSceneSwitcher.action.twitch.entry.user.getInfo");
		break;
	case MacroActionTwitch::Action::POINTS_REWARD_GET_INFO:
		layoutText = obs_module_text(
			"AdvSceneSwitcher.action.twitch.entry.reward.getInfo");
		break;
	default:
		layoutText = obs_module_text(
			"AdvSceneSwitcher.action.twitch.entry.default");
		break;
	}

	PlaceWidgets(layoutText, _layout,
		     {{"{{account}}", _tokens},
		      {"{{actions}}", _actions},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{category}}", _category},
		      {"{{markerDescription}}", _markerDescription},
		      {"{{clipHasDelay}}", _clipHasDelay},
		      {"{{duration}}", _duration},
		      {"{{announcementColor}}", _announcementColor},
		      {"{{channel}}", _channel},
		      {"{{userInfoQueryType}}", _userInfoQueryType},
		      {"{{userLogin}}", _userLogin},
		      {"{{userId}}", _userId},
		      {"{{pointsReward}}", _pointsReward},
		      {"{{rewardVariable}}", _rewardVariable},
		      {"{{toggleRewardSelection}}", _toggleRewardSelection}});

	_layout->setContentsMargins(0, 0, 0, 0);
}

void MacroActionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->GetAction())));
	_tokens->SetToken(_entryData->_token);
	_streamTitle->setText(_entryData->_streamTitle);
	_category->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);
	_tags->SetTags(_entryData->_tags);
	_tags->SetToken(_entryData->_token);
	_language->SetLanguageSelection(_entryData->_language);
	_language->SetToken(_entryData->_token);
	_contentClassification->SetContentClassification(
		_entryData->_contentClassification);
	_contentClassification->SetToken(_entryData->_token);
	_markerDescription->setText(_entryData->_markerDescription);
	_clipHasDelay->setChecked(_entryData->_clipHasDelay);
	_duration->SetDuration(_entryData->_duration);
	_announcementMessage->setPlainText(_entryData->_announcementMessage);
	_announcementColor->setCurrentIndex(
		static_cast<int>(_entryData->_announcementColor));
	_channel->SetToken(_entryData->_token);
	_channel->SetChannel(_entryData->_channel);
	_chatMessage->setPlainText(_entryData->_chatMessage);
	_userInfoQueryType->setCurrentIndex(_userInfoQueryType->findData(
		static_cast<int>(_entryData->_userInfoQueryType)));
	_userLogin->setText(_entryData->_userLogin);
	_userId->SetValue(_entryData->_userId);
	_pointsReward->SetToken(_entryData->_token);
	_pointsReward->SetChannel(_entryData->_channel);
	_pointsReward->SetPointsReward(_entryData->_pointsReward);
	_rewardVariable->SetVariable(_entryData->_rewardVariable);

	SetWidgetVisibility();
}

} // namespace advss
