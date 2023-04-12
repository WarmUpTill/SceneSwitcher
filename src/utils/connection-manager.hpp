#pragma once
#include "item-selection-helpers.hpp"

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
#include <deque>
#include <obs.hpp>
#include <websocket-helpers.hpp>

namespace advss {

class ConnectionSelection;
class ConnectionSettingsDialog;

class Connection : public Item {
public:
	Connection(std::string name, std::string address, uint64_t port,
		   std::string pass, bool connectOnStart, bool reconnect,
		   int reconnectDelay);
	Connection() = default;
	Connection(const Connection &);
	Connection &operator=(const Connection &);
	~Connection();
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<Connection>();
	}

	void Reconnect();
	void SendMsg(const std::string &msg);
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetName() { return _name; }
	std::vector<std::string> &Events() { return _client.Events(); }

private:
	std::string _address = "localhost";
	uint64_t _port = 4455;
	std::string _password = "password";
	bool _connectOnStart = true;
	bool _reconnect = true;
	int _reconnectDelay = 3;

	std::string GetURI();
	WSConnection _client;

	friend ConnectionSelection;
	friend ConnectionSettingsDialog;
};

Connection *GetConnectionByName(const QString &);
Connection *GetConnectionByName(const std::string &);
std::weak_ptr<Connection> GetWeakConnectionByName(const std::string &name);
std::weak_ptr<Connection> GetWeakConnectionByQString(const QString &name);
std::string GetWeakConnectionName(std::weak_ptr<Connection>);

class ConnectionSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	ConnectionSettingsDialog(QWidget *parent, const Connection &);
	static bool AskForSettings(QWidget *parent, Connection &settings);

private slots:
	void ReconnectChanged(int);
	void ShowPassword();
	void HidePassword();
	void SetStatus();
	void TestConnection();

private:
	QLineEdit *_address;
	QSpinBox *_port;
	QLineEdit *_password;
	QPushButton *_showPassword;
	QCheckBox *_connectOnStart;
	QCheckBox *_reconnect;
	QSpinBox *_reconnectDelay;
	QPushButton *_test;
	QLabel *_status;

	QTimer _statusTimer;
	WSConnection _testConnection;
};

class ConnectionSelection : public ItemSelection {
	Q_OBJECT

public:
	ConnectionSelection(QWidget *parent = 0);
	void SetConnection(const std::string &);
	void SetConnection(const std::weak_ptr<Connection> &);
};

} // namespace advss
