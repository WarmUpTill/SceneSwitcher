/*
Most of this code is based on https://github.com/Palakis/obs-websocket
*/

#include <QtWidgets/QMainWindow>
#include <QTime>
#include <QMessageBox>

#include "advanced-scene-switcher.hpp"
#include "scene-switch-helpers.hpp"
#include "utility.hpp"

namespace advss {

#define PARAM_SERVER_ENABLE "ServerEnabled"
#define PARAM_SERVER_PORT "ServerPort"
#define PARAM_LOCKTOIPV4 "LockToIPv4"

#define PARAM_CLIENT_ENABLE "ClientEnabled"
#define PARAM_CLIENT_PORT "ClientPort"
#define PARAM_ADDRESS "Address"
#define PARAM_CLIENT_SEND_SCENE_CHANGE "SendSceneChange"
#define PARAM_CLIENT_SEND_SCENE_CHANGE_ALL "SendSceneChangeAll"
#define PARAM_CLIENT_SENDPREVIEW "SendPreview"

#define RECONNECT_DELAY 10

#define SCENE_ENTRY "scene"
#define TRANSITION_ENTRY "transition"
#define TRANSITION_DURATION "duration"
#define SET_PREVIEW "preview"

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
	  SendSceneChange(true),
	  SendSceneChangeAll(true),
	  SendPreview(true)
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
	SendSceneChange =
		obs_data_get_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE);
	SendSceneChangeAll =
		obs_data_get_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE_ALL);
	SendPreview = obs_data_get_bool(obj, PARAM_CLIENT_SENDPREVIEW);
}

void NetworkConfig::Save(obs_data_t *obj)
{
	obs_data_set_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_bool(obj, PARAM_LOCKTOIPV4, LockToIPv4);

	obs_data_set_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE, SendSceneChange);
	obs_data_set_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE_ALL,
			  SendSceneChangeAll);
	obs_data_set_bool(obj, PARAM_CLIENT_SENDPREVIEW, SendPreview);
}

void NetworkConfig::SetDefaults(obs_data_t *obj)
{
	obs_data_set_default_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_default_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_default_bool(obj, PARAM_LOCKTOIPV4, LockToIPv4);

	obs_data_set_default_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_default_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_default_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_default_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE,
				  SendSceneChange);
	obs_data_set_default_bool(obj, PARAM_CLIENT_SEND_SCENE_CHANGE_ALL,
				  SendSceneChangeAll);
	obs_data_set_default_bool(obj, PARAM_CLIENT_SENDPREVIEW, SendPreview);
}

std::string NetworkConfig::GetClientUri()
{
	return "ws://" + Address + ":" + std::to_string(ClientPort);
}

bool NetworkConfig::ShouldSendSceneChange()
{
	return ServerEnabled && SendSceneChange;
}

bool NetworkConfig::ShouldSendFrontendSceneChange()
{
	return ShouldSendSceneChange() && SendSceneChangeAll;
}

bool NetworkConfig::ShouldSendPrviewSceneChange()
{
	return ServerEnabled && SendPreview;
}

WSServer::WSServer() : QObject(nullptr), _connections(), _clMutex()
{
	_server.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	_server.init_asio();
#ifndef _WIN32
	_server.set_reuse_addr(true);
#endif

	_server.set_open_handler(bind(&WSServer::onOpen, this, _1));
	_server.set_close_handler(bind(&WSServer::onClose, this, _1));
	_server.set_message_handler(bind(&WSServer::onMessage, this, _1, _2));
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

	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "server: listen failed: %s",
		     errorCodeMessage.c_str());

		QString errorTitle =
			obs_module_text("AdvSceneSwitcher.windowTitle");
		QString errorMessage =
			QString(obs_module_text(
					"AdvSceneSwitcher.networkTab.startFailed.message"))
				.arg(_serverPort)
				.arg(errorCodeMessage.c_str());

		QMainWindow *mainWindow = reinterpret_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		QMessageBox::warning(mainWindow, errorTitle, errorMessage);

		return;
	}
	switcher->serverStatus = ServerStatus::STARTING;

	_server.start_accept();

	_threadPool.start(Compatability::CreateFunctionRunnable([=]() {
		blog(LOG_INFO, "WSServer::start: io thread started");
		_server.run();
		blog(LOG_INFO, "WSServer::start: io thread exited");
	}));

	switcher->serverStatus = ServerStatus::RUNNING;
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

	switcher->serverStatus = ServerStatus::NOT_RUNNING;
	blog(LOG_INFO, "server stopped successfully");
}

void WSServer::sendMessage(SceneSwitchInfo sceneSwitch, bool preview)
{
	if (!sceneSwitch.scene) {
		return;
	}

	OBSData data = obs_data_create();
	obs_data_set_string(data, SCENE_ENTRY,
			    GetWeakSourceName(sceneSwitch.scene).c_str());
	obs_data_set_string(data, TRANSITION_ENTRY,
			    GetWeakSourceName(sceneSwitch.transition).c_str());
	obs_data_set_int(data, TRANSITION_DURATION, sceneSwitch.duration);
	obs_data_set_bool(data, SET_PREVIEW, preview);
	std::string message = obs_data_get_json(data);
	obs_data_release(data);

	for (connection_hdl hdl : _connections) {
		websocketpp::lib::error_code ec;
		_server.send(hdl, message, websocketpp::frame::opcode::text,
			     ec);
		if (ec) {
			std::string errorCodeMessage = ec.message();
			blog(LOG_INFO, "server: send failed: %s",
			     errorCodeMessage.c_str());
		}
	}

	if (switcher->verbose) {
		blog(LOG_INFO, "server sent message:\n%s", message.c_str());
	}
}

void WSServer::onOpen(connection_hdl hdl)
{
	{
		std::lock_guard<std::recursive_mutex> lock(_clMutex);
		_connections.insert(hdl);
	}

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
	    !obs_data_has_user_value(data, TRANSITION_ENTRY) ||
	    !obs_data_has_user_value(data, TRANSITION_DURATION) ||
	    !obs_data_has_user_value(data, SET_PREVIEW)) {
		return "missing request parameters";
	}

	std::string sceneName = obs_data_get_string(data, SCENE_ENTRY);
	std::string transitionName =
		obs_data_get_string(data, TRANSITION_ENTRY);
	int duration = obs_data_get_int(data, TRANSITION_DURATION);
	bool preview = obs_data_get_bool(data, SET_PREVIEW);

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
	if (preview) {
		SwitchPreviewScene(scene);
	} else {
		SwitchScene({scene, transition, duration});
	}
	return ret;
}

void WSServer::onMessage(connection_hdl, server::message_ptr message)
{
	auto opcode = message->get_opcode();
	if (opcode != websocketpp::frame::opcode::text) {
		return;
	}

	_threadPool.start(Compatability::CreateFunctionRunnable([=]() {
		if (message->get_payload() != "message ok") {
			blog(LOG_WARNING, "received response: %s",
			     message->get_payload().c_str());
		}
	}));
}

void WSServer::onClose(connection_hdl hdl)
{
	{
		std::lock_guard<std::recursive_mutex> lock(_clMutex);
		_connections.erase(hdl);
	}

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

	_client.set_open_handler(bind(&WSClient::onOpen, this, _1));
	_client.set_fail_handler(bind(&WSClient::onFail, this, _1));
	_client.set_message_handler(bind(&WSClient::onMessage, this, _1, _2));
	_client.set_close_handler(bind(&WSClient::onClose, this, _1));
}

WSClient::~WSClient()
{
	disconnect();
}

void WSClient::connectThread()
{
	while (_retry) {
		_client.reset();
		switcher->clientStatus = ClientStatus::CONNECTING;
		// Create a connection to the given URI and queue it for connection once
		// the event loop starts
		websocketpp::lib::error_code ec;
		client::connection_ptr con = _client.get_connection(_uri, ec);
		if (ec) {
			_failMsg = ec.message();
			blog(LOG_INFO, "client: connect failed: %s",
			     _failMsg.c_str());
			switcher->clientStatus = ClientStatus::FAIL;
		} else {
			_client.connect(con);
			_connection = connection_hdl(con);

			// Start the ASIO io_service run loop
			blog(LOG_INFO, "WSClient::connect: io thread started");
			_connected = true;
			_client.run();
			_connected = false;
			blog(LOG_INFO, "WSClient::connect: io thread exited");
		}

		if (_retry) {
			std::unique_lock<std::mutex> lck(_waitMtx);
			blog(LOG_INFO,
			     "trying to reconnect to %s in %d seconds.",
			     _uri.c_str(), RECONNECT_DELAY);
			_cv.wait_for(lck,
				     std::chrono::seconds(RECONNECT_DELAY));
		}
	}
}

void WSClient::connect(std::string uri)
{
	disconnect();
	_uri = uri;
	_retry = true;

	_thread = std::thread(&WSClient::connectThread, this);

	switcher->clientStatus = ClientStatus::DISCONNECTED;
	blog(LOG_INFO, "WSClient::connect: exited");
}

void WSClient::disconnect()
{
	_retry = false;
	websocketpp::lib::error_code ec;
	_client.close(_connection, websocketpp::close::status::normal,
		      "Client stopping", ec);

	{
		std::unique_lock<std::mutex> waitLck(_waitMtx);
		blog(LOG_INFO, "trying to reconnect to %s in %d seconds.",
		     _uri.c_str(), RECONNECT_DELAY);
		_cv.notify_all();
	}

	while (_connected) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		_client.close(_connection, websocketpp::close::status::normal,
			      "Client stopping", ec);
	}

	if (_thread.joinable()) {
		_thread.join();
	}
}

void WSClient::onOpen(connection_hdl)
{
	blog(LOG_INFO, "connection to %s opened", _uri.c_str());
	switcher->clientStatus = ClientStatus::CONNECTED;
}

void WSClient::onFail(connection_hdl)
{
	blog(LOG_INFO, "connection to %s failed", _uri.c_str());
}

void WSClient::onMessage(connection_hdl hdl, client::message_ptr message)
{
	auto opcode = message->get_opcode();
	if (opcode != websocketpp::frame::opcode::text) {
		return;
	}

	std::string payload = message->get_payload();
	std::string response = processMessage(payload);
	websocketpp::lib::error_code errorCode;
	_client.send(hdl, response, websocketpp::frame::opcode::text,
		     errorCode);

	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "client(response): send failed: %s",
		     errorCodeMessage.c_str());
	}

	if (switcher->verbose) {
		blog(LOG_INFO, "client sent message:\n%s", response.c_str());
	}
}

void WSClient::onClose(connection_hdl)
{
	blog(LOG_INFO, "client-connection to %s closed.", _uri.c_str());
	switcher->clientStatus = ClientStatus::DISCONNECTED;
}

void SwitcherData::loadNetworkSettings(obs_data_t *obj)
{
	networkConfig.Load(obj);
}

void SwitcherData::saveNetworkSwitches(obs_data_t *obj)
{
	networkConfig.Save(obj);
	if (!networkConfig.ServerEnabled) {
		server.stop();
	}
}

void AdvSceneSwitcher::SetupNetworkTab()
{
	ui->serverSettings->setChecked(switcher->networkConfig.ServerEnabled);
	ui->serverPort->setValue(switcher->networkConfig.ServerPort);
	ui->lockToIPv4->setChecked(switcher->networkConfig.LockToIPv4);

	ui->clientSettings->setChecked(switcher->networkConfig.ClientEnabled);
	ui->clientHostname->setText(switcher->networkConfig.Address.c_str());
	ui->clientPort->setValue(switcher->networkConfig.ClientPort);
	ui->sendSceneChange->setChecked(
		switcher->networkConfig.SendSceneChange);
	ui->restrictSend->setChecked(
		!switcher->networkConfig.SendSceneChangeAll);
	ui->sendPreview->setChecked(switcher->networkConfig.SendPreview);
	ui->restrictSend->setDisabled(!switcher->networkConfig.SendSceneChange);

	QTimer *statusTimer = new QTimer(this);
	connect(statusTimer, SIGNAL(timeout()), this,
		SLOT(UpdateClientStatus()));
	connect(statusTimer, SIGNAL(timeout()), this,
		SLOT(UpdateServerStatus()));
	statusTimer->start(500);
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

void AdvSceneSwitcher::UpdateServerStatus()
{
	switch (switcher->serverStatus) {
	case ServerStatus::NOT_RUNNING:
		ui->serverStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.server.status.notRunning"));
		break;
	case ServerStatus::STARTING:
		ui->serverStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.server.status.starting"));
		break;
	case ServerStatus::RUNNING:
		ui->serverStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.server.status.running"));
		break;
	default:
		break;
	}
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

void AdvSceneSwitcher::on_sendSceneChange_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.SendSceneChange = state;
	ui->restrictSend->setDisabled(!state);
}

void AdvSceneSwitcher::on_restrictSend_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.SendSceneChangeAll = !state;
}

void AdvSceneSwitcher::on_sendPreview_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->networkConfig.SendPreview = state;
}

void AdvSceneSwitcher::on_clientReconnect_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->client.connect(switcher->networkConfig.GetClientUri());
}

void AdvSceneSwitcher::UpdateClientStatus()
{
	switch (switcher->clientStatus) {
	case ClientStatus::DISCONNECTED:
		ui->clientStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.client.status.disconnected"));
		break;
	case ClientStatus::CONNECTING:
		ui->clientStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.client.status.connecting"));
		break;
	case ClientStatus::CONNECTED:
		ui->clientStatus->setText(obs_module_text(
			"AdvSceneSwitcher.networkTab.client.status.connected"));
		break;
	case ClientStatus::FAIL:
		ui->clientStatus->setText(QString("Error: ") +
					  switcher->client.getFail().c_str());
		break;
	default:
		break;
	}
}

void Compatability::StdFunctionRunnable::run()
{
	cb();
}

QRunnable *Compatability::CreateFunctionRunnable(std::function<void()> func)
{
	return new Compatability::StdFunctionRunnable(std::move(func));
}

Compatability::StdFunctionRunnable::StdFunctionRunnable(
	std::function<void()> func)
	: cb(std::move(func))
{
}

} // namespace advss
