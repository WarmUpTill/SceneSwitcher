#include "chat-connection.hpp"
#include "token.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>

#undef DispatchMessage

namespace advss {

using namespace std::chrono_literals;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

static const auto reconnectDelay = 15s;

/* ------------------------------------------------------------------------- */

static std::vector<IRCMessage::Badge> parseBadgeTag(const std::string &tag)
{
	std::vector<std::string> badgePairs;
	size_t badgePos = 0;
	size_t badgeEndPos;

	while ((badgeEndPos = tag.find(',', badgePos)) != std::string::npos) {
		badgePairs.push_back(
			tag.substr(badgePos, badgeEndPos - badgePos));
		badgePos = badgeEndPos + 1;
	}

	badgePairs.push_back(tag.substr(badgePos));

	std::vector<IRCMessage::Badge> badges;
	for (const auto &badgePair : badgePairs) {
		size_t slashPos = badgePair.find('/');
		const auto badgeName = badgePair.substr(0, slashPos);
		const auto badgeValue = (slashPos != std::string::npos)
						? badgePair.substr(slashPos + 1)
						: "";
		badges.push_back({badgeName, badgeValue != "0"});
	}

	return badges;
}

static void parseTags(const std::string &tags, IRCMessage &message)
{
	static constexpr std::array<std::string_view, 2> tagsToIgnore = {
		"client-nonce", "flags"};

	std::vector<std::string> properties;
	size_t pos = 0;
	size_t endPos;

	while ((endPos = tags.find(';', pos)) != std::string::npos) {
		properties.push_back(tags.substr(pos, endPos - pos));
		pos = endPos + 1;
	}

	properties.push_back(tags.substr(pos));

	for (const auto &tagPair : properties) {
		size_t equalsPos = tagPair.find('=');
		std::string tagName = tagPair.substr(0, equalsPos);
		std::string tagValue = (equalsPos != std::string::npos)
					       ? tagPair.substr(equalsPos + 1)
					       : "";

		if (std::find(tagsToIgnore.begin(), tagsToIgnore.end(),
			      tagName) != tagsToIgnore.end()) {
			continue;
		}

		if (tagValue.empty()) {
			continue;
		}

		if (tagName == "badge-info") {
			message.properties.badgeInfoString = tagValue;
		} else if (tagName == "badges") {
			message.properties.badges = parseBadgeTag(tagValue);
			message.properties.badgesString = tagValue;
		} else if (tagName == "bits") {
			message.properties.bits = std::stoi(tagValue);
		} else if (tagName == "color") {
			message.properties.color = tagValue;
		} else if (tagName == "display-name") {
			message.properties.displayName = tagValue;
		} else if (tagName == "emote-only") {
			message.properties.isUsingOnlyEmotes = tagValue == "1";
		} else if (tagName == "emotes") {
			message.properties.emotesString = tagValue;
		} else if (tagName == "first-msg") {
			message.properties.isFirstMessage = tagValue == "1";
		} else if (tagName == "id") {
			message.properties.id = tagValue;
		} else if (tagName == "mod") {
			message.properties.isMod = tagValue == "1";
		} else if (tagName == "reply-parent-msg-body") {
			message.properties.replyParentBody = tagValue;
		} else if (tagName == "reply-parent-display-name") {
			message.properties.replyParentDisplayName = tagValue;
		} else if (tagName == "reply-parent-msg-id") {
			message.properties.replyParentId = tagValue;
		} else if (tagName == "reply-parent-user-id") {
			message.properties.replyParentUserId = tagValue;
		} else if (tagName == "reply-parent-user-login") {
			message.properties.replyParentUserLogin = tagValue;
		} else if (tagName == "reply-thread-parent-msg-id") {
			message.properties.rootParentId = tagValue;
		} else if (tagName == "reply-thread-parent-user-login") {
			message.properties.rootParentUserLogin = tagValue;
		} else if (tagName == "subscriber") {
			message.properties.isSubscriber = tagValue == "1";
		} else if (tagName == "tmi-sent-ts") {
			message.properties.timestamp = std::stoull(tagValue);
		} else if (tagName == "turbo") {
			message.properties.isTurbo = tagValue == "1";
		} else if (tagName == "user-id") {
			message.properties.userId = tagValue;
		} else if (tagName == "user-type") {
			message.properties.userType = tagValue;
		} else if (tagName == "vip") {
			message.properties.isVIP = tagValue == "1";
		}
	}
}

static void parseSource(const std::string &rawSourceComponent,
			IRCMessage &message)
{
	if (rawSourceComponent.empty()) {
		return;
	}
	size_t nickPos = 0;
	size_t nickEndPos = rawSourceComponent.find('!');

	if (nickEndPos != std::string::npos) {
		message.source.nick = rawSourceComponent.substr(
			nickPos, nickEndPos - nickPos);
		message.source.host = rawSourceComponent.substr(nickEndPos + 1);
	} else {
		// Assume the entire source is the host if no '!' is found
		message.source.host = rawSourceComponent;
	}
}

static void parseCommand(const std::string &rawCommandComponent,
			 IRCMessage &message)
{
	std::vector<std::string> commandParts;
	size_t pos = 0;
	size_t endPos;

	while ((endPos = rawCommandComponent.find(' ', pos)) !=
	       std::string::npos) {
		commandParts.push_back(
			rawCommandComponent.substr(pos, endPos - pos));
		pos = endPos + 1;
	}

	commandParts.push_back(rawCommandComponent.substr(pos));

	if (commandParts.empty()) {
		return;
	}

	message.command.command = commandParts[0];
	if (message.command.command == "CAP") {
		if (commandParts.size() < 3) {
			return;
		}
		message.command.parameters = (commandParts[2] == "ACK");
	} else if (message.command.command == "RECONNECT") {
		blog(LOG_INFO,
		     "The Twitch IRC server is about to terminate the connection for maintenance.");
	} else if (message.command.command == "421") {
		// Unsupported command
		return;
	} else if (message.command.command == "PING" ||
		   message.command.command == "001" ||
		   message.command.command == "NOTICE" ||
		   message.command.command == "CLEARCHAT" ||
		   message.command.command == "HOSTTARGET" ||
		   message.command.command == "PRIVMSG") {
		if (commandParts.size() < 2) {
			return;
		}
		message.command.parameters = commandParts[1];
	} else if (message.command.command == "JOIN") {
		if (commandParts.size() < 2) {
			return;
		}
		message.properties.joinedChannel = true;
		message.command.parameters = commandParts[1];
	} else if (message.command.command == "PART") {
		if (commandParts.size() < 2) {
			return;
		}
		message.properties.leftChannel = true;
		message.command.parameters = commandParts[1];
	} else if (message.command.command == "GLOBALUSERSTATE" ||
		   message.command.command == "USERSTATE" ||
		   message.command.command == "ROOMSTATE" ||
		   message.command.command == "002" ||
		   message.command.command == "003" ||
		   message.command.command == "004" ||
		   message.command.command == "353" ||
		   message.command.command == "366" ||
		   message.command.command == "372" ||
		   message.command.command == "375" ||
		   message.command.command == "376") {
		// Do nothing for these cases for now
	} else {
		vblog(LOG_INFO, "Unexpected IRC command: %s",
		      message.command.command.c_str());
	}
}

static IRCMessage parseMessage(const std::string &message)
{
	IRCMessage parsedMessage;

	size_t idx = 0;
	std::string rawTagsComponent;
	std::string rawSourceComponent;
	std::string rawCommandComponent;
	std::string rawMessageComponent;

	if (message[idx] == '@') {
		size_t endIdx = message.find(' ');
		rawTagsComponent = message.substr(1, endIdx - 1);
		idx = endIdx + 1;
	}

	if (message[idx] == ':') {
		idx += 1;
		size_t endIdx = message.find(' ', idx);
		rawSourceComponent = message.substr(idx, endIdx - idx);
		idx = endIdx + 1;
	}

	size_t endIdx = message.find(':', idx);
	if (endIdx == std::string::npos) {
		endIdx = message.length();
	}

	rawCommandComponent = message.substr(idx, endIdx - idx);
	if (endIdx != message.length()) {
		idx = endIdx + 1;
		rawMessageComponent = message.substr(idx);
	}

	parseTags(rawTagsComponent, parsedMessage);
	parseSource(rawSourceComponent, parsedMessage);
	parseCommand(rawCommandComponent, parsedMessage);
	parsedMessage.message = rawMessageComponent;
	return parsedMessage;
}

static std::vector<IRCMessage> parseMessages(const std::string &messagesStr)
{
	static constexpr std::string_view delimiter = "\r\n";
	std::vector<IRCMessage> messages;

	auto isWhitespace = [](const std::string &str) -> bool {
		for (char c : str) {
			if (!std::isspace(static_cast<unsigned char>(c))) {
				return false;
			}
		}
		return true;
	};

	size_t start = 0;
	size_t end = messagesStr.find(delimiter);
	while (end != std::string::npos) {
		auto message = messagesStr.substr(start, end - start);
		if (isWhitespace(message)) {
			continue;
		}
		auto parsedMessage = parseMessage(message);
		if (parsedMessage.command.command.empty()) {
			vblog(LOG_INFO, "discarding IRC message: %s",
			      message.c_str());
		} else {
			messages.emplace_back(parsedMessage);
		}
		start = end + delimiter.length();
		end = messagesStr.find(delimiter, start);
	}
	auto message = messagesStr.substr(start);
	if (isWhitespace(message)) {
		return messages;
	}
	auto parsedMessage = parseMessage(message);
	if (parsedMessage.command.command.empty()) {
		vblog(LOG_INFO, "discarding IRC message: %s", message.c_str());
		return messages;
	}
	messages.emplace_back(parsedMessage);
	return messages;
}

/* ------------------------------------------------------------------------- */

static constexpr std::string_view defaultURL =
	"wss://irc-ws.chat.twitch.tv:443";

std::map<TwitchChatConnection::ChatMapKey, std::weak_ptr<TwitchChatConnection>>
	TwitchChatConnection::_chatMap = {};

TwitchChatConnection::TwitchChatConnection(const TwitchToken &token,
					   const TwitchChannel &channel)
	: QObject(nullptr),
	  _token(token),
	  _channel(channel)
{
	_client.get_alog().clear_channels(
		websocketpp::log::alevel::frame_header |
		websocketpp::log::alevel::frame_payload |
		websocketpp::log::alevel::control);
	_client.init_asio();
#ifndef _WIN32
	_client.set_reuse_addr(true);
#endif

	_client.set_open_handler(bind(&TwitchChatConnection::OnOpen, this, _1));
	_client.set_message_handler(
		bind(&TwitchChatConnection::OnMessage, this, _1, _2));
	_client.set_close_handler(
		bind(&TwitchChatConnection::OnClose, this, _1));
	_client.set_fail_handler(bind(&TwitchChatConnection::OnFail, this, _1));
	_client.set_tls_init_handler([](websocketpp::connection_hdl) {
		return websocketpp::lib::make_shared<asio::ssl::context>(
			asio::ssl::context::sslv23_client);
	});
	_url = defaultURL.data();
}

TwitchChatConnection::~TwitchChatConnection()
{
	Disconnect();
}

std::shared_ptr<TwitchChatConnection>
TwitchChatConnection::GetChatConnection(const TwitchToken &token_,
					const TwitchChannel &channel)
{
	auto token = token_.GetToken();
	if (!token ||
	    !token_.AnyOptionIsEnabled({{"chat:read"}, {"chat:edit"}}) ||
	    channel.GetName().empty()) {
		return {};
	}
	auto key = ChatMapKey{channel.GetName(), *token};
	auto it = _chatMap.find(key);
	if (it != _chatMap.end()) {
		auto connection = it->second.lock();
		if (connection) {
			return connection;
		}
	}
	auto connection = std::shared_ptr<TwitchChatConnection>(
		new TwitchChatConnection(token_, channel));
	_chatMap[key] = connection;
	connection->ConnectToChat();
	return connection;
}

void TwitchChatConnection::ConnectThread()
{
	while (!_stop) {
		_state = State::CONNECTING;
		_client.reset();
		websocketpp::lib::error_code ec;
		websocketpp::client<websocketpp::config::asio_tls_client>::
			connection_ptr con = _client.get_connection(_url, ec);
		if (ec) {
			blog(LOG_INFO, "TwitchChatConnection failed: %s",
			     ec.message().c_str());
		} else {
			_client.connect(con);
			_connection = connection_hdl(con);
			_client.run();
		}

		if (_stop) {
			break;
		}

		blog(LOG_INFO,
		     "TwitchChatConnection trying to reconnect to in %ld seconds.",
		     (long int)reconnectDelay.count());
		std::unique_lock<std::mutex> lock(_waitMtx);
		_cv.wait_for(lock, std::chrono::seconds(reconnectDelay));
	}
	_state = State::DISCONNECTED;
}

void TwitchChatConnection::Connect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	if (_state == State::CONNECTING || _state == State::CONNECTED) {
		vblog(LOG_INFO,
		      "Twitch TwitchChatConnection connect already in progress");
		return;
	}

	if (_thread.joinable()) {
		_thread.join();
	}

	_stop = false;
	_state = State::CONNECTING;
	_thread = std::thread(&TwitchChatConnection::ConnectThread, this);
}

void TwitchChatConnection::Disconnect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	if (_state == State::DISCONNECTED) {
		vblog(LOG_INFO, "TwitchChatConnection already disconnected");
		return;
	}

	_stop = true;

	websocketpp::lib::error_code ec;
	_client.close(_connection, websocketpp::close::status::normal,
		      "Twitch chat connection stopping", ec);
	if (ec) {
		blog(LOG_INFO, "TwitchChatConnection close failed: %s",
		     ec.message().c_str());

		// TODO: avoid using stop()
		_stop = true;
		std::this_thread::sleep_for(3s);
		_client.stop();
	}

	{
		std::unique_lock<std::mutex> waitLock(_waitMtx);
		_cv.notify_all();
	}

	if (_thread.joinable()) {
		_thread.join();
	}
}

static std::string toLowerCase(const std::string &str)
{
	std::string result;
	result.reserve(str.length());

	for (char c : str) {
		result.push_back(std::tolower(static_cast<unsigned char>(c)));
	}

	return result;
}

void TwitchChatConnection::ConnectToChat()
{
	if (_state == State::DISCONNECTED) {
		Connect();
		return;
	}
}

ChatMessageBuffer TwitchChatConnection::RegisterForMessages()
{
	ConnectToChat();
	return _messageDispatcher.RegisterClient();
}

ChatMessageBuffer TwitchChatConnection::RegisterForWhispers()
{
	ConnectToChat();
	return _whisperDispatcher.RegisterClient();
}

void TwitchChatConnection::SendChatMessage(const std::string &message)
{
	ConnectToChat();
	Send("PRIVMSG " + _joinedChannelName + " :" + message);
}

void TwitchChatConnection::Authenticate()
{
	if (!_token.GetToken()) {
		blog(LOG_INFO,
		     "Joining Twitch chat failed due to missing token!");
	}

	Send("CAP REQ :twitch.tv/membership twitch.tv/tags twitch.tv/commands");
	auto pass = _token.GetToken();
	if (!pass.has_value()) {
		blog(LOG_INFO,
		     "Joining Twitch chat failed due to invalid token!");
	}
	Send("PASS oauth:" + *pass);
	Send("NICK " + _token.GetName());
}

void TwitchChatConnection::JoinChannel(const std::string &channelName)
{
	if (!_authenticated) {
		return;
	}
	Send("JOIN #" + channelName);
}

static bool nickMatchesTokenUser(const std::string &nick, TwitchToken &token)
{
	auto tokenUserName = token.GetName();
	std::transform(tokenUserName.begin(), tokenUserName.end(),
		       tokenUserName.begin(),
		       [](unsigned char c) { return std::tolower(c); });
	return nick == tokenUserName;
}

void TwitchChatConnection::HandleJoin(const IRCMessage &message)
{
	if (!nickMatchesTokenUser(message.source.nick, _token)) {
		_messageDispatcher.DispatchMessage(message);
		return;
	}
	_joinedChannelName = std::get<std::string>(message.command.parameters);
	vblog(LOG_INFO, "Twitch chat join was successful!");
}

void TwitchChatConnection::HandlePart(const IRCMessage &message)
{
	if (!nickMatchesTokenUser(message.source.nick, _token)) {
		_messageDispatcher.DispatchMessage(message);
		return;
	}
	vblog(LOG_INFO, "Left Twitch chat!");
}

void TwitchChatConnection::HandleNewMessage(const IRCMessage &message)
{
	_messageDispatcher.DispatchMessage(message);
	vblog(LOG_INFO, "Received new chat message %s",
	      message.message.c_str());
}

void TwitchChatConnection::HandleWhisper(const IRCMessage &message)
{
	_whisperDispatcher.DispatchMessage(message);
	vblog(LOG_INFO, "Received new chat whisper message %s",
	      message.message.c_str());
}

void TwitchChatConnection::HandleNotice(const IRCMessage &message) const
{
	if (message.message == "Login unsuccessful") {
		blog(LOG_INFO, "Twitch chat connection was unsuccessful: %s",
		     message.message.c_str());
		return;
	} else if (message.message ==
		   "You don't have permission to perform that action") {
		blog(LOG_INFO,
		     "No permission. Check if the access token is still valid");
		return;
	}

	vblog(LOG_INFO, "Twitch chat notice: %s", message.message.c_str());
}

void TwitchChatConnection::HandleReconnect()
{
	blog(LOG_INFO,
	     "Received RECONNECT notice! Twitch chat connection will be terminated shortly!");

	// We need a separate thread here as we cannot close the current
	// connection from within the message handler
	std::thread reconnectThread([token = this->_token,
				     channel = this->_channel]() {
		auto chatConnection =
			TwitchChatConnection::GetChatConnection(token, channel);
		chatConnection->Disconnect();
		chatConnection->Connect();
	});
	reconnectThread.detach();
}

void TwitchChatConnection::OnOpen(connection_hdl)
{
	vblog(LOG_INFO, "Twitch chat connection opened");
	_state = State::CONNECTED;
	Authenticate();
}

void TwitchChatConnection::OnMessage(
	connection_hdl,
	websocketpp::client<websocketpp::config::asio_tls_client>::message_ptr
		message)
{
	static constexpr std::string_view authOKCommand = "001";
	static constexpr std::string_view pingCommand = "PING";
	static constexpr std::string_view joinCommand = "JOIN";
	static constexpr std::string_view partCommand = "PART";
	static constexpr std::string_view noticeCommand = "NOTICE";
	static constexpr std::string_view reconnectCommand = "RECONNECT";
	static constexpr std::string_view newMessageCommand = "PRIVMSG";
	static constexpr std::string_view whisperCommand = "WHISPER";

	if (!message) {
		return;
	}
	if (message->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}

	std::string payload = message->get_payload();
	const auto messages = parseMessages(payload);

	for (const auto &message : messages) {
		if (message.command.command == authOKCommand) {
			vblog(LOG_INFO,
			      "Twitch chat connection authenticated!");
			_authenticated = true;
			JoinChannel(_channel.GetName());
		} else if (message.command.command == pingCommand) {
			Send("PONG " +
			     std::get<std::string>(message.command.parameters));
		} else if (message.command.command == joinCommand) {
			HandleJoin(message);
		} else if (message.command.command == partCommand) {
			HandlePart(message);
		} else if (message.command.command == newMessageCommand) {
			HandleNewMessage(message);
		} else if (message.command.command == whisperCommand) {
			HandleWhisper(message);
		} else if (message.command.command == noticeCommand) {
			HandleNotice(message);
		} else if (message.command.command == reconnectCommand) {
			HandleReconnect();
		}
	}
}

void TwitchChatConnection::OnClose(connection_hdl hdl)
{
	websocketpp::client<websocketpp::config::asio_tls_client>::connection_ptr
		con = _client.get_con_from_hdl(hdl);
	auto msg = con->get_ec().message();
	blog(LOG_INFO, "Twitch chat connection closed: %s", msg.c_str());
}

void TwitchChatConnection::OnFail(connection_hdl hdl)
{
	websocketpp::client<websocketpp::config::asio_tls_client>::connection_ptr
		con = _client.get_con_from_hdl(hdl);
	auto msg = con->get_ec().message();
	blog(LOG_INFO, "Twitch chat connection failed: %s", msg.c_str());
}

void TwitchChatConnection::Send(const std::string &msg)
{
	if (_connection.expired()) {
		return;
	}
	websocketpp::lib::error_code errorCode;
	_client.send(_connection, msg, websocketpp::frame::opcode::text,
		     errorCode);
	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "Twitch chat websocket send failed: %s",
		     errorCodeMessage.c_str());
	}
}

bool TwitchChatConnection::ChatMapKey::operator<(const ChatMapKey &other) const
{
	return (channelName + token) < (other.channelName + other.token);
}

} // namespace advss
