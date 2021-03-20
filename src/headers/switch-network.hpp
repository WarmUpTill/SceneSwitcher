/*
Most of this code is based on https://github.com/Palakis/obs-websocket
*/

#pragma once

#include <map>
#include <set>
#include <atomic>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantHash>
#include <QtCore/QThreadPool>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

class NetworkConfig {
public:
	NetworkConfig();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj);
	void SetDefaults(obs_data_t *obj);

	void SetPassword(QString password);
	bool CheckAuth(QString userChallenge);
	QString GenerateSalt();
	static QString GenerateSecret(QString password, QString salt);

	std::string GetClientUri();

	// Server
	bool ServerEnabled;
	uint64_t ServerPort;
	bool LockToIPv4;

	bool ServerAuthRequired;
	QString Secret;
	QString Salt;
	QString SessionChallenge;

	// Client
	bool ClientEnabled;
	std::string Address;
	uint64_t ClientPort;
	bool ClientAuthRequired;
	std::string ClientPassword;
};

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

typedef websocketpp::client<websocketpp::config::asio_client> client;

class WSClient : public QObject {
	Q_OBJECT

public:
	explicit WSClient();
	virtual ~WSClient();
	void connect(std::string uri);
	void disconnect();

private:
	void onOpen(connection_hdl hdl);
	void onFail(connection_hdl hdl);
	void onMessage(connection_hdl hdl, client::message_ptr message);
	void onClose(connection_hdl hdl);

	client _client;
	std::string _uri;
	connection_hdl _connection;
	std::thread _thread;

	bool _retry = false;
};
