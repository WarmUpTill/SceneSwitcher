#include "websocket-helpers.hpp"
#include "connection-manager.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "websocket-api.hpp"

#include <QCryptographicHash>
#include <obs-websocket-api.h>

namespace advss {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#define RPC_VERSION 1
#undef DispatchMessage

static constexpr char VendorRequestMessage[] = "AdvancedSceneSwitcherMessage";
static constexpr char VendorEventMessage[] = "AdvancedSceneSwitcherEvent";

static WebsocketMessageDispatcher websocketMessageDispatcher;

static bool setup();
static bool setupDone = setup();
static void receiveWebsocketMessage(obs_data_t *request_data, obs_data_t *);

bool setup()
{
	RegisterWebsocketRequest(VendorRequestMessage, receiveWebsocketMessage);
	return true;
}

WebsocketMessageBuffer RegisterForWebsocketMessages()
{
	return websocketMessageDispatcher.RegisterClient();
}

void SendWebsocketEvent(const std::string &eventMsg)
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "message", eventMsg.c_str());
	SendWebsocketVendorEvent(VendorEventMessage, data);
}

static void receiveWebsocketMessage(obs_data_t *request_data, obs_data_t *)
{
	if (!obs_data_has_user_value(request_data, "message")) {
		vblog(LOG_INFO,
		      "received unexpected AdvancedSceneSwitcherMessage: '%s'",
		      obs_data_get_json(request_data));
		return;
	}

	auto msg = obs_data_get_string(request_data, "message");
	websocketMessageDispatcher.DispatchMessage(msg);
	vblog(LOG_INFO, "received message: %s", msg);
}

WSClientConnection::WSClientConnection(bool useOBSProtocol) : QObject(nullptr)
{
	_client.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	_client.init_asio();
#ifndef _WIN32
	_client.set_reuse_addr(true);
#endif

	UseOBSWebsocketProtocol(useOBSProtocol);
	_client.set_close_handler(bind(&WSClientConnection::OnClose, this, _1));
}

WSClientConnection::~WSClientConnection()
{
	Disconnect();
}

void WSClientConnection::ConnectThread()
{
	do {
		std::unique_lock<std::mutex> lck(_waitMtx);
		_client.reset();
		_status = Status::CONNECTING;
		// Create a connection to the given URI and queue it for connection once
		// the event loop starts
		websocketpp::lib::error_code ec;
		client::connection_ptr con = _client.get_connection(_uri, ec);
		if (ec) {
			_failMsg = ec.message();
			blog(LOG_INFO, "connect to '%s' failed: %s",
			     _uri.c_str(), _failMsg.c_str());
		} else {
			_failMsg = "";
			_client.connect(con);
			_connection = connection_hdl(con);

			// Start the ASIO io_service run loop
			vblog(LOG_INFO, "connect io thread started for '%s'",
			      _uri.c_str());
			_client.run();
			vblog(LOG_INFO, "connect: io thread exited '%s'",
			      _uri.c_str());
		}

		if (_reconnect) {
			blog(LOG_INFO,
			     "trying to reconnect to %s in %d seconds.",
			     _uri.c_str(), _reconnectDelay);
			_cv.wait_for(lck,
				     std::chrono::seconds(_reconnectDelay));
		}
	} while (_reconnect && !_disconnect);
	_status = Status::DISCONNECTED;
}

void WSClientConnection::Connect(const std::string &uri,
				 const std::string &pass, bool reconnect,
				 int reconnectDelay)
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	if (_status != Status::DISCONNECTED) {
		blog(LOG_INFO, "connect to '%s' already in progress",
		     uri.c_str());
		return;
	}
	_uri = uri;
	_password = pass;
	_reconnect = reconnect;
	_reconnectDelay = reconnectDelay;
	_disconnect = false;
	if (_thread.joinable()) {
		_thread.join();
	}
	_thread = std::thread(&WSClientConnection::ConnectThread, this);
	blog(LOG_INFO, "connect to '%s' started", uri.c_str());
}

void WSClientConnection::Disconnect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	_disconnect = true;
	websocketpp::lib::error_code ec;
	_client.close(_connection, websocketpp::close::status::normal,
		      "Client stopping", ec);
	{
		std::unique_lock<std::mutex> waitLck(_waitMtx);
		_cv.notify_all();
	}

	while (_status != Status::DISCONNECTED) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		_client.close(_connection, websocketpp::close::status::normal,
			      "Client stopping", ec);
	}

	if (_thread.joinable()) {
		_thread.join();
	}
	_status = Status::DISCONNECTED;
}

std::string ConstructVendorRequestMessage(const std::string &message)
{
	auto request = obs_data_create();
	obs_data_set_int(request, "op", 6);
	auto *data = obs_data_create();
	obs_data_set_string(data, "requestType", "CallVendorRequest");
	obs_data_set_string(data, "requestId", message.c_str());

	auto vendorData = obs_data_create();
	obs_data_set_string(vendorData, "vendorName", GetWebsocketVendorName());
	obs_data_set_string(vendorData, "requestType", VendorRequestMessage);

	auto msgObj = obs_data_create();
	obs_data_set_string(msgObj, "message", message.c_str());
	obs_data_set_obj(vendorData, "requestData", msgObj);
	obs_data_set_obj(data, "requestData", vendorData);

	obs_data_set_obj(request, "d", data);

	const std::string result(obs_data_get_json(request));

	obs_data_release(msgObj);
	obs_data_release(vendorData);
	obs_data_release(data);
	obs_data_release(request);

	return result;
}

void WSClientConnection::SendRequest(const std::string &msg)
{
	Send(msg);
}

WebsocketMessageBuffer WSClientConnection::RegisterForEvents()
{
	return _dispatcher.RegisterClient();
}

WSClientConnection::Status WSClientConnection::GetStatus() const
{
	return _status;
}

void WSClientConnection::UseOBSWebsocketProtocol(bool useOBSProtocol)
{
	_client.set_open_handler(
		bind(useOBSProtocol ? &WSClientConnection::OnOBSOpen
				    : &WSClientConnection::OnGenericOpen,
		     this, _1));
	_client.set_message_handler(
		bind(useOBSProtocol ? &WSClientConnection::OnOBSMessage
				    : &WSClientConnection::OnGenericMessage,
		     this, _1, _2));
}

void WSClientConnection::OnGenericOpen(connection_hdl)
{
	blog(LOG_INFO, "connection to %s opened", _uri.c_str());
	_status = Status::AUTHENTICATED;
}

void WSClientConnection::OnOBSOpen(connection_hdl)
{
	blog(LOG_INFO, "connection to %s opened", _uri.c_str());
	_status = Status::CONNECTING;
}

void WSClientConnection::HandleHello(obs_data_t *helloMsg)
{
	_status = Status::CONNECTED;

	auto identifyMsg = obs_data_create();
	obs_data_set_int(identifyMsg, "op", 1);
	auto *data = obs_data_create();
	obs_data_set_int(data, "rpcVersion", RPC_VERSION);
	// We are only interested in EventSubscription::Vendors (1 << 9)
	obs_data_set_int(data, "eventSubscriptions", 1 << 9);
	obs_data_t *helloData = obs_data_get_obj(helloMsg, "d");
	if (obs_data_has_user_value(helloData, "authentication")) {
		auto auth = obs_data_get_obj(helloData, "authentication");
		QString salt = obs_data_get_string(auth, "salt");
		QString challenge = obs_data_get_string(auth, "challenge");
		auto secret = QCryptographicHash::hash(
				      (QString::fromStdString(_password) + salt)
					      .toUtf8(),
				      QCryptographicHash::Sha256)
				      .toBase64();
		auto authenticationString = QString(
			QCryptographicHash::hash((secret + challenge).toUtf8(),
						 QCryptographicHash::Sha256)
				.toBase64());
		obs_data_set_string(data, "authentication",
				    authenticationString.toStdString().c_str());
		obs_data_release(auth);
	}
	obs_data_release(helloData);
	obs_data_set_obj(identifyMsg, "d", data);
	const std::string response(obs_data_get_json(identifyMsg));
	obs_data_release(data);
	obs_data_release(identifyMsg);
	Send(response);
}

void WSClientConnection::HandleEvent(obs_data_t *msg)
{
	auto d = obs_data_get_obj(msg, "d");
	auto eventData = obs_data_get_obj(d, "eventData");
	if (strcmp(obs_data_get_string(eventData, "vendorName"),
		   GetWebsocketVendorName()) != 0) {
		vblog(LOG_INFO, "ignoring vendor event from \"%s\"",
		      obs_data_get_string(eventData, "vendorName"));
		return;
	}
	if (strcmp(obs_data_get_string(eventData, "eventType"),
		   VendorEventMessage) != 0) {
		vblog(LOG_INFO, "ignoring event type\"%s\"",
		      obs_data_get_string(eventData, "eventType"));
		return;
	}
	auto eventDataNested = obs_data_get_obj(eventData, "eventData");
	_dispatcher.DispatchMessage(
		obs_data_get_string(eventDataNested, "message"));
	vblog(LOG_INFO, "received event msg \"%s\"",
	      obs_data_get_string(eventDataNested, "message"));
	obs_data_release(eventDataNested);
	obs_data_release(eventData);
	obs_data_release(d);
}

void WSClientConnection::HandleResponse(obs_data_t *response)
{
	auto data = obs_data_get_obj(response, "d");
	auto id = obs_data_get_string(data, "requestId");
	auto status = obs_data_get_obj(data, "requestStatus");
	bool result = obs_data_get_bool(status, "result");
	int code = obs_data_get_int(status, "code");
	auto comment = obs_data_get_string(status, "comment");
	vblog(LOG_INFO, "received result '%d' with code '%d' (%s) for id '%s'",
	      result, code, comment, id);
	obs_data_release(status);
	obs_data_release(data);
}

void WSClientConnection::OnGenericMessage(connection_hdl,
					  client::message_ptr message)
{
	if (!message) {
		return;
	}
	if (message->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}

	const auto payload = message->get_payload();
	_dispatcher.DispatchMessage(payload);
	vblog(LOG_INFO, "received event msg \"%s\"", payload.c_str());
}

void WSClientConnection::OnOBSMessage(connection_hdl,
				      client::message_ptr message)
{
	if (!message) {
		return;
	}
	if (message->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}

	std::string payload = message->get_payload();
	const char *msg = payload.c_str();
	auto json = obs_data_create_from_json(msg);
	if (!json) {
		blog(LOG_ERROR, "invalid JSON payload received for '%s'", msg);
		obs_data_release(json);
		return;
	}

	if (!obs_data_has_user_value(json, "op")) {
		blog(LOG_ERROR, "received msg has no opcode, '%s'", msg);
		obs_data_release(json);
		return;
	}

	int opcode = obs_data_get_int(json, "op");
	switch (opcode) {
	case 0: // Hello
		HandleHello(json);
		break;
	case 2: // Identified
		_status = Status::AUTHENTICATED;
		break;
	case 5: // Event (Vendor)
		HandleEvent(json);
		break;
	case 7: // RequestResponse
		HandleResponse(json);
		break;
	default:
		vblog(LOG_INFO, "ignoring unknown opcode %d", opcode);
		break;
	}
	obs_data_release(json);
}

void WSClientConnection::Send(const std::string &msg)
{
	if (_connection.expired()) {
		return;
	}
	websocketpp::lib::error_code errorCode;
	_client.send(_connection, msg, websocketpp::frame::opcode::text,
		     errorCode);
	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "websocket send failed: %s",
		     errorCodeMessage.c_str());
	}
	vblog(LOG_INFO, "sent message to '%s':\n%s", _uri.c_str(), msg.c_str());
}

void WSClientConnection::OnClose(connection_hdl)
{
	blog(LOG_INFO, "client-connection to %s closed.", _uri.c_str());
	_status = Status::DISCONNECTED;
}

} // namespace advss
