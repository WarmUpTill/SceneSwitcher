#pragma once

#include <map>
#include <set>
#include <atomic>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantHash>
#include <QtCore/QThreadPool>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>


class ConnectionProperties {
public:
	explicit ConnectionProperties();
	bool isAuthenticated();
	void setAuthenticated(bool authenticated);

private:
	std::atomic<bool> _authenticated;
};

using websocketpp::connection_hdl;

typedef websocketpp::server<websocketpp::config::asio> server;

class WSServer : public QObject {
	Q_OBJECT

public:
	explicit WSServer();
	virtual ~WSServer();
	void start(quint16 port, bool lockToIPv4);
	void stop();
	QThreadPool *threadPool() { return &_threadPool; }

private:
	void onOpen(connection_hdl hdl);
	void onMessage(connection_hdl hdl, server::message_ptr message);
	void onClose(connection_hdl hdl);
	
	QString getRemoteEndpoint(connection_hdl hdl);

	server _server;
	quint16 _serverPort;
	bool _lockToIPv4;
	std::set<connection_hdl, std::owner_less<connection_hdl>> _connections;
	std::map<connection_hdl, ConnectionProperties,
		 std::owner_less<connection_hdl>>
		_connectionProperties;
	QMutex _clMutex;
	QThreadPool _threadPool;
};
