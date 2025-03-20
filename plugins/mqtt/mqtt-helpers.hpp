#pragma once
#include <message-dispatcher.hpp>
#include <obs-data.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QPushButton>
#include <QSpinBox>

/////////

#include "item-selection-helpers.hpp"

namespace advss {

class MqttMessage;
using MqttMessageBuffer = std::shared_ptr<MessageBuffer<MqttMessage>>;
using MqttMessageDispatcher = MessageDispatcher<MqttMessage>;

///

class MqttConnection : public Item {
public:
	MqttConnection() = default;
	MqttConnection(const std::string &name, const std::string &uri,
		       const std::string &username, const std::string &password,
		       bool connectOnStart, bool reconnect, int reconnectDelay);
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<MqttConnection>();
	}

	void Reconnect();
	void Load(obs_data_t *data);
	void Save(obs_data_t *data) const;
	std::string GetName() const { return _name; }
	MqttMessageBuffer RegisterForEvents();
	const std::string &GetURI() const { return _uri; }
	bool ConnectOnStartup() const { return _connectOnStart; }

private:
	std::string _uri = "mqtt://localhost:1883";
	std::string _username = "user";
	std::string _password = "password";

	std::vector<std::string> _topics;
	std::vector<int> _qos;

	bool _connectOnStart = true;
	bool _reconnect = true;
	int _reconnectDelay = 3;

	//WSClientConnection _client;
	//
	//friend MqttConnectionSelection;
	friend class MqttConnectionSettingsDialog;
};

class MqttConnectionSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	MqttConnectionSettingsDialog(QWidget *parent, const MqttConnection &);
	static bool AskForSettings(QWidget *parent, MqttConnection &settings);

private slots:
	void ShowPassword();
	void HidePassword();
	void ReconnectChanged(int state);
	void TestConnection();

private:
	QLineEdit *_uri;
	QLineEdit *_username;
	QLineEdit *_password;
	QPushButton *_showPassword;
	QCheckBox *_connectOnStart;
	QCheckBox *_reconnect;
	QSpinBox *_reconnectDelay;
	QLabel *_status;
	QPushButton *_test;
	QGridLayout *_layout;
	MqttConnection _currentConnection;
};

class MqttConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	MqttConnectionSelection(QWidget *parent = 0);
	void SetToken(const std::string &);
	void SetToken(const std::weak_ptr<MqttConnection> &);
};

class MqttConnectionSignalManager : public QObject {
	Q_OBJECT
public:
	MqttConnectionSignalManager(QObject *parent = nullptr);
	static MqttConnectionSignalManager *Instance();

private slots:
	void OpenNewConnection(const QString &);

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
};

std::deque<std::shared_ptr<Item>> &GetMqttConnections();

} // namespace advss
