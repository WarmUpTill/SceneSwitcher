/*
Most of this code is based on https://github.com/Palakis/obs-websocket
*/
#include <QtWidgets/QMainWindow>
#include <QtConcurrent/QtConcurrent>
#include <QMessageBox>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

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

		//auto config = GetConfig();
		//if (config && config->DebugEnabled) {
		//	blog(LOG_INFO, "Request >> '%s'", payload.c_str());
		//}

		//WSRequestHandler requestHandler(connProperties);
		//std::string response = OBSRemoteProtocol::processMessage(
		//	requestHandler, payload);
		//
		//if (config && config->DebugEnabled) {
		//	blog(LOG_INFO, "Response << '%s'", response.c_str());
		//}
		//
		//websocketpp::lib::error_code errorCode;
		//_server.send(hdl, response, websocketpp::frame::opcode::text,
		//	     errorCode);
		//
		//if (errorCode) {
		//	std::string errorCodeMessage = errorCode.message();
		//	blog(LOG_INFO, "server(response): send failed: %s",
		//	     errorCodeMessage.c_str());
		//}
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

