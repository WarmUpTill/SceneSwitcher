#pragma once

#include <set>
#include <QtCore/QObject>
#include <QtCore/QMutex>
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

#include <obs.hpp>

namespace advss {

using websocketpp::connection_hdl;

void SendWebsocketEvent(const std::string &);
std::string ConstructVendorRequestMessage(const std::string &message);
std::vector<std::string> &GetWebsocketMessages();

class WSConnection : public QObject {
	using server = websocketpp::server<websocketpp::config::asio>;
	using client = websocketpp::client<websocketpp::config::asio_client>;

public:
	explicit WSConnection(bool useOBSProtocol = true);
	virtual ~WSConnection();

	void Connect(const std::string &uri, const std::string &pass,
		     bool _reconnect, int reconnectDelay = 10);
	void Disconnect();
	void SendRequest(const std::string &msg);
	std::vector<std::string> &Events() { return _messages; }
	std::string GetFail() { return _failMsg; }

	enum class Status {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		AUTHENTICATED,
	};
	Status GetStatus() const;
	void UseOBSWebsocketProtocol(bool);

private:
	void OnGenericOpen(connection_hdl hdl);
	void OnOBSOpen(connection_hdl hdl);
	void OnGenericMessage(connection_hdl hdl, client::message_ptr message);
	void OnOBSMessage(connection_hdl hdl, client::message_ptr message);
	void OnClose(connection_hdl hdl);
	void Send(const std::string &);
	void ConnectThread();
	void HandleHello(obs_data_t *helloMsg);
	void HandleEvent(obs_data_t *event);
	void HandleResponse(obs_data_t *response);

	client _client;
	std::string _uri = "";
	std::string _password = "";
	connection_hdl _connection;
	std::thread _thread;
	bool _reconnect = false;
	int _reconnectDelay = 10;
	std::mutex _waitMtx;
	std::mutex _connectMtx;
	std::condition_variable _cv;
	std::string _failMsg = "";
	std::atomic<Status> _status = {Status::DISCONNECTED};
	std::atomic_bool _disconnect{false};

	std::vector<std::string> _messages;
};

} // namespace advss
