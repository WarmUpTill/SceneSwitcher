/*
Most of this code is based on https://github.com/Palakis/obs-websocket
*/

#pragma once

#include <set>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantHash>
#include <QtCore/QThreadPool>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <QRunnable>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

namespace advss {

using websocketpp::connection_hdl;
typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::client<websocketpp::config::asio_client> client;

struct SceneSwitchInfo;

class NetworkConfig {
public:
	NetworkConfig();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj);
	void SetDefaults(obs_data_t *obj);

	std::string GetClientUri();

	bool ShouldSendSceneChange();
	bool ShouldSendFrontendSceneChange();
	bool ShouldSendPrviewSceneChange();

	// Server
	bool ServerEnabled;
	uint64_t ServerPort;
	bool LockToIPv4;

	// Client
	bool ClientEnabled;
	std::string Address;
	uint64_t ClientPort;
	bool SendSceneChange;
	bool SendSceneChangeAll;
	bool SendPreview;
};

class WSServer : public QObject {
	Q_OBJECT

public:
	explicit WSServer();
	virtual ~WSServer();
	void start(quint16 port, bool lockToIPv4);
	void stop();
	void sendMessage(SceneSwitchInfo sceneSwitch, bool preview = false);
	QThreadPool *threadPool() { return &_threadPool; }

private:
	void onOpen(connection_hdl hdl);
	void onMessage(connection_hdl hdl, server::message_ptr message);
	void onClose(connection_hdl hdl);

	QString getRemoteEndpoint(connection_hdl hdl);

	server _server;
	quint16 _serverPort = 55555;
	bool _lockToIPv4 = false;
	std::set<connection_hdl, std::owner_less<connection_hdl>> _connections;
	std::recursive_mutex _clMutex;
	QThreadPool _threadPool;
};

enum class ServerStatus {
	NOT_RUNNING,
	STARTING,
	RUNNING,
};

class WSClient : public QObject {
	Q_OBJECT

public:
	explicit WSClient();
	virtual ~WSClient();
	void connect(std::string uri);
	void disconnect();
	std::string getFail() { return _failMsg; }

private:
	void onOpen(connection_hdl hdl);
	void onFail(connection_hdl hdl);
	void onMessage(connection_hdl hdl, client::message_ptr message);
	void onClose(connection_hdl hdl);
	void connectThread();

	client _client;
	std::string _uri;
	connection_hdl _connection;
	std::thread _thread;
	bool _retry = false;
	std::atomic_bool _connected = {false};
	std::mutex _waitMtx;
	std::condition_variable _cv;
	std::string _failMsg;
};

enum class ClientStatus {
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	FAIL,
};

namespace Compatability {
// Reimplement QRunnable for std::function. Retrocompatability for Qt < 5.15
class StdFunctionRunnable : public QRunnable {
	std::function<void()> cb;

public:
	StdFunctionRunnable(std::function<void()> func);
	void run() override;
};

QRunnable *CreateFunctionRunnable(std::function<void()> func);
}

} // namespace advss
