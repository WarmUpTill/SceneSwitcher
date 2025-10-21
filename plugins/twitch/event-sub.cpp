#include "event-sub.hpp"
#include "token.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>

#ifdef VERIFY_TIMESTAMPS
#include "date/tz.h"
#endif

namespace advss {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#ifdef USE_TWITCH_CLI_MOCK
static constexpr std::string_view defaultURL = "ws://127.0.0.1:8080/ws";
static constexpr std::string_view registerSubscriptionURL =
	"http://127.0.0.1:8080";
static constexpr std::string_view registerSubscriptionPath =
	"/eventsub/subscriptions";
#else
static constexpr std::string_view defaultURL =
	"wss://eventsub.wss.twitch.tv/ws";
static constexpr std::string_view registerSubscriptionURL =
	"https://api.twitch.tv";
static constexpr std::string_view registerSubscriptionPath =
	"/helix/eventsub/subscriptions";
#endif
static const int reconnectDelay = 15;

#undef DispatchMessage

EventSub::EventSub()
	: QObject(nullptr),
	  _client(std::make_unique<EventSubWSClient>())
{
	SetupClient(*_client);

	_client->set_open_handler(bind(&EventSub::OnOpen, this, _1));
	_client->set_message_handler(bind(&EventSub::OnMessage, this, _1, _2));
	_client->set_close_handler(bind(&EventSub::OnClose, this, _1));
	_client->set_fail_handler(bind(&EventSub::OnFail, this, _1));

	_url = defaultURL.data();
	RegisterInstance();
}

EventSub::~EventSub()
{
	Disconnect();
	UnregisterInstance();
}

std::mutex EventSub::_instancesMtx;
std::vector<EventSub *> EventSub::_instances;

void EventSub::RegisterInstance()
{
	std::lock_guard<std::mutex> lock(_instancesMtx);
	_instances.emplace_back(this);
}

void EventSub::UnregisterInstance()
{
	std::lock_guard<std::mutex> lock(_instancesMtx);
	auto it = std::remove(_instances.begin(), _instances.end(), this);
	_instances.erase(it, _instances.end());
}

void EventSub::ConnectThread()
{
	_client->reset();
	_connected = true;
	websocketpp::lib::error_code ec;
	EventSubWSClient::connection_ptr con =
		_client->get_connection(_url, ec);
	if (ec) {
		blog(LOG_INFO, "Twitch EventSub failed: %s",
		     ec.message().c_str());
	} else {
		_client->connect(con);
		_connection = connection_hdl(con);
		_client->run();
	}

	_connected = false;
}

void EventSub::WaitAndReconnect()
{
	auto thread = std::thread([this]() {
		std::unique_lock<std::mutex> lock(_waitMtx);
		blog(LOG_INFO,
		     "Twitch EventSub trying to reconnect to in %d seconds.",
		     reconnectDelay);
		_cv.wait_for(lock, std::chrono::seconds(reconnectDelay));
		Connect();
	});
	thread.detach();
}

void EventSub::Connect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	if (_connected) {
		vblog(LOG_INFO, "Twitch EventSub connect already in progress");
		return;
	}
	_disconnect = true;
	if (_thread.joinable()) {
		_thread.join();
	}
	_disconnect = false;
	_thread = std::thread(&EventSub::ConnectThread, this);
}

void EventSub::ClearActiveSubscriptions()
{
	std::lock_guard<std::mutex> lock(_subscriptionMtx);
	_activeSubscriptions.clear();
}

void EventSub::EnableTimestampValidation(bool enable)
{
	_validateTimestamps = enable;
}

void EventSub::Disconnect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	_disconnect = true;
	websocketpp::lib::error_code ec;
	_client->close(_connection, websocketpp::close::status::normal,
		       "Twitch EventSub stopping", ec);
	{
		std::unique_lock<std::mutex> waitLock(_waitMtx);
		_cv.notify_all();
	}

	while (_connected) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		_client->close(_connection, websocketpp::close::status::normal,
			       "Twitch EventSub stopping", ec);
	}

	if (_thread.joinable()) {
		_thread.join();
	}
	_connected = false;
	ClearActiveSubscriptions();
}

EventSubMessageBuffer EventSub::RegisterForEvents()
{
	return _dispatcher.RegisterClient();
}

bool EventSub::SubscriptionIsActive(const std::string &id)
{
	std::lock_guard<std::mutex> lock(_subscriptionMtx);
	for (const auto &subscription : _activeSubscriptions) {
		if (subscription.id == id) {
			return true;
		}
	}
	return false;
}

static bool isAlreadySubscribed(const std::set<Subscription> &subscriptions,
				const Subscription &newSubsciption)
{
	return subscriptions.find(newSubsciption) != subscriptions.end();
}

static void setTransportData(const OBSData &data, const std::string &sessionID)
{
	OBSDataAutoRelease transport = obs_data_create();
	obs_data_set_string(transport, "method", "websocket");
	obs_data_set_string(transport, "session_id", sessionID.c_str());
	obs_data_set_obj(data, "transport", transport);
}

static obs_data_t *copyData(const OBSData &data)
{
	auto json = obs_data_get_json(data);
	if (!json) {
		return nullptr;
	}
	return obs_data_create_from_json(json);
}

std::string EventSub::AddEventSubscription(std::shared_ptr<TwitchToken> token,
					   Subscription subscription)
{
	auto eventSub = token->GetEventSub();
	if (!eventSub) {
		blog(LOG_WARNING, "failed to get Twitch EventSub from token!");
		return "";
	}

	std::unique_lock<std::mutex> lock(eventSub->_subscriptionMtx);
	if (!eventSub->_connected) {
		vblog(LOG_INFO, "Twitch EventSub connect started for %s",
		      token->GetName().c_str());
		lock.unlock();
		eventSub->Connect();
		return "";
	}

	if (isAlreadySubscribed(eventSub->_activeSubscriptions, subscription)) {
		return eventSub->_activeSubscriptions.find(subscription)->id;
	}

	OBSDataAutoRelease postData = copyData(subscription.data);
	setTransportData(postData.Get(), eventSub->_sessionID);
	auto result = SendPostRequest(*token, registerSubscriptionURL.data(),
				      registerSubscriptionPath.data(), {},
				      postData.Get());

	if (result.status != 202) {
		vblog(LOG_INFO, "failed to register Twitch EventSub (%d)",
		      result.status);
		return "";
	}

	OBSDataArrayAutoRelease replyArray =
		obs_data_get_array(result.data, "data");
	OBSDataAutoRelease replyData = obs_data_array_item(replyArray, 0);
	subscription.id = obs_data_get_string(replyData, "id");
	eventSub->_activeSubscriptions.emplace(subscription);
	return subscription.id;
}

void EventSub::SetupClient(EventSubWSClient &client)
{
	client.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	client.init_asio();
#ifndef _WIN32
	client.set_reuse_addr(true);
#endif
#ifndef USE_TWITCH_CLI_MOCK
	client.set_tls_init_handler([](websocketpp::connection_hdl) {
		return websocketpp::lib::make_shared<asio::ssl::context>(
			asio::ssl::context::sslv23_client);
	});
#endif
}

void EventSub::OnOpen(connection_hdl)
{
	vblog(LOG_INFO, "Twitch EventSub connection opened");
	_connected = true;
}

static bool isValidTimestamp(const std::string &timestamp)
{
#ifdef VERIFY_TIMESTAMPS
	// Example input: 2023-07-19T14:56:51.634234626Z
	try {
		// Discard the nanosecond part
		static constexpr size_t dotPos = 19;
		std::string trimmed = timestamp.substr(0, dotPos);
		auto tzStart = timestamp.find_first_of("Z+-", dotPos);
		trimmed = timestamp.substr(0, dotPos);
		if (tzStart != std::string::npos) {
			trimmed += timestamp.substr(tzStart);
		}

		std::istringstream in(trimmed);
		date::sys_time<std::chrono::seconds> parsedTime;
		in >> date::parse("%FT%TZ", parsedTime);
		if (in.fail()) {
			blog(LOG_WARNING, "failed to parse timestamp %s",
			     timestamp.c_str());
			return false;
		}

		auto now = date::zoned_time{date::current_zone(),
					    std::chrono::system_clock::now()}
				   .get_sys_time();

		auto duration = now - parsedTime;
		// Clocks might be off by a bit, so allow negative values also
		return duration <= std::chrono::minutes(10) &&
		       duration >= std::chrono::minutes(-1);
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "%s: %s", __func__, e.what());
		return false;
	}
#else
	// Just assume timestamps are always valid
	return true;
#endif
}

bool EventSub::IsValidMessageID(const std::string &id)
{
	auto it = std::find(_messageIDs.begin(), _messageIDs.end(), id);
	if (it != _messageIDs.end()) {
		return false;
	}
	if (!_messageIDs.empty()) {
		_messageIDs.pop_front();
	}
	_messageIDs.push_back(id);
	return true;
}

bool EventSub::IsValidID(const std::string &id)
{
	return !_sessionID.empty() && id == _sessionID;
}

std::optional<const EventSub::ParsedMessage>
EventSub::ParseWebSocketMessage(const EventSubWSClient::message_ptr &message)
{
	if (!message) {
		return {};
	}
	if (message->get_opcode() != websocketpp::frame::opcode::text) {
		return {};
	}

	std::string payload = message->get_payload();
	OBSDataAutoRelease json = obs_data_create_from_json(payload.c_str());
	if (!json) {
		blog(LOG_ERROR, "invalid JSON payload received for '%s'",
		     payload.c_str());
		return {};
	}

	OBSDataAutoRelease metadata = obs_data_get_obj(json, "metadata");
	std::string timestamp =
		obs_data_get_string(metadata, "message_timestamp");
	if (_validateTimestamps && !isValidTimestamp(timestamp)) {
		blog(LOG_WARNING,
		     "discarding Twitch EventSub with invalid timestamp %s",
		     timestamp.c_str());
		return {};
	}
	std::string id = obs_data_get_string(metadata, "message_id");
	if (!IsValidMessageID(id)) {
		blog(LOG_WARNING,
		     "discarding Twitch EventSub with invalid message_id");
		return {};
	}

	ParsedMessage parsedMessage{obs_data_get_string(metadata,
							"message_type"),
				    obs_data_get_obj(json, "payload")};
	return parsedMessage;
}

void EventSub::OnMessage(connection_hdl, EventSubWSClient::message_ptr message)
{
	const auto msg = ParseWebSocketMessage(message);
	if (!msg) {
		return;
	}

	const auto &type = msg->type;
	const auto &data = msg->payload;
	if (type == "session_welcome") {
		HandleWelcome(data);
	} else if (type == "session_keepalive") {
		HandleKeepAlive();
	} else if (type == "notification") {
		HandleNotification(data);
	} else if (type == "session_reconnect") {
		HandleServerMigration(data);
	} else if (type == "revocation") {
		HandleRevocation(data);
	} else {
		vblog(LOG_INFO,
		      "Twitch EventSub ignoring message of unknown type '%s'",
		      type.c_str());
	}
}

void EventSub::HandleWelcome(obs_data_t *data)
{
	OBSDataAutoRelease session = obs_data_get_obj(data, "session");
	_sessionID = obs_data_get_string(session, "id");
	blog(LOG_INFO, "Twitch EventSub connected");
}

void EventSub::HandleKeepAlive() const
{
	// Nothing to do
}

void EventSub::HandleNotification(obs_data_t *data)
{
	Event event;
	OBSDataAutoRelease subscription =
		obs_data_get_obj(data, "subscription");
	event.id = obs_data_get_string(subscription, "id");
	event.type = obs_data_get_string(subscription, "type");
	OBSDataAutoRelease eventData = obs_data_get_obj(data, "event");
	event.data = eventData;
	_dispatcher.DispatchMessage(event);
}

void EventSub::OnServerMigrationWelcome(
	connection_hdl newHdl, std::unique_ptr<EventSubWSClient> &newClient)
{
	std::lock_guard<std::mutex> lock(_connectMtx);

	// Disable reconnect handling for old connection which will be closed
	_client->set_close_handler([](connection_hdl) {});
	_client->set_fail_handler([](connection_hdl) {});
	auto connection = _client->get_con_from_hdl(_connection);
	connection->set_close_handler([](connection_hdl) {
		vblog(LOG_INFO, "previous Twitch EventSub connection closed");
	});
	connection->set_fail_handler([](connection_hdl) {});

	websocketpp::lib::error_code ec;
	_client->close(_connection, websocketpp::close::status::normal,
		       "Switching to new connection", ec);

	_client.swap(newClient);
	_sessionID = _migrationSessionID;
	_connection = newHdl;

	_client->set_open_handler(bind(&EventSub::OnOpen, this, _1));
	_client->set_message_handler(bind(&EventSub::OnMessage, this, _1, _2));
	_client->set_close_handler(bind(&EventSub::OnClose, this, _1));
	_client->set_fail_handler(bind(&EventSub::OnFail, this, _1));
	auto newConnection = _client->get_con_from_hdl(_connection);
	newConnection->set_open_handler(bind(&EventSub::OnOpen, this, _1));
	newConnection->set_message_handler(
		bind(&EventSub::OnMessage, this, _1, _2));
	newConnection->set_close_handler(bind(&EventSub::OnClose, this, _1));
	newConnection->set_fail_handler(bind(&EventSub::OnFail, this, _1));

	_connected = true;
	_migrating = false;
}

void EventSub::StartServerMigrationClient(const std::string &url)
{
	auto client = std::make_unique<EventSubWSClient>();
	SetupClient(*client);

	client->set_open_handler([this](connection_hdl hdl) {
		vblog(LOG_INFO, "Twitch EventSub migration client opened");
	});

	client->set_message_handler([this,
				     &client](connection_hdl hdl,
					      EventSubWSClient::message_ptr
						      message) {
		const auto msg = ParseWebSocketMessage(message);
		if (!msg) {
			return;
		}

		const auto &type = msg->type;
		const auto &data = msg->payload;

		if (type == "session_welcome") {
			vblog(LOG_INFO,
			      "Twitch EventSub migration successful - switching to new connection");
			OBSDataAutoRelease session =
				obs_data_get_obj(data, "session");
			_migrationSessionID =
				obs_data_get_string(session, "id");
			OnServerMigrationWelcome(hdl, client);
		} else {
			OnMessage(hdl, message);
		}
	});

	websocketpp::lib::error_code ec;
	auto con = client->get_connection(url, ec);
	if (ec) {
		blog(LOG_ERROR,
		     "Twitch EventSub migration connection failed: %s",
		     ec.message().c_str());
		_migrating = false;
		return;
	}

	_migrationConnection = con;
	client->connect(con);
	client->run();
}

void EventSub::HandleServerMigration(obs_data_t *data)
{
	blog(LOG_INFO, "Twitch EventSub session_reconnect received");

	OBSDataAutoRelease session = obs_data_get_obj(data, "session");
	auto id = obs_data_get_string(session, "id");
	if (!IsValidID(id)) {
		vblog(LOG_INFO,
		      "ignoring Twitch EventSub reconnect message with invalid id");
		return;
	}

	const std::string newURL =
		obs_data_get_string(session, "reconnect_url");
	if (newURL.empty()) {
		blog(LOG_WARNING, "missing reconnect_url in session_reconnect");
		return;
	}

	if (_migrating.exchange(true)) {
		vblog(LOG_INFO,
		      "ignoring Twitch EventSub session_reconnect - already in progress");
		return;
	}

	vblog(LOG_INFO, "Twitch EventSub reconnect: connecting to %s",
	      newURL.c_str());

	std::thread([this, newURL]() {
		StartServerMigrationClient(newURL);
	}).detach();
}

void EventSub::HandleRevocation(obs_data_t *data)
{
	OBSDataAutoRelease subscription =
		obs_data_get_obj(data, "subscription");
	auto id = obs_data_get_string(subscription, "id");
	auto status = obs_data_get_string(subscription, "status");
	auto type = obs_data_get_string(subscription, "type");
	auto version = obs_data_get_string(subscription, "version");
	OBSDataAutoRelease condition =
		obs_data_get_obj(subscription, "condition");
	auto conditionJson = obs_data_get_json(condition);
	blog(LOG_INFO,
	     "Twitch EventSub revoked:\n"
	     "id: %s\n"
	     "status: %s\n"
	     "type: %s\n"
	     "version: %s\n"
	     "condition: %s\n",
	     id, status, type, version, conditionJson ? conditionJson : "");

	std::lock_guard<std::mutex> lock(_subscriptionMtx);
	for (auto it = _activeSubscriptions.begin();
	     it != _activeSubscriptions.begin();) {
		if (it->id == id) {
			it = _activeSubscriptions.erase(it);
		} else {
			++it;
		}
	}
}

void EventSub::OnClose(connection_hdl hdl)
{
	EventSubWSClient::connection_ptr con = _client->get_con_from_hdl(hdl);
	const auto msg = con->get_ec().message();
	const auto reason = con->get_remote_close_reason();
	const auto code = con->get_remote_close_code();
	blog(LOG_INFO, "Twitch EventSub connection closed: %s / %s (%d)",
	     msg.c_str(), reason.c_str(), code);

	if (_migrating) {
		ClearActiveSubscriptions();
	}
	_connected = false;

	if (_disconnect) {
		return;
	}

	WaitAndReconnect();
}

void EventSub::OnFail(connection_hdl hdl)
{
	EventSubWSClient::connection_ptr con = _client->get_con_from_hdl(hdl);
	auto msg = con->get_ec().message();
	blog(LOG_INFO, "Twitch EventSub connection failed: %s", msg.c_str());

	if (!_migrating) {
		ClearActiveSubscriptions();
	}
	_connected = false;

	if (_disconnect) {
		return;
	}

	WaitAndReconnect();
}

bool Subscription::operator<(const Subscription &other) const
{
	auto json = obs_data_get_json(data);
	std::string jsonString = json ? json : "";
	auto otherJson = obs_data_get_json(other.data);
	std::string otherJsonString = otherJson ? otherJson : "";
	return jsonString < otherJsonString;
}

std::string Event::ToString() const
{
	auto json = obs_data_get_json(data);
	return json ? json : "";
}

} // namespace advss
