#pragma once
#include "channel-selection.hpp"
#include "token.hpp"

#include <websocketpp/client.hpp>
#include <QObject>
#include <mutex>
#include <condition_variable>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>

namespace advss {

using websocketpp::connection_hdl;

struct ParsedTags {
	using BadgeMap = std::unordered_map<std::string, std::string>;
	using EmoteMap = std::unordered_map<std::string,
					    std::vector<std::pair<int, int>>>;
	using EmoteSet = std::vector<std::string>;
	std::unordered_map<std::string, std::variant<std::string, BadgeMap,
						     EmoteMap, EmoteSet>>
		tagMap;
};

struct IRCMessage {
	ParsedTags tags;
	struct {
		std::string nick;
		std::string host;
	} source;
	struct {
		std::string command;
		std::variant<std::string, bool, std::vector<std::string>>
			parameters;
	} command;
	std::string message;
};

class TwitchChatConnection : public QObject {
public:
	~TwitchChatConnection();

	static std::shared_ptr<TwitchChatConnection>
	GetChatConnection(const TwitchToken &token,
			  const TwitchChannel &channel);
	std::vector<IRCMessage> Messages();
	std::vector<IRCMessage> Whispers();
	void SendChatMessage(const std::string &message);

	static void ClearAllMessages();
	void ClearMessages();

private:
	TwitchChatConnection(const TwitchToken &token,
			     const TwitchChannel &channel);

	void Connect();
	void Disconnect();

	void OnOpen(connection_hdl hdl);
	void
	OnMessage(connection_hdl hdl,
		  websocketpp::client<websocketpp::config::asio_tls_client>::
			  message_ptr message);
	void OnClose(connection_hdl hdl);
	void OnFail(connection_hdl hdl);
	void Send(const std::string &msg);
	void ConnectThread();

	void Authenticate();
	void JoinChannel(const std::string &);
	void HandleJoin(const IRCMessage &);
	void HandleNewMessage(const IRCMessage &);
	void HandleWhisper(const IRCMessage &);
	void HandleNotice(const IRCMessage &) const;
	void HandleReconnect();

	void RegisterInstance();
	void UnregisterInstance();

	struct ChatMapKey {
		std::string channelName;
		std::string token;
		bool operator<(const ChatMapKey &) const;
	};
	static std::map<ChatMapKey, std::weak_ptr<TwitchChatConnection>>
		_chatMap;

	TwitchToken _token;
	TwitchChannel _channel;
	std::string _joinedChannelName;

	websocketpp::client<websocketpp::config::asio_tls_client> _client;
	connection_hdl _connection;
	std::thread _thread;
	std::mutex _waitMtx;
	std::mutex _connectMtx;
	std::condition_variable _cv;
	std::atomic_bool _connected{false};
	std::atomic_bool _authenticated{false};
	std::atomic_bool _disconnect{false};
	std::string _url;

	std::mutex _messageMtx;
	std::vector<IRCMessage> _messages;
	std::vector<IRCMessage> _whispers;
	static std::mutex _instancesMtx;
	static std::vector<TwitchChatConnection *> _instances;
	static bool _setupDone;
};

} // namespace advss
