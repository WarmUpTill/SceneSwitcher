#include "macro-action-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <layout-helpers.hpp>

namespace advss {

const std::string MacroActionTwitch::id = "twitch";

bool MacroActionTwitch::_registered = MacroActionFactory::Register(
	MacroActionTwitch::id,
	{MacroActionTwitch::Create, MacroActionTwitchEdit::Create,
	 "AdvSceneSwitcher.action.twitch"});

std::string MacroActionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

const static std::map<MacroActionTwitch::Action, std::string> actionTypes = {
	{MacroActionTwitch::Action::CHANNEL_INFO_TITLE_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.title.set"},
	{MacroActionTwitch::Action::CHANNEL_INFO_CATEGORY_SET,
	 "AdvSceneSwitcher.action.twitch.type.channel.info.category.set"},
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
	 "AdvSceneSwitcher.action.twitch.type.sendChatMessage"},
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

	if (result.status != 200) {
		OBSDataArrayAutoRelease replyArray =
			obs_data_get_array(result.data, "data");
		OBSDataAutoRelease replyData =
			obs_data_array_item(replyArray, 0);
		blog(LOG_INFO,
		     "Failed to start commercial! (%d)\n"
		     "length: %lld\n"
		     "message: %s\n"
		     "retry_after: %lld\n",
		     result.status, obs_data_get_int(replyData, "length"),
		     obs_data_get_string(replyData, "message"),
		     obs_data_get_int(replyData, "retry_after"));
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
	case MacroActionTwitch::Action::SEND_CHAT_MESSAGE:
		SendChatMessage(token);
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
		vblog(LOG_INFO, "performed action \"%s\" with token for \"%s\"",
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
	_markerDescription.Save(obj, "markerDescription");
	obs_data_set_bool(obj, "clipHasDelay", _clipHasDelay);
	_duration.Save(obj);
	_announcementMessage.Save(obj, "announcementMessage");
	obs_data_set_int(obj, "announcementColor",
			 static_cast<int>(_announcementColor));
	_channel.Save(obj);
	_chatMessage.Save(obj, "chatMessage");

	return true;
}

bool MacroActionTwitch::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);

	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_category.Load(obj);
	_markerDescription.Load(obj, "markerDescription");
	_clipHasDelay = obs_data_get_bool(obj, "clipHasDelay");
	_duration.Load(obj);
	_announcementMessage.Load(obj, "announcementMessage");
	_announcementColor = static_cast<AnnouncementColor>(
		obs_data_get_int(obj, "announcementColor"));
	_channel.Load(obj);
	_chatMessage.Load(obj, "chatMessage");
	SetAction(static_cast<Action>(obs_data_get_int(obj, "action")));

	return true;
}

void MacroActionTwitch::SetAction(Action action)
{
	_action = action;
	ResetChatConnection();
}

bool MacroActionTwitch::ActionIsSupportedByToken()
{
	static const std::unordered_map<Action, TokenOption> requiredOption = {
		{Action::CHANNEL_INFO_TITLE_SET, {"channel:manage:broadcast"}},
		{Action::CHANNEL_INFO_CATEGORY_SET,
		 {"channel:manage:broadcast"}},
		{Action::CHANNEL_INFO_LANGUAGE_SET,
		 {"channel:manage:broadcast"}},
		{Action::CHANNEL_INFO_DELAY_SET, {"channel:manage:broadcast"}},
		{Action::CHANNEL_INFO_BRANDED_CONTENT_ENABLE,
		 {"channel:manage:broadcast"}},
		{Action::CHANNEL_INFO_BRANDED_CONTENT_DISABLE,
		 {"channel:manage:broadcast"}},
		{Action::RAID_START, {"channel:manage:raids"}},
		{Action::RAID_END, {"channel:manage:raids"}},
		{Action::SHOUTOUT_SEND, {"moderator:manage:shoutouts"}},
		{Action::POLL_END, {"channel:manage:polls"}},
		{Action::PREDICTION_END, {"channel:manage:predictions"}},
		{Action::SHIELD_MODE_START, {"moderator:manage:shield_mode"}},
		{Action::SHIELD_MODE_END, {"moderator:manage:shield_mode"}},
		{Action::POINTS_REWARD_ENABLE, {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_DISABLE, {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_PAUSE, {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_UNPAUSE, {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_TITLE_SET,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_PROMPT_SET,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_COST_SET,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_USER_INPUT_REQUIRE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_USER_INPUT_UNREQUIRE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_COOLDOWN_ENABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_COOLDOWN_DISABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_QUEUE_SKIP_ENABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_QUEUE_SKIP_DISABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_MAX_PER_STREAM_ENABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_MAX_PER_STREAM_DISABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_MAX_PER_USER_ENABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_MAX_PER_USER_DISABLE,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_DELETE, {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_REDEMPTION_CANCEL,
		 {"channel:manage:redemptions"}},
		{Action::POINTS_REWARD_REDEMPTION_FULFILL,
		 {"channel:manage:redemptions"}},
		{Action::USER_BAN, {"moderator:manage:banned_users"}},
		{Action::USER_UNBAN, {"moderator:manage:banned_users"}},
		{Action::USER_BLOCK, {"user:manage:blocked_users"}},
		{Action::USER_UNBLOCK, {"user:manage:blocked_users"}},
		{Action::USER_MODERATOR_ADD, {"channel:manage:moderators"}},
		{Action::USER_MODERATOR_DELETE, {"channel:manage:moderators"}},
		{Action::USER_VIP_ADD, {"channel:manage:vips"}},
		{Action::USER_VIP_DELETE, {"channel:manage:vips"}},
		{Action::COMMERCIAL_START, {"channel:edit:commercial"}},
		{Action::COMMERCIAL_SNOOZE, {"channel:manage:ads"}},
		{Action::MARKER_CREATE, {"channel:manage:broadcast"}},
		{Action::CLIP_CREATE, {"clips:edit"}},
		{Action::CHAT_ANNOUNCEMENT_SEND,
		 {"moderator:manage:announcements"}},
		{Action::CHAT_EMOTE_ONLY_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_EMOTE_ONLY_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_FOLLOWER_ONLY_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_FOLLOWER_ONLY_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_SUBSCRIBER_ONLY_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_SUBSCRIBER_ONLY_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_SLOW_MODE_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_SLOW_MODE_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_NON_MODERATOR_DELAY_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_NON_MODERATOR_DELAY_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_UNIQUE_MODE_ENABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::CHAT_UNIQUE_MODE_DISABLE,
		 {"moderator:manage:chat_settings"}},
		{Action::WHISPER_SEND, {"user:manage:whispers"}},
		{Action::SEND_CHAT_MESSAGE, {"chat:edit"}},
	};

	auto token = _token.lock();
	if (!token) {
		return false;
	}

	auto option = requiredOption.find(_action);
	assert(option != requiredOption.end());
	if (option == requiredOption.end()) {
		return false;
	}

	return token->OptionIsEnabled(option->second);
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

MacroActionTwitchEdit::MacroActionTwitchEdit(
	QWidget *parent, std::shared_ptr<MacroActionTwitch> entryData)
	: QWidget(parent),
	  _layout(new QHBoxLayout()),
	  _actions(new FilterComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _tokenWarning(new QLabel()),
	  _streamTitle(new VariableLineEdit(this)),
	  _category(new TwitchCategoryWidget(this)),
	  _markerDescription(new VariableLineEdit(this)),
	  _clipHasDelay(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.twitch.clip.hasDelay"))),
	  _duration(new DurationSelection(this, false, 0)),
	  _announcementMessage(new VariableTextEdit(this)),
	  _announcementColor(new QComboBox(this)),
	  _channel(new TwitchChannelSelection(this)),
	  _chatMessage(new VariableTextEdit(this))
{
	SetWidgetProperties();
	SetWidgetSignalConnections();

	_entryData = entryData;
	SetWidgetLayout();

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_layout);
	mainLayout->addWidget(_announcementMessage);
	mainLayout->addWidget(_chatMessage);
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	_channel->SetToken(_entryData->_token);
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
}

void MacroActionTwitchEdit::CategoreyChanged(const TwitchCategory &category)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_category = category;
}

void MacroActionTwitchEdit::MarkerDescriptionChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_markerDescription =
		_markerDescription->text().toStdString();
}

void MacroActionTwitchEdit::ClipHasDelayChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_clipHasDelay = state;
}

void MacroActionTwitchEdit::DurationChanged(const Duration &duration)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_duration = duration;
}

void MacroActionTwitchEdit::AnnouncementMessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_announcementMessage =
		_announcementMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::AnnouncementColorChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
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
}

void MacroActionTwitchEdit::SetWidgetVisibility()
{
	_streamTitle->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_TITLE_SET);
	_category->setVisible(
		_entryData->GetAction() ==
		MacroActionTwitch::Action::CHANNEL_INFO_CATEGORY_SET);
	_channel->setVisible(
		_entryData->GetAction() ==
			MacroActionTwitch::Action::RAID_START ||
		_entryData->GetAction() ==
			MacroActionTwitch::Action::RAID_END ||
		_entryData->GetAction() ==
			MacroActionTwitch::Action::SEND_CHAT_MESSAGE);
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_channel = channel;
	_entryData->ResetChatConnection();
}

void MacroActionTwitchEdit::ChatMessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_chatMessage = _chatMessage->toPlainText().toStdString();

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::SetWidgetLayout()
{
	const std::vector<QWidget *> widgets{
		_tokens,   _actions,           _streamTitle,
		_category, _markerDescription, _clipHasDelay,
		_duration, _announcementColor, _channel};
	for (auto widget : widgets) {
		_layout->removeWidget(widget);
	}
	ClearLayout(_layout);

	auto layoutText =
		_entryData->GetAction() ==
				MacroActionTwitch::Action::SEND_CHAT_MESSAGE
			? obs_module_text(
				  "AdvSceneSwitcher.action.twitch.entry.chat")
			: obs_module_text(
				  "AdvSceneSwitcher.action.twitch.entry.default");
	PlaceWidgets(layoutText, _layout,
		     {{"{{account}}", _tokens},
		      {"{{actions}}", _actions},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{category}}", _category},
		      {"{{markerDescription}}", _markerDescription},
		      {"{{clipHasDelay}}", _clipHasDelay},
		      {"{{duration}}", _duration},
		      {"{{announcementColor}}", _announcementColor},
		      {"{{channel}}", _channel}});
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
	_markerDescription->setText(_entryData->_markerDescription);
	_clipHasDelay->setChecked(_entryData->_clipHasDelay);
	_duration->SetDuration(_entryData->_duration);
	_announcementMessage->setPlainText(_entryData->_announcementMessage);
	_announcementColor->setCurrentIndex(
		static_cast<int>(_entryData->_announcementColor));
	_channel->SetToken(_entryData->_token);
	_channel->SetChannel(_entryData->_channel);
	_chatMessage->setPlainText(_entryData->_chatMessage);

	SetWidgetVisibility();
}

} // namespace advss
