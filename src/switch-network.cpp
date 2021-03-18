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
#define PARAM_SERVER_AUTHREQUIRED "ServerAuthRequired"
#define PARAM_SECRET "AuthSecret"
#define PARAM_SALT "AuthSalt"

#define PARAM_CLIENT_ENABLE "ClientEnabled"
#define PARAM_CLIENT_PORT "ClientPort"
#define PARAM_ADDRESS "Address"
#define PARAM_CLIENT_AUTHREQUIRED "ClientAuthRequired"
#define PARAM_CLIENT_PASS "ClientPass"

NetworkConfig::NetworkConfig()
	: ServerEnabled(true),
	  ServerPort(55555),
	  LockToIPv4(false),
	  ServerAuthRequired(true),
	  Secret(""),
	  Salt(""),
	  ClientEnabled(false),
	  Address(""),
	  ClientPort(55555),
	  ClientAuthRequired(false),
	  ClientPassword("")
{
	qsrand(QTime::currentTime().msec());
	SessionChallenge = GenerateSalt();
}

void NetworkConfig::Load(obs_data_t *obj)
{
	ServerEnabled = obs_data_get_bool(obj, PARAM_SERVER_ENABLE);
	ServerPort = obs_data_get_int(obj, PARAM_SERVER_PORT);
	LockToIPv4 = obs_data_get_bool(obj, PARAM_SERVER_PORT);
	ServerAuthRequired = obs_data_get_bool(obj, PARAM_SERVER_AUTHREQUIRED);
	Secret = obs_data_get_string(obj, PARAM_SECRET);
	Salt = obs_data_get_string(obj, PARAM_SALT);

	ClientEnabled = obs_data_get_bool(obj, PARAM_CLIENT_ENABLE);
	Address = obs_data_get_string(obj, PARAM_ADDRESS);
	ClientPort = obs_data_get_int(obj, PARAM_CLIENT_PORT);
	ClientAuthRequired = obs_data_get_bool(obj, PARAM_CLIENT_AUTHREQUIRED);
	ClientPassword = obs_data_get_string(obj, PARAM_CLIENT_PASS);
}

void NetworkConfig::Save(obs_data_t *obj)
{
	obs_data_set_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_bool(obj, PARAM_SERVER_PORT, LockToIPv4);
	obs_data_set_bool(obj, PARAM_SERVER_AUTHREQUIRED, ServerAuthRequired);
	obs_data_set_string(obj, PARAM_SECRET, Secret.toUtf8().constData());
	obs_data_set_string(obj, PARAM_SALT, Salt.toUtf8().constData());

	obs_data_set_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_bool(obj, PARAM_CLIENT_AUTHREQUIRED, ClientAuthRequired);
	obs_data_set_string(obj, PARAM_CLIENT_PASS, ClientPassword.c_str());
}

void NetworkConfig::SetDefaults(obs_data_t *obj)
{
	obs_data_set_default_bool(obj, PARAM_SERVER_ENABLE, ServerEnabled);
	obs_data_set_default_int(obj, PARAM_SERVER_PORT, ServerPort);
	obs_data_set_default_bool(obj, PARAM_SERVER_PORT, LockToIPv4);
	obs_data_set_default_bool(obj, PARAM_SERVER_AUTHREQUIRED,
				  ServerAuthRequired);
	obs_data_set_default_string(obj, PARAM_SECRET,
				    Secret.toUtf8().constData());
	obs_data_set_default_string(obj, PARAM_SALT, Salt.toUtf8().constData());

	obs_data_set_default_bool(obj, PARAM_CLIENT_ENABLE, ClientEnabled);
	obs_data_set_default_string(obj, PARAM_ADDRESS, Address.c_str());
	obs_data_set_default_int(obj, PARAM_CLIENT_PORT, ClientPort);
	obs_data_set_default_bool(obj, PARAM_CLIENT_AUTHREQUIRED,
				  ClientAuthRequired);
	obs_data_set_default_string(obj, PARAM_CLIENT_PASS,
				    ClientPassword.c_str());
}

QString NetworkConfig::GenerateSalt()
{
	// Generate 32 random chars
	const size_t randomCount = 32;
	QByteArray randomChars;
	for (size_t i = 0; i < randomCount; i++) {
		randomChars.append((char)qrand());
	}

	// Convert the 32 random chars to a base64 string
	QString salt = randomChars.toBase64();

	return salt;
}

QString NetworkConfig::GenerateSecret(QString password, QString salt)
{
	// Concatenate the password and the salt
	QString passAndSalt = "";
	passAndSalt += password;
	passAndSalt += salt;

	// Generate a SHA256 hash of the password and salt
	auto challengeHash = QCryptographicHash::hash(
		passAndSalt.toUtf8(), QCryptographicHash::Algorithm::Sha256);

	// Encode SHA256 hash to Base64
	QString challenge = challengeHash.toBase64();

	return challenge;
}

void NetworkConfig::SetPassword(QString password)
{
	QString newSalt = GenerateSalt();
	QString newChallenge = GenerateSecret(password, newSalt);

	this->Salt = newSalt;
	this->Secret = newChallenge;
}

bool NetworkConfig::CheckAuth(QString response)
{
	// Concatenate auth secret with the challenge sent to the user
	QString challengeAndResponse = "";
	challengeAndResponse += Secret;
	challengeAndResponse += SessionChallenge;

	// Generate a SHA256 hash of challengeAndResponse
	auto hash =
		QCryptographicHash::hash(challengeAndResponse.toUtf8(),
					 QCryptographicHash::Algorithm::Sha256);

	// Encode the SHA256 hash to Base64
	QString expectedResponse = hash.toBase64();

	bool authSuccess = false;
	if (response == expectedResponse) {
		SessionChallenge = GenerateSalt();
		authSuccess = true;
	}

	return authSuccess;
}

ConnectionProperties::ConnectionProperties() : _authenticated(false) {}

bool ConnectionProperties::isAuthenticated()
{
	return _authenticated.load();
}

void ConnectionProperties::setAuthenticated(bool authenticated)
{
	_authenticated.store(authenticated);
}

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

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
		blog(LOG_INFO, "io thread started");
		_server.run();
		blog(LOG_INFO, "io thread exited");
	});

	blog(LOG_INFO, "server started successfully on port %d", _serverPort);
}

void WSServer::stop()
{
	if (!_server.is_listening()) {
		return;
	}

	_server.stop_listening();
	for (connection_hdl hdl : _connections) {
		_server.close(hdl, websocketpp::close::status::going_away,
			      "Server stopping");
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
	//auto config = GetConfig();
	//if ((config && config->AuthRequired) &&
	//    (!authNotRequired.contains(request.methodName())) &&
	//    (!_connProperties.isAuthenticated())) {
	//	return RpcResponse::fail(request, "Not Authenticated");
	//}

	std::string msgContainer(payload);
	const char *msg = msgContainer.c_str();

	OBSData data = obs_data_create_from_json(msg);
	if (!data) {
		blog(LOG_ERROR, "invalid JSON payload received for '%s'", msg);
		return "invalid JSON payload";
	}

	if (!obs_data_has_user_value(data, "scene") ||
	    !obs_data_has_user_value(data, "transition")) {
		return "missing request parameters";
	}

	auto scene = GetWeakSourceByName(obs_data_get_string(data, "scene"));
	auto transition =
		GetWeakSourceByName(obs_data_get_string(data, "transition"));

	switchScene(scene, transition, switcher->tansitionOverrideOverride);

	obs_data_release(data);
	return "ok";
}

void WSServer::onMessage(connection_hdl hdl, server::message_ptr message)
{
	auto opcode = message->get_opcode();
	if (opcode != websocketpp::frame::opcode::text) {
		return;
	}

	QtConcurrent::run(&_threadPool, [=]() {
		std::string payload = message->get_payload();

		QMutexLocker locker(&_clMutex);
		ConnectionProperties &connProperties =
			_connectionProperties[hdl];
		locker.unlock();

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
	_connectionProperties.erase(hdl);
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

void AdvSceneSwitcher::on_serverSettings_toggled(bool on) {}

void AdvSceneSwitcher::on_serverPort_valueChanged(int value) {}

void AdvSceneSwitcher::on_serverAuthRequired_stateChanged(int state) {}

void AdvSceneSwitcher::on_serverPassword_textChanged(const QString &text) {}

void AdvSceneSwitcher::on_lockToIPv4_stateChanged(int state) {}

void AdvSceneSwitcher::on_serverRestart_clicked() {}

void AdvSceneSwitcher::on_clientSettings_toggled(bool on) {}

void AdvSceneSwitcher::on_clientHostname_textChanged(const QString &text) {}

void AdvSceneSwitcher::on_clientPort_valueChanged(int value) {}

void AdvSceneSwitcher::on_clientAuthRequired_stateChanged(int state) {}

void AdvSceneSwitcher::on_clientPassword_textChanged(const QString &text) {}

void AdvSceneSwitcher::on_clientReconnect_clicked() {}
