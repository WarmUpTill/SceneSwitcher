/*
Most of this code is based on https://github.com/Palakis/obs-websocket
*/

#include <QtWidgets/QMainWindow>
#include <QtConcurrent/QtConcurrent>
#include <QCryptographicHash>
#include <QTime>
#include <QMessageBox>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#define PARAM_SERVER_ENABLE "ServerEnabled"
#define PARAM_SERVER_PORT "ServerPort"
#define PARAM_LOCKTOIPV4 "LockToIPv4"

#define PARAM_CLIENT_ENABLE "ClientEnabled"
#define PARAM_CLIENT_PORT "ClientPort"
#define PARAM_ADDRESS "Address"
#define PARAM_CLIENT_SENDALL "SendAll"

#define RECONNECT_DELAY 5

#define SCENE_ENTRY "scene"
#define TRANSITION_ENTRY "transition"

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

NetworkConfig::NetworkConfig()
	: ServerEnabled(false),
	  ServerPort(55555),
	  LockToIPv4(false),
	  ClientEnabled(false),
	  Address(""),
	  ClientPort(55555),
	  SendAll(true)
{
}

void NetworkConfig::Load(obs_data_t *obj)
{
	SetDefaults(obj);

	ServerEnabled = obs_data_get_bool(obj, PARAM_SERVER_ENABLE);
	ServerPort = obs_data_get_int(obj, PARAM_SERVER_PORT);
	LockToIPv4 = obs_data_get_bool(obj, PARAM_LOCKTOIPV4);

	ClientEnabled = obs_data_get_bool(obj, PARAM_CLIENT_ENABLE);
	Address = obs_data_get_string(obj, PARAM_ADDRESS);
	ClientPort = obs_data_get_int(obj, PARAM_CLIENT_PORT);
	SendAll = obs_data_get_bool(obj, PARAM_CLIENT_SENDALL);
}

void NetworkConfig::Save(obs_data_t *obj)
{
	obs_data_set_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_bool(obj, PARAM_LOCKTOIPV4, LockToIPv4);

	obs_data_set_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_bool(obj, PARAM_CLIENT_SENDALL, SendAll);
}

void NetworkConfig::SetDefaults(obs_data_t *obj)
{
	obs_data_set_default_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_default_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_default_bool(obj, PARAM_LOCKTOIPV4, LockToIPv4);

	obs_data_set_default_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_default_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_default_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_default_bool(obj, PARAM_CLIENT_SENDALL, SendAll);
}

std::string NetworkConfig::GetClientUri()
{
	return "ws://" + Address + ":" + std::to_string(ClientPort);
}

WSServer::WSServer()
	: QObject(nullptr), _connections(), _clMutex(QMutex::Recursive)
{
	_server.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	_server.init_asio();
#ifndef _WIN32
	_server.set_reuse_addr(true);
#endif

	_server.set_open_handler(bind(&WSServer::onOpen, this, ::_1));
	_server.set_close_handler(bind(&WSServer::onClose, this, ::_1));
	_server.set_message_handler(
		bind(&WSServer::onMessage, this, ::_1, ::_2));
}

WSServer::~WSServer()
{
	stop();
}

void WSServer::start(quint16 port, bool lockToIPv4)
{
	if (_server.is_listening() &&
	    (port == _serverPort && _lockToIPv4 == lockToIPv4)) {
		blog(LOG_INFO,
		     "WSServer::start: server already on this port and protocol mode. no restart needed");
		return;
	}

	if (_server.is_listening()) {
		stop();
	}

	_server.reset();

	_serverPort = port;
	_lockToIPv4 = lockToIPv4;

	websocketpp::lib::error_code errorCode;
	if (lockToIPv4) {
		blog(LOG_INFO, "WSServer::start: Locked to IPv4 bindings");
		_server.listen(websocketpp::lib::asio::ip::tcp::v4(),
			       _serverPort, errorCode);
	} else {
		blog(LOG_INFO, "WSServer::start: Not locked to IPv4 bindings");
		_server.listen(_serverPort, errorCode);
	}

	// TODO adjust error Messagebox
	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "server: listen failed: %s",
		     errorCodeMessage.c_str());

		obs_frontend_push_ui_translation(obs_module_get_string);
		QString errorTitle =
			tr("OBSWebsocket.Server.StartFailed.Title");
		QString errorMessage =
			tr("OBSWebsocket.Server.StartFailed.Message")
				.arg(_serverPort)
				.arg(errorCodeMessage.c_str());
		obs_frontend_pop_ui_translation();

		QMainWindow *mainWindow = reinterpret_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		QMessageBox::warning(mainWindow, errorTitle, errorMessage);

		return;
	}

	_server.start_accept();

	QtConcurrent::run([=]() {
		blog(LOG_INFO, "WSServer::start: io thread started");
		_server.run();
		blog(LOG_INFO, "WSServer::start: io thread exited");
	});

	blog(LOG_INFO,
	     "WSServer::start: server started successfully on port %d",
	     _serverPort);
}

void WSServer::stop()
{
	if (!_server.is_listening()) {
		return;
	}

	_server.stop_listening();
	for (connection_hdl hdl : _connections) {
		websocketpp::lib::error_code ec;
		_server.close(hdl, websocketpp::close::status::going_away,
			      "Server stopping", ec);
	}

	_threadPool.waitForDone();

	while (_connections.size() > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	blog(LOG_INFO, "server stopped successfully");
}

void WSServer::onOpen(connection_hdl hdl)
{
	QMutexLocker locker(&_clMutex);
	_connections.insert(hdl);
	locker.unlock();

	QString clientIp = getRemoteEndpoint(hdl);
	blog(LOG_INFO, "new client connection from %s",
	     clientIp.toUtf8().constData());
}

std::string processMessage(std::string payload)
{
	auto config = switcher->networkConfig;
	std::string msgContainer(payload);
	const char *msg = msgContainer.c_str();

	OBSData data = obs_data_create_from_json(msg);
	if (!data) {
		blog(LOG_ERROR, "invalid JSON payload received for '%s'", msg);
		return "invalid JSON payload";
	}

	if (!obs_data_has_user_value(data, SCENE_ENTRY) ||
	    !obs_data_has_user_value(data, TRANSITION_ENTRY)) {
		return "missing request parameters";
	}

	std::string sceneName = obs_data_get_string(data, SCENE_ENTRY);
	std::string transitionName =
		obs_data_get_string(data, TRANSITION_ENTRY);

	obs_data_release(data);

	auto scene = GetWeakSourceByName(sceneName.c_str());
	if (!scene) {
		return "ignoring request - unknown scene '" + sceneName + "'";
	}

	std::string ret = "message ok";

	auto transition = GetWeakTransitionByName(transitionName.c_str());
	if (switcher->verbose && !transition) {
		ret += " - ignoring invalid transition: '" + transitionName +
		       "'";
	}

	switchScene(scene, transition, switcher->tansitionOverrideOverride);
	return ret;
}

void WSServer::onMessage(connection_hdl hdl, server::message_ptr message)
{
	auto opcode = message->get_opcode();
	if (opcode != websocketpp::frame::opcode::text) {
		return;
	}

	QtConcurrent::run(&_threadPool, [=]() {
		std::string payload = message->get_payload();
		std::string response = processMessage(payload);
		websocketpp::lib::error_code errorCode;
		_server.send(hdl, response, websocketpp::frame::opcode::text,
			     errorCode);

		if (errorCode) {
			std::string errorCodeMessage = errorCode.message();
			blog(LOG_INFO, "server(response): send failed: %s",
			     errorCodeMessage.c_str());
		}
	});
}

void WSServer::onClose(connection_hdl hdl)
{
	QMutexLocker locker(&_clMutex);
	_connections.erase(hdl);
	locker.unlock();

	auto conn = _server.get_con_from_hdl(hdl);
	auto localCloseCode = conn->get_local_close_code();

	if (localCloseCode != websocketpp::close::status::going_away) {
		QString clientIp = getRemoteEndpoint(hdl);
		blog(LOG_INFO, "client %s disconnected",
		     clientIp.toUtf8().constData());
	}
}

QString WSServer::getRemoteEndpoint(connection_hdl hdl)
{
	auto conn = _server.get_con_from_hdl(hdl);
	return QString::fromStdString(conn->get_remote_endpoint());
}

WSClient::WSClient() : QObject(nullptr)
{
	_client.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	_client.init_asio();
#ifndef _WIN32
	_client.set_reuse_addr(true);
#endif

	_client.set_open_handler(bind(&WSClient::onOpen, this, ::_1));
	_client.set_fail_handler(bind(&WSClient::onFail, this, ::_1));
	_client.set_message_handler(
		bind(&WSClient::onMessage, this, ::_1, ::_2));
	_client.set_close_handler(bind(&WSClient::onClose, this, ::_1));
}

WSClient::~WSClient()
{
	disconnect();
}

void WSClient::connect(std::string uri)
{
	disconnect();
	_client.reset();
	_uri = uri;
	_retry = true;

	_thread = std::thread([=]() {
		while (_retry) {
			// Create a connection to the given URI and queue it for connection once
			// the event loop starts
			websocketpp::lib::error_code ec;
			client::connection_ptr con =
				_client.get_connection(uri, ec);
			_client.connect(con);
			_connection = connection_hdl(con);

			// Start the ASIO io_service run loop
			blog(LOG_INFO, "WSClient::connect: io thread started");
			_client.run();
			blog(LOG_INFO, "WSClient::connect: io thread exited");

			if (_retry) {
				blog(LOG_INFO,
				     "trying to reconnect to %s in %d seconds.",
				     _uri.c_str(), RECONNECT_DELAY);
				std::this_thread::sleep_for(
					std::chrono::seconds(RECONNECT_DELAY));
			}
		}
	});

	blog(LOG_INFO, "WSClient::connect: exited");
}

void WSClient::sendMessage(OBSWeakSource scene, OBSWeakSource transition)
{
	if (!scene) {
		return;
	}

	OBSData data = obs_data_create();
	obs_data_set_string(data, SCENE_ENTRY,
			    GetWeakSourceName(scene).c_str());
	obs_data_set_string(data, TRANSITION_ENTRY,
			    GetWeakSourceName(transition).c_str());
	std::string message = obs_data_get_json(data);
	obs_data_release(data);

	websocketpp::lib::error_code ec;
	_client.send(_connection, message, websocketpp::frame::opcode::text,
		     ec);

	if (switcher->verbose) {
		blog(LOG_INFO, "client sent message:\n%s", message.c_str());
	}
}

void WSClient::disconnect()
{
	_retry = false;
	websocketpp::lib::error_code ec;
	if (!_connection.expired()) {
		_client.close(_connection, websocketpp::close::status::normal,
			      "Client stopping", ec);
	}

	if (_thread.joinable()) {
		_thread.join();
	}
}

void WSClient::onOpen(connection_hdl hdl)
{
	blog(LOG_INFO, "connection to %s opened", _uri.c_str());
}

void WSClient::onFail(connection_hdl hdl)
{
	blog(LOG_INFO, "connection to %s failed", _uri.c_str());
}

void WSClient::onMessage(connection_hdl hdl, client::message_ptr message)
{
	if (message->get_payload() != "message ok") {
		blog(LOG_WARNING, "received response: %s",
		     message->get_payload().c_str());
	}
}

void WSClient::onClose(connection_hdl hdl)
{
	blog(LOG_INFO, "client-connection to %s closed.", _uri.c_str());
}

void SwitcherData::loadNetworkSettings(obs_data_t *obj)
{
	networkConfig.Load(obj);
}

void SwitcherData::saveNetworkSwitches(obs_data_t *obj)
{
	networkConfig.Save(obj);
	if (!networkConfig.ServerEnabled) {
		switcher->server.stop();
	}
}

void AdvSceneSwitcher::setupNetworkTab()
{
	ui->serverSettings->setChecked(switcher->networkConfig.ServerEnabled);
	ui->serverPort->setValue(switcher->networkConfig.ServerPort);
	ui->lockToIPv4->setChecked(switcher->networkConfig.LockToIPv4);

	ui->clientSettings->setChecked(switcher->networkConfig.ClientEnabled);
	ui->clientHostname->setText(switcher->networkConfig.Address.c_str());
	ui->clientPort->setValue(switcher->networkConfig.ClientPort);
	ui->restrictSend->setChecked(!switcher->networkConfig.SendAll);
}

void AdvSceneSwitcher::on_serverSettings_toggled(bool on)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.ServerEnabled = on;
	if (on) {
		switcher->server.start(switcher->networkConfig.ServerPort,
				       switcher->networkConfig.LockToIPv4);
	} else {
		switcher->server.stop();
	}
}

void AdvSceneSwitcher::on_serverPort_valueChanged(int value)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.ServerPort = value;
}

void AdvSceneSwitcher::on_lockToIPv4_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.LockToIPv4 = state;
}

void AdvSceneSwitcher::on_serverRestart_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->server.start(switcher->networkConfig.ServerPort,
			       switcher->networkConfig.LockToIPv4);
}

void AdvSceneSwitcher::on_clientSettings_toggled(bool on)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.ClientEnabled = on;

	if (on) {
		switcher->client.connect(
			switcher->networkConfig.GetClientUri());
	} else {
		switcher->client.disconnect();
	}
}

void AdvSceneSwitcher::on_clientHostname_textChanged(const QString &text)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.Address = text.toUtf8().constData();
}

void AdvSceneSwitcher::on_clientPort_valueChanged(int value)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.ClientPort = value;
}

void AdvSceneSwitcher::on_restrictSend_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.SendAll = !state;
}

// TODO: dont block UI in reconnect loop
void AdvSceneSwitcher::on_clientReconnect_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->client.connect(switcher->networkConfig.GetClientUri());
}
