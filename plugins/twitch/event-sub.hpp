#pragma once
#include "message-dispatcher.hpp"

#include <obs.hpp>
#include <websocketpp/client.hpp>
#include <QObject>
#include <mutex>
#include <condition_variable>
#include <set>

#ifdef USE_TWITCH_CLI_MOCK
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#else
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#endif

namespace advss {

#ifdef USE_TWITCH_CLI_MOCK
typedef websocketpp::client<websocketpp::config::asio_client> EventSubWSClient;
#else
typedef websocketpp::client<websocketpp::config::asio_tls_client>
	EventSubWSClient;
#endif

struct Event;
class TwitchToken;

using websocketpp::connection_hdl;
using EventSubMessageBuffer = std::shared_ptr<MessageBuffer<Event>>;
using EventSubMessageDispatcher = MessageDispatcher<Event>;

struct Event {
	std::string id;
	std::string type;
	OBSData data;

	std::string ToString() const;
};

struct Subscription {
	OBSData data;
	std::string id;
	bool operator<(const Subscription &) const;
};

class EventSub : public QObject {
public:
	explicit EventSub();
	virtual ~EventSub();

	void Connect();
	void Disconnect();
	[[nodiscard]] EventSubMessageBuffer RegisterForEvents();
	bool SubscriptionIsActive(const std::string &id);
	static std::string AddEventSubscription(std::shared_ptr<TwitchToken>,
						Subscription);
	void ClearActiveSubscriptions();

private:
	void OnOpen(connection_hdl hdl);
	void OnMessage(connection_hdl hdl,
		       EventSubWSClient::message_ptr message);
	void OnClose(connection_hdl hdl);
	void OnFail(connection_hdl hdl);
	void ConnectThread();

	bool IsValidMessageID(const std::string &);
	bool IsValidID(const std::string &);

	void HandleWelcome(obs_data_t *);
	void HandleKeepAlive() const;
	void HandleNotification(obs_data_t *);
	void HandleReconnect(obs_data_t *);
	void HandleRevocation(obs_data_t *);

	void RegisterInstance();
	void UnregisterInstance();

	EventSubWSClient _client;
	connection_hdl _connection;
	std::thread _thread;
	std::mutex _waitMtx;
	std::mutex _connectMtx;
	std::condition_variable _cv;
	std::atomic_bool _connected{false};
	std::atomic_bool _disconnect{false};
	std::string _url;
	std::string _sessionID;

	std::deque<std::string> _messageIDs;
	std::mutex _subscriptionMtx;
	std::set<Subscription> _activeSubscriptions;
	static std::mutex _instancesMtx;
	static std::vector<EventSub *> _instances;
	EventSubMessageDispatcher _dispatcher;
};

} // namespace advss
