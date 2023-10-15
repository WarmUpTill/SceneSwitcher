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
	{MacroConditionTwitch::Condition::LIVE_EVENT_REGULAR,
	 "AdvSceneSwitcher.condition.twitch.type.channelWentLive"},
	{MacroConditionTwitch::Condition::LIVE_EVENT_PLAYLIST,
	 "AdvSceneSwitcher.condition.twitch.type.channelStartedPlaylist"},
	{MacroConditionTwitch::Condition::LIVE_EVENT_WATCHPARTY,
	 "AdvSceneSwitcher.condition.twitch.type.channelStartedWatchparty"},
	{MacroConditionTwitch::Condition::LIVE_EVENT_PREMIERE,
	 "AdvSceneSwitcher.condition.twitch.type.channelStartedPremiere"},
	{MacroConditionTwitch::Condition::LIVE_EVENT_RERUN,
	 "AdvSceneSwitcher.condition.twitch.type.channelStartedRerun"},
	{MacroConditionTwitch::Condition::OFFLINE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelWentOffline"},
	{MacroConditionTwitch::Condition::FOLLOW_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelFollow"},
	{MacroConditionTwitch::Condition::SUBSCRIBE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelSubscribe"},
	{MacroConditionTwitch::Condition::SUBSCRIBE_END_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelSubscribeEnd"},
	{MacroConditionTwitch::Condition::SUBSCRIBE_GIFT_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelSubscribeGift"},
	{MacroConditionTwitch::Condition::SUBSCRIBE_MESSAGE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelSubscribeMessage"},
	{MacroConditionTwitch::Condition::CHEER_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelCheer"},
	{MacroConditionTwitch::Condition::LIVE,
	 "AdvSceneSwitcher.condition.twitch.type.channelIsLive"},
	{MacroConditionTwitch::Condition::TITLE,
	 "AdvSceneSwitcher.condition.twitch.type.channelTitle"},
	{MacroConditionTwitch::Condition::CATEGORY,
	 "AdvSceneSwitcher.condition.twitch.type.category"},
	{MacroConditionTwitch::Condition::CHANNEL_UPDATE_EVENT,
	 "AdvSceneSwitcher.condition.twitch.type.channelUpdateEvent"},
};

const static std::map<MacroConditionTwitch::Condition, std::string>
	eventIdentifiers = {
		{MacroConditionTwitch::Condition::LIVE_EVENT_REGULAR,
		 "stream.online"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_PLAYLIST,
		 "stream.online"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_WATCHPARTY,
		 "stream.online"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_PREMIERE,
		 "stream.online"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_RERUN,
		 "stream.online"},
		{MacroConditionTwitch::Condition::OFFLINE_EVENT,
		 "stream.offline"},
		{MacroConditionTwitch::Condition::CHANNEL_UPDATE_EVENT,
		 "channel.update"},
		{MacroConditionTwitch::Condition::FOLLOW_EVENT,
		 "channel.follow"},
		{MacroConditionTwitch::Condition::SUBSCRIBE_EVENT,
		 "channel.subscribe"},
		{MacroConditionTwitch::Condition::SUBSCRIBE_END_EVENT,
		 "channel.subscription.end"},
		{MacroConditionTwitch::Condition::SUBSCRIBE_GIFT_EVENT,
		 "channel.subscription.gift"},
		{MacroConditionTwitch::Condition::SUBSCRIBE_MESSAGE_EVENT,
		 "channel.subscription.message"},
		{MacroConditionTwitch::Condition::CHEER_EVENT, "channel.cheer"},
};

const static std::map<MacroConditionTwitch::Condition, std::string>
	liveEventIDs = {
		{MacroConditionTwitch::Condition::LIVE_EVENT_REGULAR, "live"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_PLAYLIST,
		 "playlist"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_WATCHPARTY,
		 "watch_party"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_PREMIERE,
		 "premiere"},
		{MacroConditionTwitch::Condition::LIVE_EVENT_RERUN, "rerun"},
};

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

bool MacroConditionTwitch::CheckChannelOfflineEvents(TwitchToken &token)
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
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChannelUpdateEvents(TwitchToken &token)
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
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChannelFollowEvents(TwitchToken &token)
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
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChannelSubscribeEvents(TwitchToken &token)
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
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
}

bool MacroConditionTwitch::CheckChannelCheerEvents(TwitchToken &token)
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
		SetVariableValue(event.ToString());
		return true;
	}
	return false;
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
	case Condition::LIVE: {
		auto info = _channel.GetLiveInfo(*token);
		if (!info) {
			return false;
		}
		return info->IsLive();
	}
	case Condition::LIVE_EVENT_REGULAR:
	case Condition::LIVE_EVENT_PLAYLIST:
	case Condition::LIVE_EVENT_WATCHPARTY:
	case Condition::LIVE_EVENT_PREMIERE:
	case Condition::LIVE_EVENT_RERUN:
		return CheckChannelLiveEvents(*token);
	case Condition::TITLE: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->title);
		return titleMatches(_regex, info->title, _streamTitle);
	}
	case Condition::CATEGORY: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		SetVariableValue(info->game_name);
		return info->game_id == std::to_string(_category.id);
	}
	case Condition::CHANNEL_UPDATE_EVENT:
		return CheckChannelUpdateEvents(*token);
	case Condition::FOLLOW_EVENT:
		return CheckChannelFollowEvents(*token);
	case Condition::SUBSCRIBE_EVENT:
	case Condition::SUBSCRIBE_END_EVENT:
	case Condition::SUBSCRIBE_GIFT_EVENT:
	case Condition::SUBSCRIBE_MESSAGE_EVENT:
		return CheckChannelSubscribeEvents(*token);
	case Condition::CHEER_EVENT:
		return CheckChannelCheerEvents(*token);
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
	static const std::unordered_map<Condition, TokenOption> requiredOption = {
		{Condition::LIVE_EVENT_REGULAR, {""}},
		{Condition::LIVE_EVENT_PLAYLIST, {""}},
		{Condition::LIVE_EVENT_WATCHPARTY, {""}},
		{Condition::LIVE_EVENT_PREMIERE, {""}},
		{Condition::LIVE_EVENT_RERUN, {""}},
		{Condition::OFFLINE_EVENT, {""}},
		{Condition::CHANNEL_UPDATE_EVENT, {""}},
		{Condition::FOLLOW_EVENT, {"moderator:read:followers"}},
		{Condition::SUBSCRIBE_EVENT, {"channel:read:subscriptions"}},
		{Condition::SUBSCRIBE_END_EVENT,
		 {"channel:read:subscriptions"}},
		{Condition::SUBSCRIBE_GIFT_EVENT,
		 {"channel:read:subscriptions"}},
		{Condition::SUBSCRIBE_MESSAGE_EVENT,
		 {"channel:read:subscriptions"}},
		{Condition::CHEER_EVENT, {"bits:read"}},
		{Condition::LIVE, {""}},
		{Condition::TITLE, {""}},
		{Condition::CATEGORY, {""}},
	};
	auto token = _token.lock();
	if (!token) {
		return false;
	}
	auto option = requiredOption.find(_condition);
	assert(option != requiredOption.end());
	if (option == requiredOption.end()) {
		return false;
	}
	return option->second.apiId.empty() ||
	       token->OptionIsEnabled(option->second);
}

void MacroConditionTwitch::SetupEventSubscriptions()
{
	if (!IsUsingEventSubCondition()) {
		return;
	}

	switch (_condition) {
	case MacroConditionTwitch::Condition::LIVE_EVENT_REGULAR:
	case MacroConditionTwitch::Condition::LIVE_EVENT_PLAYLIST:
	case MacroConditionTwitch::Condition::LIVE_EVENT_WATCHPARTY:
	case MacroConditionTwitch::Condition::LIVE_EVENT_PREMIERE:
	case MacroConditionTwitch::Condition::LIVE_EVENT_RERUN:
		AddChannelLiveEventSubscription();
		break;
	case MacroConditionTwitch::Condition::OFFLINE_EVENT:
		AddChannelOfflineEventSubscription();
		break;
	case MacroConditionTwitch::Condition::CHANNEL_UPDATE_EVENT:
		AddChannelUpdateEventSubscription();
		break;
	case MacroConditionTwitch::Condition::FOLLOW_EVENT:
		AddChannelFollowEventSubscription();
		break;
	case MacroConditionTwitch::Condition::SUBSCRIBE_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIBE_END_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIBE_GIFT_EVENT:
	case MacroConditionTwitch::Condition::SUBSCRIBE_MESSAGE_EVENT:
		AddChannelSubscribeEventSubscription();
		break;
	default:
		break;
	}
}

bool MacroConditionTwitch::IsUsingEventSubCondition()
{
	const static std::set<Condition> eventConditions{
		Condition::LIVE_EVENT_REGULAR,
		Condition::LIVE_EVENT_PLAYLIST,
		Condition::LIVE_EVENT_WATCHPARTY,
		Condition::LIVE_EVENT_PREMIERE,
		Condition::LIVE_EVENT_RERUN,
		Condition::CHANNEL_UPDATE_EVENT,
		Condition::FOLLOW_EVENT,
		Condition::SUBSCRIBE_EVENT,
		Condition::SUBSCRIBE_END_EVENT,
		Condition::SUBSCRIBE_GIFT_EVENT,
		Condition::SUBSCRIBE_MESSAGE_EVENT,
		Condition::CHEER_EVENT,
	};
	return eventConditions.find(_condition) != eventConditions.end();
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

void MacroConditionTwitch::AddChannelLiveEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "1");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

void MacroConditionTwitch::AddChannelOfflineEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "1");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

void MacroConditionTwitch::AddChannelUpdateEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "2");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

void MacroConditionTwitch::AddChannelFollowEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "2");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
	obs_data_set_string(condition, "moderator_user_id",
			    token->GetUserID().c_str());
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

void MacroConditionTwitch::AddChannelSubscribeEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "1");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
	obs_data_set_obj(subscription.data, "condition", condition);
	_subscriptionIDFuture = waitForSubscription(token, subscription);
}

void MacroConditionTwitch::AddChannelCheerEventSubscription()
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
	obs_data_set_string(subscription.data, "version", "1");
	OBSDataAutoRelease condition = obs_data_create();
	obs_data_set_string(condition, "broadcaster_user_id",
			    _channel.GetUserID(*token).c_str());
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
	_streamTitle->setVisible(condition ==
				 MacroConditionTwitch::Condition::TITLE);
	_regex->setVisible(condition == MacroConditionTwitch::Condition::TITLE);
	_category->setVisible(condition ==
			      MacroConditionTwitch::Condition::CATEGORY);
	if (condition == MacroConditionTwitch::Condition::TITLE) {
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
