#include "chat-connection.hpp"
#include "token.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <plugin-state-helpers.hpp>

namespace advss {

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

static constexpr int reconnectDelay = 15;
static constexpr int messageIDBufferSize = 20;

/* ------------------------------------------------------------------------- */

static ParsedTags parseTags(const std::string &tags)
{
	ParsedTags parsedTags;
	static constexpr std::array<std::string_view, 2> tagsToIgnore = {
		"client-nonce", "flags"};

	std::vector<std::string> parsedTagPairs;
	size_t pos = 0;
	size_t endPos;

	while ((endPos = tags.find(';', pos)) != std::string::npos) {
		parsedTagPairs.push_back(tags.substr(pos, endPos - pos));
		pos = endPos + 1;
	}

	parsedTagPairs.push_back(tags.substr(pos));

	for (const auto &tagPair : parsedTagPairs) {
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
			parsedTags.tagMap[tagName] = {};
			continue;
		}

		if (tagName == "badges" || tagName == "badge-info") {
			ParsedTags::BadgeMap badgeMap;
			std::vector<std::string> badgePairs;
			size_t badgePos = 0;
			size_t badgeEndPos;

			while ((badgeEndPos = tagValue.find(',', badgePos)) !=
			       std::string::npos) {
				badgePairs.push_back(tagValue.substr(
					badgePos, badgeEndPos - badgePos));
				badgePos = badgeEndPos + 1;
			}

			badgePairs.push_back(tagValue.substr(badgePos));

			for (const auto &badgePair : badgePairs) {
				size_t slashPos = badgePair.find('/');
				std::string badgeName =
					badgePair.substr(0, slashPos);
				std::string badgeValue =
					(slashPos != std::string::npos)
						? badgePair.substr(slashPos + 1)
						: "";

				badgeMap[badgeName] = badgeValue;
			}

			parsedTags.tagMap[tagName] = badgeMap;

		} else if (tagName == "emotes") {
			ParsedTags::EmoteMap emoteMap;
			std::vector<std::string> emotePairs;
			size_t emotePos = 0;
			size_t emoteEndPos;

			while ((emoteEndPos = tagValue.find('/', emotePos)) !=
			       std::string::npos) {
				emotePairs.push_back(tagValue.substr(
					emotePos, emoteEndPos - emotePos));
				emotePos = emoteEndPos + 1;
			}

			emotePairs.push_back(tagValue.substr(emotePos));

			for (const auto &emotePair : emotePairs) {
				size_t colonPos = emotePair.find(':');
				std::string emoteId =
					emotePair.substr(0, colonPos);
				std::string positions =
					(colonPos != std::string::npos)
						? emotePair.substr(colonPos + 1)
						: "";

				std::vector<std::pair<int, int>> textPositions;
				size_t positionPos = 0;
				size_t positionEndPos;

				while ((positionEndPos = positions.find(
						',', positionPos)) !=
				       std::string::npos) {
					std::string position = positions.substr(
						positionPos,
						positionEndPos - positionPos);
					size_t dashPos = position.find('-');
					int startPos = std::stoi(
						position.substr(0, dashPos));
					int endPos = std::stoi(
						position.substr(dashPos + 1));
					textPositions.push_back(
						{startPos, endPos});
					positionPos = positionEndPos + 1;
				}

				textPositions.push_back(
					{std::stoi(
						 positions.substr(positionPos)),
					 std::stoi(positions.substr(
						 positionPos))});
				emoteMap[emoteId] = textPositions;
			}

			parsedTags.tagMap[tagName] = emoteMap;
		} else if (tagName == "emote-sets") {
			ParsedTags::EmoteSet emoteSetIds;
			size_t setIdPos = 0;
			size_t setIdEndPos;

			while ((setIdEndPos = tagValue.find(',', setIdPos)) !=
			       std::string::npos) {
				emoteSetIds.push_back(tagValue.substr(
					setIdPos, setIdEndPos - setIdPos));
				setIdPos = setIdEndPos + 1;
			}

			emoteSetIds.push_back(tagValue.substr(setIdPos));

			parsedTags.tagMap[tagName] = emoteSetIds;
		} else {
			parsedTags.tagMap[tagName] = tagValue;
		}
	}

	return parsedTags;
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
		   message.command.command == "JOIN" ||
		   message.command.command == "PART" ||
		   message.command.command == "NOTICE" ||
		   message.command.command == "CLEARCHAT" ||
		   message.command.command == "HOSTTARGET" ||
		   message.command.command == "PRIVMSG") {
		if (commandParts.size() < 2) {
			return;
		}
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

	parsedMessage.tags = parseTags(rawTagsComponent);
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
	: QObject(nullptr), _token(token), _channel(channel)
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
	RegisterInstance();
}

TwitchChatConnection::~TwitchChatConnection()
{
	Disconnect();
	UnregisterInstance();
}

static bool setupChatMessageClear()
{
	AddIntervalResetStep(&TwitchChatConnection::ClearAllMessages);
	return true;
}

bool TwitchChatConnection::_setupDone = setupChatMessageClear();

void TwitchChatConnection::ClearMessages()
{
	std::lock_guard<std::mutex> lock(_messageMtx);
	_messages.clear();
}

std::mutex TwitchChatConnection::_instancesMtx;
std::vector<TwitchChatConnection *> TwitchChatConnection::_instances;

void TwitchChatConnection::RegisterInstance()
{
	std::lock_guard<std::mutex> lock(_instancesMtx);
	_instances.emplace_back(this);
}

void TwitchChatConnection::UnregisterInstance()
{
	std::lock_guard<std::mutex> lock(_instancesMtx);
	auto it = std::remove(_instances.begin(), _instances.end(), this);
	_instances.erase(it, _instances.end());
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
	return connection;
}

void TwitchChatConnection::ClearAllMessages()
{
	std::lock_guard<std::mutex> lock(_instancesMtx);
	for (const auto &TwitchChatConnection : _instances) {
		TwitchChatConnection->ClearMessages();
	}
}

void TwitchChatConnection::ConnectThread()
{
	while (!_disconnect) {
		std::unique_lock<std::mutex> lock(_waitMtx);
		_client.reset();
		_connected = true;
		websocketpp::lib::error_code ec;
		websocketpp::client<websocketpp::config::asio_tls_client>::
			connection_ptr con = _client.get_connection(_url, ec);
		if (ec) {
			blog(LOG_INFO, "Twitch TwitchChatConnection failed: %s",
			     ec.message().c_str());
		} else {
			_client.connect(con);
			_connection = connection_hdl(con);
			_client.run();
		}

		blog(LOG_INFO,
		     "Twitch TwitchChatConnection trying to reconnect to in %d seconds.",
		     reconnectDelay);
		_cv.wait_for(lock, std::chrono::seconds(reconnectDelay));
	}
	_connected = false;
}

void TwitchChatConnection::Connect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	if (_connected) {
		vblog(LOG_INFO,
		      "Twitch TwitchChatConnection connect already in progress");
		return;
	}
	_disconnect = true;
	if (_thread.joinable()) {
		_thread.join();
	}
	_disconnect = false;
	_thread = std::thread(&TwitchChatConnection::ConnectThread, this);
}

void TwitchChatConnection::Disconnect()
{
	std::lock_guard<std::mutex> lock(_connectMtx);
	_disconnect = true;
	websocketpp::lib::error_code ec;
	_client.close(_connection, websocketpp::close::status::normal,
		      "Twitch chat connection stopping", ec);
	{
		std::unique_lock<std::mutex> waitLock(_waitMtx);
		_cv.notify_all();
	}

	while (_connected) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		_client.close(_connection, websocketpp::close::status::normal,
			      "Twitch chat connection stopping", ec);
	}

	if (_thread.joinable()) {
		_thread.join();
	}
	_connected = false;
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

std::vector<IRCMessage> TwitchChatConnection::Messages()
{
	if (!_connected) {
		Connect();
		return {};
	}

	if (_joinedChannelName !=
	    "#" + toLowerCase(std::string(_channel.GetName()))) {
		// TODO: disconnect previous channel?
		JoinChannel(_channel.GetName());
		return {};
	}

	std::lock_guard<std::mutex> lock(_messageMtx);
	return _messages;
}

std::vector<IRCMessage> TwitchChatConnection::Whispers()
{
	if (!_connected) {
		Connect();
		return {};
	}

	if (_joinedChannelName !=
	    "#" + toLowerCase(std::string(_channel.GetName()))) {
		// TODO: disconnect previous channel?
		JoinChannel(_channel.GetName());
		return {};
	}

	std::lock_guard<std::mutex> lock(_messageMtx);
	return _whispers;
}

void TwitchChatConnection::SendChatMessage(const std::string &message)
{
	if (!_connected) {
		Connect();
		return;
	}

	if (_joinedChannelName !=
	    "#" + toLowerCase(std::string(_channel.GetName()))) {
		// TODO: disconnect previous channel?
		JoinChannel(_channel.GetName());
		return;
	}
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

void TwitchChatConnection::HandleJoin(const IRCMessage &message)
{
	std::lock_guard<std::mutex> lock(_messageMtx);
	_joinedChannelName = std::get<std::string>(message.command.parameters);
	vblog(LOG_INFO, "Twitch chat join was successful!");
}

void TwitchChatConnection::HandleNewMessage(const IRCMessage &message)
{
	std::lock_guard<std::mutex> lock(_messageMtx);
	_messages.emplace_back(message);
	vblog(LOG_INFO, "Received new chat message %s",
	      message.message.c_str());
}

void TwitchChatConnection::HandleWhisper(const IRCMessage &message)
{
	std::lock_guard<std::mutex> lock(_messageMtx);
	_whispers.emplace_back(message);
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
	     "Received RECONNECT notice! Twitch chat connection will be terminated!");
	Disconnect();
}

void TwitchChatConnection::OnOpen(connection_hdl)
{
	vblog(LOG_INFO, "Twitch chat connection opened");
	_connected = true;

	Authenticate();
}

void TwitchChatConnection::OnMessage(
	connection_hdl,
	websocketpp::client<websocketpp::config::asio_tls_client>::message_ptr
		message)
{
	constexpr std::string_view authOKCommand = "001";
	constexpr std::string_view pingCommand = "PING";
	constexpr std::string_view joinOKCommand = "JOIN";
	constexpr std::string_view noticeCommand = "NOTICE";
	constexpr std::string_view reconnectCommand = "RECONNECT";
	constexpr std::string_view newMessageCommand = "PRIVMSG";
	constexpr std::string_view whisperCommand = "WHISPER";

	if (!message) {
		return;
	}
	if (message->get_opcode() != websocketpp::frame::opcode::text) {
		return;
	}

	std::string payload = message->get_payload();
	auto messages = parseMessages(payload);

	for (const auto &message : messages) {
		if (message.command.command == authOKCommand) {
			vblog(LOG_INFO,
			      "Twitch chat connection authenticated!");
			_authenticated = true;
		} else if (message.command.command == pingCommand) {
			Send("PONG " +
			     std::get<std::string>(message.command.parameters));
		} else if (message.command.command == joinOKCommand) {
			HandleJoin(message);
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
	_connected = false;
}

void TwitchChatConnection::OnFail(connection_hdl hdl)
{
	websocketpp::client<websocketpp::config::asio_tls_client>::connection_ptr
		con = _client.get_con_from_hdl(hdl);
	auto msg = con->get_ec().message();
	blog(LOG_INFO, "Twitch chat connection failed: %s", msg.c_str());
	_connected = false;
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
