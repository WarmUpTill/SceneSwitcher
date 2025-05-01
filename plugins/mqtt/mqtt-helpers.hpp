#pragma once
#include "message-dispatcher.hpp"
#include "item-selection-helpers.hpp"
#include "topic-selection.hpp"

#include <condition_variable>
#include <obs-data.h>
#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QPushButton>
#include <QSpinBox>
#include <string>
#include <thread>

#include <mqtt/async_client.h>

namespace advss {

using MqttMessageBuffer = std::shared_ptr<MessageBuffer<std::string>>;
using MqttMessageDispatcher = MessageDispatcher<std::string>;

class MqttConnection : public Item {
public:
	MqttConnection() = default;
	~MqttConnection();
	MqttConnection(const MqttConnection &other);
	MqttConnection(const std::string &name, const std::string &uri,
		       const std::string &username, const std::string &password,
		       bool connectOnStart, bool reconnect, int reconnectDelay);
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<MqttConnection>();
	}

	void Connect();
	bool SendMessage(const std::string &topic, const std::string &payload,
			 int qos, bool retained);
	void Load(obs_data_t *data);
	void Save(obs_data_t *data) const;
	MqttMessageBuffer RegisterForEvents();
	bool ConnectOnStartup() const { return _connectOnStart; }
	QString GetURI() const { return QString::fromStdString(_uri); }
	int GetTopicSubscriptionCount() const { return _topics.size(); }
	QString GetStatus() const;

private:
	void ConnectThread();
	void Disconnect();

	std::string _uri = "mqtt://localhost:1883";
	std::string _username = "user";
	std::string _password = "password";

	std::vector<std::string> _topics = {"/#"};
	std::vector<int> _qos = {1};

	std::thread _thread;
	bool _connectOnStart = true;
	bool _reconnect = true;
	int _reconnectDelay = 3;
	std::shared_ptr<mqtt::async_client> _client;
	std::atomic_bool _disconnect{false};
	std::atomic_bool _connected{false};
	std::atomic_bool _connecting{false};
	std::mutex _clientMtx;
	std::mutex _waitMtx;
	std::condition_variable _cv;
	std::string _lastError = "";

	MqttMessageDispatcher _dispatcher;

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
	MqttTopicListWidget *_topics;
	QCheckBox *_connectOnStart;
	QCheckBox *_reconnect;
	QSpinBox *_reconnectDelay;
	QLabel *_status;
	QPushButton *_test;
	QGridLayout *_layout;
	QTimer *_updateStatusTimer = nullptr;
};

class MqttConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	MqttConnectionSelection(QWidget *parent = 0);
	void SetConnection(const std::weak_ptr<MqttConnection> &);
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
MqttConnection *GetMqttConnectionByName(const QString &);
MqttConnection *GetMqttConnectionByName(const std::string &);
std::weak_ptr<MqttConnection>
GetWeakMqttConnectionByName(const std::string &name);
std::weak_ptr<MqttConnection>
GetWeakMqttConnectionByQString(const QString &name);
std::string GetWeakMqttConnectionName(const std::weak_ptr<MqttConnection> &);

} // namespace advss
