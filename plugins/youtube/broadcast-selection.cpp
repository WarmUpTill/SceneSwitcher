#include "broadcast-selection.hpp"
#include "youtube-connection.hpp"
#include "youtube-helpers.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <obs-module-helper.hpp>

#include <obs-frontend-api.h>
#include <mutex>
#include <QDateTime>
#include <QLabel>
#include <QLocale>
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
listBroadcasts(YouTubeConnection &connection, httplib::Params params,
	       bool useCache)
{
	params.emplace("part", broadcastParts);
	params.emplace("maxResults", "50");
	const auto result =
		SendYouTubeRequest(connection, YouTubeRequestMethod::GET,
				   "/liveBroadcasts", params, "", useCache);
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
				   const std::string &streamKey, bool useCache)
{
	const auto result = SendYouTubeRequest(
		connection, YouTubeRequestMethod::GET, "/liveStreams",
		{{"part", "cdn"}, {"mine", "true"}, {"maxResults", "50"}}, "",
		useCache);
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
			       const std::string &streamKey, bool useCache)
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

	const auto streamId = requestStreamId(connection, streamKey, useCache);
	if (streamId.empty()) {
		return "";
	}

	std::lock_guard<std::mutex> lock(mutex);
	cache[cacheKey] = streamId;
	return streamId;
}

static std::optional<YouTubeBroadcastInfo>
getBroadcastBoundToObsStreamKey(YouTubeConnection &connection, bool useCache)
{
	const auto streamKey = getObsStreamKey();
	if (streamKey.empty()) {
		vblog(LOG_INFO, "no OBS stream key set");
		return {};
	}

	const auto streamId = getStreamId(connection, streamKey, useCache);
	if (streamId.empty()) {
		vblog(LOG_INFO,
		      "OBS stream key does not belong to YouTube channel \"%s\"",
		      connection.Name().c_str());
		return {};
	}

	for (const auto status : {"active", "upcoming"}) {
		const auto broadcasts = listBroadcasts(
			connection,
			{{"broadcastStatus", status}, {"broadcastType", "all"}},
			useCache);
		for (const auto &broadcast : broadcasts) {
			if (broadcast.boundStreamId == streamId) {
				return broadcast;
			}
		}
	}
	return {};
}

static std::optional<YouTubeBroadcastInfo>
getBroadcastById(YouTubeConnection &connection, const std::string &id,
		 bool useCache)
{
	if (id.empty()) {
		return {};
	}

	const auto broadcasts =
		listBroadcasts(connection, {{"id", id}}, useCache);
	if (broadcasts.empty()) {
		return {};
	}
	return broadcasts.front();
}

void YouTubeBroadcast::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "broadcast");
	const auto type = static_cast<Type>(obs_data_get_int(data, "type"));
	_type = (type == Type::ID || type == Type::SELECTED)
			? type
			: Type::BOUND_TO_OBS_STREAM_KEY;
	_id.Load(data, "id");
	_selectedId = obs_data_get_string(data, "selectedId");
	_selectedTitle = obs_data_get_string(data, "selectedTitle");
}

void YouTubeBroadcast::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "type", static_cast<int>(_type));
	_id.Save(data, "id");
	obs_data_set_string(data, "selectedId", _selectedId.c_str());
	obs_data_set_string(data, "selectedTitle", _selectedTitle.c_str());
	obs_data_set_obj(obj, "broadcast", data);
}

void YouTubeBroadcast::ResolveVariables()
{
	_id.ResolveVariables();
}

std::optional<YouTubeBroadcastInfo>
YouTubeBroadcast::GetInfo(YouTubeConnection &connection, bool useCache) const
{
	switch (_type) {
	case Type::BOUND_TO_OBS_STREAM_KEY:
		return getBroadcastBoundToObsStreamKey(connection, useCache);
	case Type::ID:
		return getBroadcastById(connection, _id, useCache);
	case Type::SELECTED:
		return getBroadcastById(connection, _selectedId, useCache);
	}
	return {};
}

void BroadcastComboBox::showPopup()
{
	emit AboutToShowPopup();
	QComboBox::showPopup();
}

static const int typeRole = Qt::UserRole;
static const int idRole = Qt::UserRole + 1;
static const int titleRole = Qt::UserRole + 2;

static const int selectionRow = 0;
static const int idRow = 1;

static bool isLive(const std::string &lifeCycleStatus)
{
	return lifeCycleStatus == "live" || lifeCycleStatus == "liveStarting";
}

static bool isComplete(const std::string &lifeCycleStatus)
{
	return lifeCycleStatus == "complete" || lifeCycleStatus == "revoked";
}

static QString getStatusText(const std::string &lifeCycleStatus)
{
	if (isLive(lifeCycleStatus)) {
		return obs_module_text(
			"AdvSceneSwitcher.youtube.broadcast.status.live");
	}
	if (isComplete(lifeCycleStatus)) {
		return obs_module_text(
			"AdvSceneSwitcher.youtube.broadcast.status.complete");
	}
	return obs_module_text(
		"AdvSceneSwitcher.youtube.broadcast.status.upcoming");
}

static QString getDisplayText(const YouTubeBroadcastInfo &broadcast)
{
	const auto title = QString::fromStdString(broadcast.title);
	const auto status = getStatusText(broadcast.lifeCycleStatus);
	const auto start = QDateTime::fromString(
		QString::fromStdString(broadcast.scheduledStartTime),
		Qt::ISODate);
	const bool isUpcoming = !isLive(broadcast.lifeCycleStatus) &&
				!isComplete(broadcast.lifeCycleStatus);
	if (!isUpcoming || !start.isValid()) {
		return QString(obs_module_text(
				       "AdvSceneSwitcher.youtube.broadcast.entry"))
			.arg(title, status);
	}

	return QString(obs_module_text(
			       "AdvSceneSwitcher.youtube.broadcast.entry.scheduled"))
		.arg(title, status,
		     QLocale().toString(start.toLocalTime(),
					QLocale::ShortFormat));
}

YouTubeBroadcastSelection::YouTubeBroadcastSelection(QWidget *parent)
	: QWidget(parent),
	  _selection(new BroadcastComboBox(this)),
	  _id(new VariableLineEdit(this)),
	  _layout(new QGridLayout())
{
	QWidget::connect(_selection, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));
	QWidget::connect(_selection, SIGNAL(AboutToShowPopup()), this,
			 SLOT(UpdateBroadcastList()));
	QWidget::connect(_id, SIGNAL(editingFinished()), this,
			 SLOT(IdChanged()));

	_layout->setContentsMargins(0, 0, 0, 0);
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.youtube.broadcast")),
			   selectionRow, 0);
	_layout->addWidget(_selection, selectionRow, 1);
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.youtube.broadcast.id")),
			   idRow, 0);
	_layout->addWidget(_id, idRow, 1);
	_layout->setColumnStretch(2, 1);
	setLayout(_layout);

	PopulateSelection({});
	SetWidgetVisibility();
}

void YouTubeBroadcastSelection::SetBroadcast(const YouTubeBroadcast &broadcast)
{
	_broadcast = broadcast;
	PopulateSelection({});
	_id->setText(broadcast._id);
	SetWidgetVisibility();
}

void YouTubeBroadcastSelection::SetConnection(
	const std::weak_ptr<YouTubeConnection> &connection)
{
	_connection = connection;
}

void YouTubeBroadcastSelection::AddSelectionItem(const QString &text,
						 YouTubeBroadcast::Type type,
						 const std::string &id,
						 const std::string &title)
{
	_selection->addItem(text);
	const int index = _selection->count() - 1;
	_selection->setItemData(index, static_cast<int>(type), typeRole);
	_selection->setItemData(index, QString::fromStdString(id), idRole);
	_selection->setItemData(index, QString::fromStdString(title),
				titleRole);
}

void YouTubeBroadcastSelection::PopulateSelection(
	const std::vector<YouTubeBroadcastInfo> &broadcasts)
{
	const QSignalBlocker blocker(_selection);
	_selection->clear();

	AddSelectionItem(
		obs_module_text(
			"AdvSceneSwitcher.youtube.broadcast.type.boundToObsStreamKey"),
		YouTubeBroadcast::Type::BOUND_TO_OBS_STREAM_KEY);
	AddSelectionItem(
		obs_module_text("AdvSceneSwitcher.youtube.broadcast.type.id"),
		YouTubeBroadcast::Type::ID);
	_selection->insertSeparator(_selection->count());

	bool selectedBroadcastListed = false;
	for (const auto &broadcast : broadcasts) {
		AddSelectionItem(getDisplayText(broadcast),
				 YouTubeBroadcast::Type::SELECTED, broadcast.id,
				 broadcast.title);
		selectedBroadcastListed |= broadcast.id ==
					   _broadcast._selectedId;
	}

	// Keep showing the selection if the list was not fetched yet or the
	// broadcast is no longer part of it
	if (_broadcast._type == YouTubeBroadcast::Type::SELECTED &&
	    !selectedBroadcastListed) {
		AddSelectionItem(
			QString::fromStdString(_broadcast._selectedTitle),
			YouTubeBroadcast::Type::SELECTED,
			_broadcast._selectedId, _broadcast._selectedTitle);
	}

	for (int i = 0; i < _selection->count(); i++) {
		const auto type = static_cast<YouTubeBroadcast::Type>(
			_selection->itemData(i, typeRole).toInt());
		if (type != _broadcast._type) {
			continue;
		}
		if (type == YouTubeBroadcast::Type::SELECTED &&
		    _selection->itemData(i, idRole).toString().toStdString() !=
			    _broadcast._selectedId) {
			continue;
		}
		_selection->setCurrentIndex(i);
		break;
	}
}

void YouTubeBroadcastSelection::UpdateBroadcastList()
{
	auto connection = _connection.lock();
	if (!connection) {
		PopulateSelection({});
		return;
	}

	PopulateSelection(listBroadcasts(
		*connection,
		{{"broadcastStatus", "all"}, {"broadcastType", "all"}}, true));
}

void YouTubeBroadcastSelection::SelectionChanged(int index)
{
	_broadcast._type = static_cast<YouTubeBroadcast::Type>(
		_selection->itemData(index, typeRole).toInt());
	if (_broadcast._type == YouTubeBroadcast::Type::SELECTED) {
		_broadcast._selectedId = _selection->itemData(index, idRole)
						 .toString()
						 .toStdString();
		_broadcast._selectedTitle =
			_selection->itemData(index, titleRole)
				.toString()
				.toStdString();
	}
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
	SetGridLayoutRowVisible(_layout, idRow,
				_broadcast._type == YouTubeBroadcast::Type::ID);
	adjustSize();
	updateGeometry();
}

} // namespace advss
