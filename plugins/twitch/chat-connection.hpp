#pragma once
#include "channel-selection.hpp"
#include "token.hpp"

#include <condition_variable>
#include <mutex>
#include <message-buffer.hpp>
#include <QObject>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>

namespace advss {

using websocketpp::connection_hdl;

struct IRCMessage {
	struct Badge {
		std::string name;
		bool enabled;
	};
	struct {
		std::string badgeInfoString;
		std::string badgesString;
		std::vector<Badge> badges;
		int bits = 0;
		std::string color;
		std::string displayName;
		bool isUsingOnlyEmotes = false;
		std::string emotesString;
		bool isFirstMessage = false;
		std::string id;
		bool isMod = false;
		std::string replyParentBody;
		std::string replyParentDisplayName;
		std::string replyParentId;
		std::string replyParentUserId;
		std::string replyParentUserLogin;
		std::string rootParentId;
		std::string rootParentUserLogin;
		bool isSubscriber = false;
		unsigned long long timestamp;
		bool isTurbo = false;
		std::string userId;
		std::string userType;
		bool isVIP = false;
		bool joinedChannel = false;
		bool leftChannel = false;
	} properties;

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

using ChatMessageBuffer = std::shared_ptr<MessageBuffer<IRCMessage>>;
using ChatMessageDispatcher = MessageDispatcher<IRCMessage>;

class TwitchChatConnection : public QObject {
public:
	~TwitchChatConnection();

	static std::shared_ptr<TwitchChatConnection>
	GetChatConnection(const TwitchToken &token,
			  const TwitchChannel &channel);
	[[nodiscard]] ChatMessageBuffer RegisterForMessages();
	[[nodiscard]] ChatMessageBuffer RegisterForWhispers();
	void SendChatMessage(const std::string &message);
	void ConnectToChat();

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
	void HandlePart(const IRCMessage &);
	void HandleNewMessage(const IRCMessage &);
	void HandleWhisper(const IRCMessage &);
	void HandleNotice(const IRCMessage &) const;
	void HandleReconnect();

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

	enum class State { DISCONNECTED, CONNECTING, CONNECTED };
	std::atomic<State> _state = {State::DISCONNECTED};

	std::atomic_bool _authenticated{false};
	std::atomic_bool _stop{false};
	std::string _url;

	ChatMessageDispatcher _messageDispatcher;
	ChatMessageDispatcher _whisperDispatcher;
};

} // namespace advss
