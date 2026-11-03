#include "broadcast-selection.hpp"
#include "youtube-connection.hpp"
#include "youtube-helpers.hpp"

#include <log-helper.hpp>
#include <obs-module-helper.hpp>

#include <obs-frontend-api.h>
#include <mutex>
#include <QHBoxLayout>
#include <unordered_map>

namespace advss {

static const char *broadcastParts = "id,snippet,status,contentDetails";

static YouTubeBroadcastInfo parseBroadcast(obs_data_t *item)
{
	OBSDataAutoRelease snippet = obs_data_get_obj(item, "snippet");
	OBSDataAutoRelease status = obs_data_get_obj(item, "status");
	OBSDataAutoRelease contentDetails =
		obs_data_get_obj(item, "contentDetails");
	OBSDataAutoRelease monitorStream =
		obs_data_get_obj(contentDetails, "monitorStream");

	YouTubeBroadcastInfo info;
	info.id = obs_data_get_string(item, "id");
	info.title = obs_data_get_string(snippet, "title");
	info.scheduledStartTime =
		obs_data_get_string(snippet, "scheduledStartTime");
	info.lifeCycleStatus = obs_data_get_string(status, "lifeCycleStatus");
	info.boundStreamId =
		obs_data_get_string(contentDetails, "boundStreamId");
	info.monitorStreamEnabled =
		obs_data_get_bool(monitorStream, "enableMonitorStream");
	return info;
}

static std::vector<YouTubeBroadcastInfo>
listBroadcasts(YouTubeConnection &connection, httplib::Params params)
{
	params.emplace("part", broadcastParts);
	params.emplace("maxResults", "50");
	const auto result = SendYouTubeRequest(connection,
					       YouTubeRequestMethod::GET,
					       "/liveBroadcasts", params);
	if (result.status != 200) {
		return {};
	}

	std::vector<YouTubeBroadcastInfo> broadcasts;
	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	const size_t count = obs_data_array_count(items);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(items, i);
		broadcasts.emplace_back(parseBroadcast(item));
	}
	return broadcasts;
}

static std::string getObsStreamKey()
{
	auto service = obs_frontend_get_streaming_service();
	if (!service) {
		return "";
	}
	const char *key = obs_service_get_connect_info(
		service, OBS_SERVICE_CONNECT_INFO_STREAM_KEY);
	return key ? key : "";
}

static std::string requestStreamId(YouTubeConnection &connection,
				   const std::string &streamKey)
{
	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::GET, "/liveStreams",
		{{"part", "cdn"}, {"mine", "true"}, {"maxResults", "50"}});
	if (result.status != 200) {
		return "";
	}

	OBSDataArrayAutoRelease items =
		obs_data_get_array(result.data, "items");
	const size_t count = obs_data_array_count(items);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(items, i);
		OBSDataAutoRelease cdn = obs_data_get_obj(item, "cdn");
		OBSDataAutoRelease ingestionInfo =
			obs_data_get_obj(cdn, "ingestionInfo");
		if (streamKey ==
		    obs_data_get_string(ingestionInfo, "streamName")) {
			return obs_data_get_string(item, "id");
		}
	}
	return "";
}

static std::string getStreamId(YouTubeConnection &connection,
			       const std::string &streamKey)
{
	// The stream belonging to a stream key does not change, so there is
	// no need to spend API quota on looking it up again
	static std::mutex mutex;
	static std::unordered_map<std::string, std::string> cache;

	const auto cacheKey = connection.GetChannelID() + "/" + streamKey;
	{
		std::lock_guard<std::mutex> lock(mutex);
		const auto it = cache.find(cacheKey);
		if (it != cache.end()) {
			return it->second;
		}
	}

	const auto streamId = requestStreamId(connection, streamKey);
	if (streamId.empty()) {
		return "";
	}

	std::lock_guard<std::mutex> lock(mutex);
	cache[cacheKey] = streamId;
	return streamId;
}

static std::optional<YouTubeBroadcastInfo>
getBroadcastBoundToObsStreamKey(YouTubeConnection &connection)
{
	const auto streamKey = getObsStreamKey();
	if (streamKey.empty()) {
		vblog(LOG_INFO, "no OBS stream key set");
		return {};
	}

	const auto streamId = getStreamId(connection, streamKey);
	if (streamId.empty()) {
		vblog(LOG_INFO,
		      "OBS stream key does not belong to YouTube channel \"%s\"",
		      connection.Name().c_str());
		return {};
	}

	for (const auto status : {"active", "upcoming"}) {
		const auto broadcasts =
			listBroadcasts(connection, {{"broadcastStatus", status},
						    {"broadcastType", "all"}});
		for (const auto &broadcast : broadcasts) {
			if (broadcast.boundStreamId == streamId) {
				return broadcast;
			}
		}
	}
	return {};
}

static std::optional<YouTubeBroadcastInfo>
getNextUpcomingBroadcast(YouTubeConnection &connection)
{
	const auto broadcasts =
		listBroadcasts(connection, {{"broadcastStatus", "upcoming"},
					    {"broadcastType", "all"}});

	std::optional<YouTubeBroadcastInfo> next;
	for (const auto &broadcast : broadcasts) {
		// Persistent broadcasts have no scheduled start time
		if (broadcast.scheduledStartTime.empty()) {
			if (!next) {
				next = broadcast;
			}
			continue;
		}
		// Timestamps are ISO 8601 in UTC, so comparing strings is enough
		if (!next || next->scheduledStartTime.empty() ||
		    broadcast.scheduledStartTime < next->scheduledStartTime) {
			next = broadcast;
		}
	}
	return next;
}

static std::optional<YouTubeBroadcastInfo>
getBroadcastById(YouTubeConnection &connection, const std::string &id)
{
	if (id.empty()) {
		return {};
	}

	const auto broadcasts = listBroadcasts(connection, {{"id", id}});
	if (broadcasts.empty()) {
		return {};
	}
	return broadcasts.front();
}

void YouTubeBroadcast::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "broadcast");
	_type = static_cast<Type>(obs_data_get_int(data, "type"));
	_id.Load(data, "id");
}

void YouTubeBroadcast::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "type", static_cast<int>(_type));
	_id.Save(data, "id");
	obs_data_set_obj(obj, "broadcast", data);
}

void YouTubeBroadcast::ResolveVariables()
{
	_id.ResolveVariables();
}

std::optional<YouTubeBroadcastInfo>
YouTubeBroadcast::GetInfo(YouTubeConnection &connection) const
{
	switch (_type) {
	case Type::BOUND_TO_OBS_STREAM_KEY:
		return getBroadcastBoundToObsStreamKey(connection);
	case Type::NEXT_UPCOMING:
		return getNextUpcomingBroadcast(connection);
	case Type::ID:
		return getBroadcastById(connection, _id);
	}
	return {};
}

static const std::unordered_map<YouTubeBroadcast::Type, const char *> types{
	{YouTubeBroadcast::Type::BOUND_TO_OBS_STREAM_KEY,
	 "AdvSceneSwitcher.youtube.broadcast.type.boundToObsStreamKey"},
	{YouTubeBroadcast::Type::NEXT_UPCOMING,
	 "AdvSceneSwitcher.youtube.broadcast.type.nextUpcoming"},
	{YouTubeBroadcast::Type::ID,
	 "AdvSceneSwitcher.youtube.broadcast.type.id"},
};

YouTubeBroadcastSelection::YouTubeBroadcastSelection(QWidget *parent)
	: QWidget(parent),
	  _type(new QComboBox(this)),
	  _id(new VariableLineEdit(this))
{
	for (const auto type : {YouTubeBroadcast::Type::BOUND_TO_OBS_STREAM_KEY,
				YouTubeBroadcast::Type::NEXT_UPCOMING,
				YouTubeBroadcast::Type::ID}) {
		_type->addItem(obs_module_text(types.at(type)),
			       static_cast<int>(type));
	}

	QWidget::connect(_type, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_id, SIGNAL(editingFinished()), this,
			 SLOT(IdChanged()));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_type);
	layout->addWidget(_id);
	setLayout(layout);

	SetWidgetVisibility();
}

void YouTubeBroadcastSelection::SetBroadcast(const YouTubeBroadcast &broadcast)
{
	_broadcast = broadcast;
	_type->setCurrentIndex(
		_type->findData(static_cast<int>(broadcast._type)));
	_id->setText(broadcast._id);
	SetWidgetVisibility();
}

void YouTubeBroadcastSelection::TypeChanged(int index)
{
	_broadcast._type = static_cast<YouTubeBroadcast::Type>(
		_type->itemData(index).toInt());
	SetWidgetVisibility();
	emit BroadcastChanged(_broadcast);
}

void YouTubeBroadcastSelection::IdChanged()
{
	_broadcast._id = _id->text().toStdString();
	emit BroadcastChanged(_broadcast);
}

void YouTubeBroadcastSelection::SetWidgetVisibility()
{
	_id->setVisible(_broadcast._type == YouTubeBroadcast::Type::ID);
}

} // namespace advss
