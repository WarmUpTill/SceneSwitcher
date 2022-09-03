#pragma once

#include <QComboBox>
#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include <deque>
#include <obs.hpp>
#include <websocket-helpers.hpp>

class ConnectionSelection;
class ConnectionSettingsDialog;

class Connection {
public:
	Connection(std::string name, std::string address, uint64_t port,
		   std::string pass, bool connectOnStart, bool reconnect,
		   int reconnectDelay);
	Connection() = default;
	Connection(const Connection &);
	Connection &operator=(const Connection &);
	~Connection();

	void Reconnect();
	void SendMsg(const std::string &msg);
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj);
	std::string GetName() { return _name; }

private:
	std::string _name = "";
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

class ConnectionSettingsDialog : public QDialog {
	Q_OBJECT

public:
	ConnectionSettingsDialog(QWidget *parent, const Connection &);
	static bool AskForSettings(QWidget *parent, Connection &settings);

private slots:
	void NameChanged(const QString &);
	void ReconnectChanged(int);
	void ShowPassword();
	void HidePassword();
	void SetStatus();
	void TestConnection();

private:
	void SetNameWarning(const QString);

	QLineEdit *_name;
	QLabel *_nameHint;
	QLineEdit *_address;
	QSpinBox *_port;
	QLineEdit *_password;
	QPushButton *_showPassword;
	QCheckBox *_connectOnStart;
	QCheckBox *_reconnect;
	QSpinBox *_reconnectDelay;
	QPushButton *_test;
	QLabel *_status;
	QDialogButtonBox *_buttonbox;

	QTimer _statusTimer;
	WSConnection _testConnection;
};

class ConnectionSelection : public QWidget {
	Q_OBJECT

public:
	ConnectionSelection(QWidget *parent = 0);
	void SetConnection(const std::string &);

private slots:
	void ModifyButtonClicked();
	void ChangeSelection(const QString &);
	void RenameConnection();
	void RenameConnection(const QString &oldName, const QString &name);
	void AddConnection(const QString &);
	void RemoveConnection();
	void RemoveConnection(const QString &);
signals:
	void SelectionChanged(const QString &);
	void ConnectionAdded(const QString &);
	void ConnectionRemoved(const QString &);
	void ConnectionRenamed(const QString &oldName, const QString &name);

private:
	Connection *GetCurrentConnection();

	QComboBox *_selection;
	QPushButton *_modify;
};
