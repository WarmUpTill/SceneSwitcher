#pragma once
#include <QComboBox>
#include <message-dispatcher.hpp>
#include <obs-data.h>
#include <variable-number.hpp>
#include <variable-spinbox.hpp>
#include <variable-string.hpp>

/////////

#include "item-selection-helpers.hpp"

namespace advss {

class MqttMessage;
using MqttMessageBuffer = std::shared_ptr<MessageBuffer<MqttMessage>>;
using MqttMessageDispatcher = MessageDispatcher<MqttMessage>;

///

class MqttConnection : public Item {
public:
	MqttConnection(bool useCustomURI, std::string customURI,
		       std::string name, std::string address, uint64_t port,
		       std::string pass, bool connectOnStart, bool reconnect,
		       int reconnectDelay, bool useOBSWebsocketProtocol);
	MqttConnection() = default;
	MqttConnection(const MqttConnection &);
	MqttConnection &operator=(const MqttConnection &);
	~MqttConnection();
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<MqttConnection>();
	}

	void Reconnect();
	void SendMsg(const std::string &msg);
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetName() const { return _name; }
	MqttMessageBuffer RegisterForEvents();
	bool IsUsingOBSProtocol() const { return _useOBSWSProtocol; }
	std::string GetURI() const;
	uint64_t GetPort() const { return _port; }

private:
	void UseOBSWebsocketProtocol(bool);

	bool _useCustomURI = false;
	std::string _customURI = "ws://localhost:4455";
	std::string _address = "localhost";
	uint64_t _port = 4455;
	std::string _password = "password";
	bool _connectOnStart = true;
	bool _reconnect = true;
	int _reconnectDelay = 3;
	bool _useOBSWSProtocol = true;

	//WSClientConnection _client;
	//
	//friend MqttConnectionSelection;
	//friend MqttConnectionSettingsDialog;
};

} // namespace advss
