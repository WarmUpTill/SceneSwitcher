#pragma once
#include "item-selection-helpers.hpp"
#include "websocket-helpers.hpp"

#include <QComboBox>
#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include <QGridLayout>
#include <deque>
#include <obs.hpp>

namespace advss {

class WSConnectionSelection;
class WSConnectionSettingsDialog;

class WSConnection : public Item {
public:
	WSConnection(bool useCustomURI, std::string customURI, std::string name,
		     std::string address, uint64_t port, std::string pass,
		     bool connectOnStart, bool reconnect, int reconnectDelay,
		     bool useOBSWebsocketProtocol);
	WSConnection() = default;
	WSConnection(const WSConnection &);
	WSConnection &operator=(const WSConnection &);
	~WSConnection();
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<WSConnection>();
	}

	void Reconnect();
	void SendMsg(const std::string &msg);
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetName() const { return _name; }
	WebsocketMessageBuffer RegisterForEvents();
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

	WSClientConnection _client;

	friend WSConnectionSelection;
	friend WSConnectionSettingsDialog;
};

class WSConnectionSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	WSConnectionSettingsDialog(QWidget *parent, const WSConnection &);
	static bool AskForSettings(QWidget *parent, WSConnection &settings);

private slots:
	void UseCustomURIChanged(int);
	void ProtocolChanged(int);
	void ReconnectChanged(int);
	void ShowPassword();
	void HidePassword();
	void SetStatus();
	void TestConnection();

private:
	QCheckBox *_useCustomURI;
	QLineEdit *_customUri;
	QLineEdit *_address;
	QSpinBox *_port;
	QLineEdit *_password;
	QPushButton *_showPassword;
	QCheckBox *_connectOnStart;
	QCheckBox *_reconnect;
	QSpinBox *_reconnectDelay;
	QCheckBox *_useOBSWSProtocol;
	QPushButton *_test;
	QLabel *_status;
	QGridLayout *_layout;

	QTimer _statusTimer;
	WSClientConnection _testConnection;

	int _customURIRow = -1;
	int _addressRow = -1;
	int _portRow = -1;
};

class WSConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	WSConnectionSelection(QWidget *parent = 0);
	void SetConnection(const std::string &);
	void SetConnection(const std::weak_ptr<WSConnection> &);
};

class ConnectionSelectionSignalManager : public QObject {
	Q_OBJECT
public:
	ConnectionSelectionSignalManager(QObject *parent = nullptr);
	static ConnectionSelectionSignalManager *Instance();

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
};

WSConnection *GetConnectionByName(const QString &);
WSConnection *GetConnectionByName(const std::string &);
std::weak_ptr<WSConnection> GetWeakConnectionByName(const std::string &name);
std::weak_ptr<WSConnection> GetWeakConnectionByQString(const QString &name);
std::string GetWeakConnectionName(std::weak_ptr<WSConnection>);
std::deque<std::shared_ptr<Item>> &GetConnections();

} // namespace advss
