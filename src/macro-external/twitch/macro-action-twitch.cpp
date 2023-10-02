#include "macro-action-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <utility.hpp>

namespace advss {

const std::string MacroActionTwitch::id = "twitch";

bool MacroActionTwitch::_registered = MacroActionFactory::Register(
	MacroActionTwitch::id,
	{MacroActionTwitch::Create, MacroActionTwitchEdit::Create,
	 "AdvSceneSwitcher.action.twitch"});

const static std::map<MacroActionTwitch::Action, std::string> actionTypes = {
	{MacroActionTwitch::Action::TITLE,
	 "AdvSceneSwitcher.action.twitch.type.title"},
	{MacroActionTwitch::Action::CATEGORY,
	 "AdvSceneSwitcher.action.twitch.type.category"},
	{MacroActionTwitch::Action::MARKER,
	 "AdvSceneSwitcher.action.twitch.type.marker"},
	{MacroActionTwitch::Action::CLIP,
	 "AdvSceneSwitcher.action.twitch.type.clip"},
	{MacroActionTwitch::Action::COMMERCIAL,
	 "AdvSceneSwitcher.action.twitch.type.commercial"},
	{MacroActionTwitch::Action::ANNOUNCEMENT,
	 "AdvSceneSwitcher.action.twitch.type.announcement"},
	{MacroActionTwitch::Action::EMOTE_ONLY,
	 "AdvSceneSwitcher.action.twitch.type.emoteOnly"},
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

void MacroActionTwitch::SetStreamTitle(
	const std::shared_ptr<TwitchToken> &token) const
{
	if (std::string(_streamTitle).empty()) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "title", _streamTitle.c_str());
	auto result = SendPatchRequest(
		"https://api.twitch.tv",
		std::string("/helix/channels?broadcaster_id=") +
			token->GetUserID(),
		*token, data.Get());

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
	auto result = SendPatchRequest(
		"https://api.twitch.tv",
		std::string("/helix/channels?broadcaster_id=") +
			token->GetUserID(),
		*token, data.Get());
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

	auto result = SendPostRequest("https://api.twitch.tv",
				      "/helix/streams/markers", *token,
				      data.Get());

	if (result.status != 200) {
		blog(LOG_INFO, "Failed to create marker! (%d)", result.status);
	}
}

void MacroActionTwitch::CreateStreamClip(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	auto hasDelay = _clipHasDelay ? "true" : "false";

	auto result = SendPostRequest(
		"https://api.twitch.tv",
		"/helix/clips?broadcaster_id=" + token->GetUserID() +
			"&has_delay=" + hasDelay,
		*token, data.Get());

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
	auto result = SendPostRequest("https://api.twitch.tv",
				      "/helix/channels/commercial", *token,
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

	auto result =
		SendPostRequest("https://api.twitch.tv",
				"/helix/chat/announcements?broadcaster_id=" +
					userId + "&moderator_id=" + userId,
				*token, data.Get());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to send chat announcement! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::SetChatEmoteOnlyMode(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "emote_mode", _emoteOnlyEnabled);
	auto userId = token->GetUserID();

	auto result =
		SendPatchRequest("https://api.twitch.tv",
				 "/helix/chat/settings?broadcaster_id=" +
					 userId + "&moderator_id=" + userId,
				 *token, data.Get());

	if (result.status != 200) {
		blog(LOG_INFO, "Failed to set chat's emote-only mode! (%d)",
		     result.status);
	}
}

bool MacroActionTwitch::PerformAction()
{
	auto token = _token.lock();
	if (!token) {
		return true;
	}

	switch (_action) {
	case MacroActionTwitch::Action::TITLE:
		SetStreamTitle(token);
		break;
	case MacroActionTwitch::Action::CATEGORY:
		SetStreamCategory(token);
		break;
	case MacroActionTwitch::Action::MARKER:
		CreateStreamMarker(token);
		break;
	case MacroActionTwitch::Action::CLIP:
		CreateStreamClip(token);
		break;
	case MacroActionTwitch::Action::COMMERCIAL:
		StartCommercial(token);
		break;
	case MacroActionTwitch::Action::ANNOUNCEMENT:
		SendChatAnnouncement(token);
		break;
	case MacroActionTwitch::Action::EMOTE_ONLY:
		SetChatEmoteOnlyMode(token);
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
	obs_data_set_bool(obj, "emoteOnlyEnabled", _emoteOnlyEnabled);

	return true;
}

bool MacroActionTwitch::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_category.Load(obj);
	_markerDescription.Load(obj, "markerDescription");
	_clipHasDelay = obs_data_get_bool(obj, "clipHasDelay");
	_duration.Load(obj);
	_announcementMessage.Load(obj, "announcementMessage");
	_announcementColor = static_cast<AnnouncementColor>(
		obs_data_get_int(obj, "announcementColor"));
	_emoteOnlyEnabled = obs_data_get_bool(obj, "emoteOnlyEnabled");

	return true;
}

std::string MacroActionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

bool MacroActionTwitch::ActionIsSupportedByToken()
{
	static const std::unordered_map<Action, TokenOption> requiredOption = {
		{Action::TITLE, {"channel:manage:broadcast"}},
		{Action::CATEGORY, {"channel:manage:broadcast"}},
		{Action::MARKER, {"channel:manage:broadcast"}},
		{Action::CLIP, {"clips:edit"}},
		{Action::COMMERCIAL, {"channel:edit:commercial"}},
		{Action::ANNOUNCEMENT, {"moderator:manage:announcements"}},
		{Action::EMOTE_ONLY, {"moderator:manage:chat_settings"}},
	};
	auto token = _token.lock();
	if (!token) {
		return false;
	}
	auto option = requiredOption.find(_action);
	assert(option != requiredOption.end());
	return token->OptionIsEnabled(option->second);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
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
	  _firstLineLayout(new QHBoxLayout()),
	  _secondLineLayout(new QHBoxLayout()),
	  _actions(new QComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _tokenPermissionWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.action.twitch.tokenPermissionsInsufficient"))),
	  _streamTitle(new VariableLineEdit(this)),
	  _category(new TwitchCategorySelection(this)),
	  _manualCategorySearch(new TwitchCategorySearchButton()),
	  _markerDescription(new VariableLineEdit(this)),
	  _clipHasDelay(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.twitch.clip.hasDelay"))),
	  _duration(new DurationSelection(this, false, 0)),
	  _announcementMessage(new VariableTextEdit(this)),
	  _announcementColor(new QComboBox(this)),
	  _emoteOnlyEnabled(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.twitch.emoteOnly.enabled")))
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);
	_markerDescription->setSizePolicy(QSizePolicy::MinimumExpanding,
					  QSizePolicy::Preferred);
	_markerDescription->setMaxLength(140);
	_announcementMessage->setSizePolicy(QSizePolicy::MinimumExpanding,
					    QSizePolicy::Preferred);

	auto spinBox = _duration->SpinBox();
	spinBox->setSuffix("s");
	spinBox->setMaximum(180);

	populateActionSelection(_actions);
	populateAnnouncementColorSelection(_announcementColor);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(&_tokenPermissionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckTokenPermissions()));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_category,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QWidget::connect(_markerDescription, SIGNAL(editingFinished()), this,
			 SLOT(MarkerDescriptionChanged()));
	QObject::connect(_clipHasDelay, SIGNAL(stateChanged(int)), this,
			 SLOT(HasClipDelayChanged(int)));
	QObject::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_announcementMessage, SIGNAL(textChanged()), this,
			 SLOT(AnnouncementMessageChanged()));
	QWidget::connect(_announcementColor, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(AnnouncementColorChanged(int)));
	QObject::connect(_emoteOnlyEnabled, SIGNAL(stateChanged(int)), this,
			 SLOT(EmoteOnlyEnabledChanged(int)));

	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.twitch.entry.line1"),
		_firstLineLayout,
		{{"{{account}}", _tokens},
		 {"{{actions}}", _actions},
		 {"{{streamTitle}}", _streamTitle},
		 {"{{category}}", _category},
		 {"{{manualCategorySearch}}", _manualCategorySearch},
		 {"{{markerDescription}}", _markerDescription},
		 {"{{clipHasDelay}}", _clipHasDelay},
		 {"{{duration}}", _duration},
		 {"{{announcementColor}}", _announcementColor},
		 {"{{emoteOnlyEnabled}}", _emoteOnlyEnabled}});
	_firstLineLayout->setContentsMargins(0, 0, 0, 0);

	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.twitch.entry.line2"),
		_secondLineLayout,
		{{"{{announcementMessage}}", _announcementMessage}});
	_secondLineLayout->setContentsMargins(0, 0, 0, 0);

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_firstLineLayout);
	mainLayout->addLayout(_secondLineLayout);
	mainLayout->addWidget(_tokenPermissionWarning);
	setLayout(mainLayout);

	_tokenPermissionCheckTimer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	_manualCategorySearch->SetToken(_entryData->_token);
	SetupWidgetVisibility();
	emit(HeaderInfoChanged(token));
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

void MacroActionTwitchEdit::EmoteOnlyEnabledChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_emoteOnlyEnabled = state;
}

void MacroActionTwitchEdit::CheckTokenPermissions()
{
	_tokenPermissionWarning->setVisible(
		_entryData && !_entryData->ActionIsSupportedByToken());
	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::SetupWidgetVisibility()
{
	_streamTitle->setVisible(_entryData->_action ==
				 MacroActionTwitch::Action::TITLE);
	_category->setVisible(_entryData->_action ==
			      MacroActionTwitch::Action::CATEGORY);
	_manualCategorySearch->setVisible(_entryData->_action ==
					  MacroActionTwitch::Action::CATEGORY);
	_markerDescription->setVisible(_entryData->_action ==
				       MacroActionTwitch::Action::MARKER);
	_clipHasDelay->setVisible(_entryData->_action ==
				  MacroActionTwitch::Action::CLIP);
	_duration->setVisible(_entryData->_action ==
			      MacroActionTwitch::Action::COMMERCIAL);
	_announcementMessage->setVisible(
		_entryData->_action == MacroActionTwitch::Action::ANNOUNCEMENT);
	_announcementColor->setVisible(_entryData->_action ==
				       MacroActionTwitch::Action::ANNOUNCEMENT);
	_emoteOnlyEnabled->setVisible(_entryData->_action ==
				      MacroActionTwitch::Action::EMOTE_ONLY);

	if (_entryData->_action == MacroActionTwitch::Action::TITLE ||
	    _entryData->_action == MacroActionTwitch::Action::MARKER) {
		RemoveStretchIfPresent(_firstLineLayout);
	} else {
		AddStretchIfNecessary(_firstLineLayout);
	}

	if (_entryData->_action == MacroActionTwitch::Action::ANNOUNCEMENT) {
		RemoveStretchIfPresent(_secondLineLayout);
	} else {
		AddStretchIfNecessary(_secondLineLayout);
	}

	_tokenPermissionWarning->setVisible(
		!_entryData->ActionIsSupportedByToken());

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_tokens->SetToken(_entryData->_token);
	_streamTitle->setText(_entryData->_streamTitle);
	_category->SetToken(_entryData->_token);
	_manualCategorySearch->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);
	_markerDescription->setText(_entryData->_markerDescription);
	_clipHasDelay->setChecked(_entryData->_clipHasDelay);
	_duration->SetDuration(_entryData->_duration);
	_announcementMessage->setPlainText(_entryData->_announcementMessage);
	_announcementColor->setCurrentIndex(
		static_cast<int>(_entryData->_announcementColor));
	_emoteOnlyEnabled->setChecked(_entryData->_emoteOnlyEnabled);

	SetupWidgetVisibility();
}

void MacroActionTwitchEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionTwitch::Action>(value);
	SetupWidgetVisibility();
}

} // namespace advss
