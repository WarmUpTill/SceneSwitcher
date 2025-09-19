#include "channel-selection.hpp"
#include "log-helper.hpp"
#include "twitch-helpers.hpp"
#include "token.hpp"

#include <QLayout>
#include <QDesktopServices>
#include <obs-module-helper.hpp>
#include <nlohmann/json.hpp>

namespace advss {

bool ChannelLiveInfo::IsLive() const
{
	return type == "live";
}

void TwitchChannel::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "channel");
	_name.Load(data, "name");
}

void TwitchChannel::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	_name.Save(data, "name");
	obs_data_set_obj(obj, "channel", data);
}

std::string TwitchChannel::GetUserID(const TwitchToken &token) const
{
	static std::map<std::string, std::string> userIDCache;
	if (!token.IsValid()) {
		vblog(LOG_INFO, "%s() failed: token invalid", __func__);
		return "invalid";
	}

	auto it = userIDCache.find(std::string(_name));
	if (it != userIDCache.end()) {
		return it->second;
	}

	auto res = SendGetRequest(token, "https://api.twitch.tv",
				  "/helix/users", {{"login", _name}}, true);

	if (res.status == 400) {
		userIDCache[_name] = "invalid";
		blog(LOG_INFO, "ignoring invalid channel selection '%s'!",
		     _name.c_str());
		return "invalid";
	}

	if (res.status != 200) {
		blog(LOG_INFO, "failed to get Twitch user id for channel! (%d)",
		     res.status);
		return "invalid";
	}
	OBSDataArrayAutoRelease array = obs_data_get_array(res.data, "data");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		std::string id = obs_data_get_string(arrayObj, "id");
		userIDCache[_name] = id;
		return id;
	}
	userIDCache[_name] = "invalid";
	return "invalid";
}

static QDate parseRFC3339Date(const std::string &rfc3339Date)
{
	QString rfc3339QString = QString::fromStdString(rfc3339Date);
	QRegularExpression regex("(\\d{4})-(\\d{2})-(\\d{2})");
	QRegularExpressionMatch match = regex.match(rfc3339QString);

	if (match.hasMatch()) {
		int year = match.captured(1).toInt();
		int month = match.captured(2).toInt();
		int day = match.captured(3).toInt();
		QDate date(year, month, day);
		return date;
	} else {
		return QDate();
	}
}

static std::vector<std::string>
getStringArrayHelper(const OBSDataAutoRelease &data, const std::string &value)
{
	std::vector<std::string> result;
	auto jsonStr = obs_data_get_json(data);
	if (!jsonStr) {
		return result;
	}
	try {
		auto json = nlohmann::json::parse(jsonStr);
		auto &array = json[value];
		if (!array.is_array()) {
			return result;
		}
		for (const auto &element : array) {
			std::string strElement = element.get<std::string>();
			result.emplace_back(strElement);
		}
	} catch (const nlohmann::json::exception &) {
	}
	return result;
}

std::optional<ChannelLiveInfo>
TwitchChannel::GetLiveInfo(const TwitchToken &token) const
{
	auto id = GetUserID(token);
	if (!IsValid(id)) {
		return {};
	}

	httplib::Params params = {{"first", "1"},
				  {"after", ""},
				  {"user_id", id}};
	auto result = SendGetRequest(token, "https://api.twitch.tv",
				     "/helix/streams", params, true);
	if (result.status != 200) {
		return {};
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(result.data, "data");
	size_t count = obs_data_array_count(array);
	if (count == 0) {
		return {};
	}
	OBSDataAutoRelease data = obs_data_array_item(array, 0);

	ChannelLiveInfo info;
	info.id = obs_data_get_string(data, "id");
	info.user_id = obs_data_get_string(data, "user_id");
	info.user_login = obs_data_get_string(data, "user_login");
	info.user_name = obs_data_get_string(data, "user_name");
	info.game_id = obs_data_get_string(data, "game_id");
	info.game_name = obs_data_get_string(data, "game_name");
	info.type = obs_data_get_string(data, "type");
	info.title = obs_data_get_string(data, "title");
	info.tags = getStringArrayHelper(data, "tags");
	info.viewer_count = obs_data_get_int(data, "viewer_count");
	info.started_at =
		parseRFC3339Date(obs_data_get_string(data, "started_at"));
	info.language = obs_data_get_string(data, "language");
	info.thumbnail_url = obs_data_get_string(data, "thumbnail_url");
	info.is_mature = obs_data_get_bool(data, "is_mature");
	return info;
}

std::optional<ChannelInfo>
TwitchChannel::GetInfo(const TwitchToken &token) const
{
	auto id = GetUserID(token);
	if (!IsValid(id)) {
		return {};
	}

	httplib::Params params = {{"first", "1"},
				  {"after", ""},
				  {"broadcaster_id", id}};
	auto result = SendGetRequest(token, "https://api.twitch.tv",
				     "/helix/channels", params, true);
	if (result.status != 200) {
		return {};
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(result.data, "data");
	size_t count = obs_data_array_count(array);
	if (count == 0) {
		return {};
	}
	OBSDataAutoRelease data = obs_data_array_item(array, 0);

	ChannelInfo info;
	info.broadcaster_id = obs_data_get_string(data, "broadcaster_id");
	info.broadcaster_login = obs_data_get_string(data, "broadcaster_login");
	info.broadcaster_name = obs_data_get_string(data, "broadcaster_name");
	info.broadcaster_language =
		obs_data_get_string(data, "broadcaster_language");
	info.game_name = obs_data_get_string(data, "game_name");
	info.game_id = obs_data_get_string(data, "game_id");
	info.title = obs_data_get_string(data, "title");
	info.delay = obs_data_get_int(data, "delay");
	info.tags = getStringArrayHelper(data, "tags");
	info.content_classification_labels =
		getStringArrayHelper(data, "content_classification_labels");
	info.is_branded_content = obs_data_get_bool(data, "is_branded_content");
	return info;
}

void TwitchChannel::ResolveVariables()
{
	_name.ResolveVariables();
}

bool TwitchChannel::IsValid(const std::string &id)
{
	return id != "invalid" && !id.empty();
}

TwitchChannelSelection::TwitchChannelSelection(QWidget *parent)
	: QWidget(parent),
	  _channelName(new VariableLineEdit(this)),
	  _openChannel(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.twitch.selection.channel.open")))
{
	QWidget::connect(_channelName, SIGNAL(editingFinished()), this,
			 SLOT(SelectionChanged()));
	QWidget::connect(_openChannel, SIGNAL(pressed()), this,
			 SLOT(OpenChannel()));
	QWidget::connect(this, SIGNAL(ChannelChanged(const TwitchChannel &)),
			 this,
			 SLOT(SetOpenChannelState(const TwitchChannel &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_channelName);
	layout->addWidget(_openChannel);
	setLayout(layout);
}

void TwitchChannelSelection::SetChannel(const TwitchChannel &channel)
{
	_channelName->setText(channel._name);

	emit ChannelChanged(channel);
}

void TwitchChannelSelection::SetToken(const std::weak_ptr<TwitchToken> &token)
{
	_token = token;
}

void TwitchChannelSelection::OpenChannel()
{
	// Resolve potential variables
	StringVariable temp = _channelName->text().toStdString();
	QDesktopServices::openUrl("https://www.twitch.tv/" +
				  QString::fromStdString(temp));
}

void TwitchChannelSelection::SetOpenChannelState(const TwitchChannel &channel)
{
	auto token = _token.lock();

	if (!token) {
		_openChannel->setToolTip(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.channel.open.tooltip.noAccount"));
		_openChannel->setDisabled(false);
		return;
	}

	if (channel.GetUserID(*token) != "invalid") {
		_openChannel->setToolTip(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.channel.open.tooltip.details"));
		_openChannel->setDisabled(false);
		return;
	}

	_openChannel->setToolTip(obs_module_text(
		"AdvSceneSwitcher.twitch.selection.channel.open.tooltip.noChannel"));
	_openChannel->setDisabled(true);
}

void TwitchChannelSelection::SelectionChanged()
{
	TwitchChannel channel;
	channel._name = _channelName->text().toStdString();

	emit ChannelChanged(channel);
}

} // namespace advss
