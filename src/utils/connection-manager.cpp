#include "connection-manager.hpp"
#include "utility.hpp"
#include "name-dialog.hpp"
#include "switcher-data-structs.hpp"

#include <algorithm>
#include <QAction>
#include <QMenu>
#include <QLayout>
#include <obs-module.h>

Q_DECLARE_METATYPE(advss::Connection *);

namespace advss {

void SwitcherData::saveConnections(obs_data_t *obj)
{
	obs_data_array_t *connectionArray = obs_data_array_create();
	for (const auto &c : connections) {
		obs_data_t *array_obj = obs_data_create();
		c->Save(array_obj);
		obs_data_array_push_back(connectionArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "connections", connectionArray);
	obs_data_array_release(connectionArray);
}

void SwitcherData::loadConnections(obs_data_t *obj)
{
	connections.clear();

	obs_data_array_t *connectionArray =
		obs_data_get_array(obj, "connections");
	size_t count = obs_data_array_count(connectionArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(connectionArray, i);
		auto con = Connection::Create();
		connections.emplace_back(con);
		connections.back()->Load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(connectionArray);
}

Connection::Connection(std::string name, std::string address, uint64_t port,
		       std::string pass, bool connectOnStart, bool reconnect,
		       int reconnectDelay)
	: Item(name),
	  _address(address),
	  _port(port),
	  _password(pass),
	  _connectOnStart(connectOnStart),
	  _reconnect(reconnect),
	  _reconnectDelay(reconnectDelay)
{
}

Connection::Connection(const Connection &other) : Item(other)
{
	_name = other._name;
	_address = other._address;
	_port = other._port;
	_password = other._password;
	_connectOnStart = other._connectOnStart;
	_reconnect = other._reconnect;
	_reconnectDelay = other._reconnectDelay;
}

Connection &Connection::operator=(const Connection &other)
{
	if (this != &other) {
		_name = other._name;
		_address = other._address;
		_port = other._port;
		_password = other._password;
		_connectOnStart = other._connectOnStart;
		_reconnect = other._reconnect;
		_reconnectDelay = other._reconnectDelay;
		_client.Disconnect();
	}
	return *this;
}

Connection::~Connection()
{
	_client.Disconnect();
}

std::string GetUri(std::string addr, int port)
{
	return "ws://" + addr + ":" + std::to_string(port);
}

std::string Connection::GetURI()
{
	return GetUri(_address, _port);
}

void Connection::Reconnect()
{
	_client.Disconnect();
	_client.Connect(GetURI(), _password, _reconnect, _reconnectDelay);
}

void Connection::SendMsg(const std::string &msg)
{
	const auto status = _client.GetStatus();
	if (status == WSConnection::Status::DISCONNECTED) {
		_client.Connect(GetURI(), _password, _reconnect,
				_reconnectDelay);
		blog(LOG_WARNING,
		     "could not send message '%s' (connection to '%s' not established)",
		     msg.c_str(), GetURI().c_str());
		return;
	}

	if (status == WSConnection::Status::AUTHENTICATED) {
		_client.SendRequest(msg);
	}
}

Connection *GetConnectionByName(const QString &name)
{
	return GetConnectionByName(name.toStdString());
}
Connection *GetConnectionByName(const std::string &name)
{
	for (auto &con : switcher->connections) {
		if (con->Name() == name) {
			return dynamic_cast<Connection *>(con.get());
		}
	}
	return nullptr;
}

std::weak_ptr<Connection> GetWeakConnectionByName(const std::string &name)
{
	for (const auto &c : switcher->connections) {
		if (c->Name() == name) {
			std::weak_ptr<Connection> wp =
				std::dynamic_pointer_cast<Connection>(c);
			return wp;
		}
	}
	return std::weak_ptr<Connection>();
}

std::weak_ptr<Connection> GetWeakConnectionByQString(const QString &name)
{
	return GetWeakConnectionByName(name.toStdString());
}

std::string GetWeakConnectionName(std::weak_ptr<Connection> connection)
{
	auto con = connection.lock();
	if (!con) {
		return "invalid connection selection";
	}
	return con->Name();
}

bool ConnectionNameAvailable(const QString &name)
{
	return !GetConnectionByName(name);
}

bool ConnectionNameAvailable(const std::string &name)
{
	return ConnectionNameAvailable(QString::fromStdString(name));
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	Connection &ConnectionSettings = dynamic_cast<Connection &>(settings);
	return ConnectionSettingsDialog::AskForSettings(parent,
							ConnectionSettings);
}

ConnectionSelection::ConnectionSelection(QWidget *parent)
	: ItemSelection(switcher->connections, Connection::Create,
			AskForSettingsWrapper,
			"AdvSceneSwitcher.connection.select",
			"AdvSceneSwitcher.connection.add", parent)
{
	// Connect to slots
	QWidget::connect(
		window(),
		SIGNAL(ConnectionRenamed(const QString &, const QString &)),
		this, SLOT(RenameItem(const QString &, const QString &)));
	QWidget::connect(window(), SIGNAL(ConnectionAdded(const QString &)),
			 this, SLOT(AddItem(const QString &)));
	QWidget::connect(window(), SIGNAL(ConnectionRemoved(const QString &)),
			 this, SLOT(RemoveItem(const QString &)));

	// Forward signals
	QWidget::connect(
		this, SIGNAL(ItemRenamed(const QString &, const QString &)),
		window(),
		SIGNAL(ConnectionRenamed(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)), window(),
			 SIGNAL(ConnectionAdded(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)), window(),
			 SIGNAL(ConnectionRemoved(const QString &)));
}

void ConnectionSelection::SetConnection(const std::string &con)
{
	const QSignalBlocker blocker(_selection);
	if (!!GetConnectionByName(con)) {
		_selection->setCurrentText(QString::fromStdString(con));
	} else {
		_selection->setCurrentIndex(0);
	}
}

void ConnectionSelection::SetConnection(
	const std::weak_ptr<Connection> &connection_)
{
	const QSignalBlocker blocker(_selection);
	auto connection = connection_.lock();
	if (connection) {
		SetConnection(connection->Name());
	} else {
		_selection->setCurrentIndex(0);
	}
}

ConnectionSettingsDialog::ConnectionSettingsDialog(QWidget *parent,
						   const Connection &settings)
	: ItemSettingsDialog(settings, switcher->connections,
			     "AdvSceneSwitcher.connection.select",
			     "AdvSceneSwitcher.connection.add", parent),
	  _address(new QLineEdit()),
	  _port(new QSpinBox()),
	  _password(new QLineEdit()),
	  _showPassword(new QPushButton()),
	  _connectOnStart(new QCheckBox()),
	  _reconnect(new QCheckBox()),
	  _reconnectDelay(new QSpinBox()),
	  _test(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.connection.test"))),
	  _status(new QLabel())
{
	_port->setMaximum(65535);
	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");
	_reconnectDelay->setMaximum(9999);
	_reconnectDelay->setSuffix("s");

	_address->setText(QString::fromStdString(settings._address));
	_port->setValue(settings._port);
	_password->setText(QString::fromStdString(settings._password));
	_connectOnStart->setChecked(settings._connectOnStart);
	_reconnect->setChecked(settings._reconnect);
	_reconnectDelay->setValue(settings._reconnectDelay);

	QWidget::connect(_reconnect, SIGNAL(stateChanged(int)), this,
			 SLOT(ReconnectChanged(int)));
	QWidget::connect(_showPassword, SIGNAL(pressed()), this,
			 SLOT(ShowPassword()));
	QWidget::connect(_showPassword, SIGNAL(released()), this,
			 SLOT(HidePassword()));
	QWidget::connect(_test, SIGNAL(clicked()), this,
			 SLOT(TestConnection()));

	QGridLayout *layout = new QGridLayout;
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.connection.name")),
		row, 0);
	QHBoxLayout *nameLayout = new QHBoxLayout;
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	layout->addLayout(nameLayout, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.connection.address")),
			  row, 0);
	layout->addWidget(_address, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.connection.port")),
		row, 0);
	layout->addWidget(_port, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.connection.password")),
			  row, 0);
	QHBoxLayout *passLayout = new QHBoxLayout;
	passLayout->addWidget(_password);
	passLayout->addWidget(_showPassword);
	layout->addLayout(passLayout, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.connection.connectOnStart")),
		row, 0);
	layout->addWidget(_connectOnStart, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.connection.reconnect")),
			  row, 0);
	layout->addWidget(_reconnect, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.connection.reconnectDelay")),
		row, 0);
	layout->addWidget(_reconnectDelay, row, 1);
	++row;
	layout->addWidget(_test, row, 0);
	layout->addWidget(_status, row, 1);
	++row;
	layout->addWidget(_buttonbox, row, 0, 1, -1);
	setLayout(layout);

	ReconnectChanged(_reconnect->isChecked());
	HidePassword();
}

void ConnectionSettingsDialog::ReconnectChanged(int state)
{
	_reconnectDelay->setEnabled(state);
}

void ConnectionSettingsDialog::SetStatus()
{
	switch (_testConnection.GetStatus()) {
	case WSConnection::Status::DISCONNECTED:
		_status->setText(obs_module_text(
			"AdvSceneSwitcher.connection.status.disconnected"));
		break;
	case WSConnection::Status::CONNECTING:
		_status->setText(obs_module_text(
			"AdvSceneSwitcher.connection.status.connecting"));
		break;
	case WSConnection::Status::CONNECTED:
		_status->setText(obs_module_text(
			"AdvSceneSwitcher.connection.status.connected"));
		break;
	case WSConnection::Status::AUTHENTICATED:
		_status->setText(obs_module_text(
			"AdvSceneSwitcher.connection.status.authenticated"));
		break;
	default:
		break;
	}
}

void ConnectionSettingsDialog::ShowPassword()
{
	setButtonIcon(_showPassword, ":res/images/visible.svg");
	_password->setEchoMode(QLineEdit::Normal);
}

void ConnectionSettingsDialog::HidePassword()
{
	setButtonIcon(_showPassword, ":res/images/invisible.svg");
	_password->setEchoMode(QLineEdit::PasswordEchoOnEdit);
}

void ConnectionSettingsDialog::TestConnection()
{
	_testConnection.Disconnect();
	_testConnection.Connect(GetUri(_address->text().toStdString(),
				       _port->value()),
				_password->text().toStdString(), false);
	_statusTimer.setInterval(1000);
	QWidget::connect(&_statusTimer, &QTimer::timeout, this,
			 &ConnectionSettingsDialog::SetStatus);
	_statusTimer.start();
}

bool ConnectionSettingsDialog::AskForSettings(QWidget *parent,
					      Connection &settings)
{
	ConnectionSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	settings._name = dialog._name->text().toStdString();
	settings._address = dialog._address->text().toStdString();
	settings._port = dialog._port->value();
	settings._password = dialog._password->text().toStdString();
	settings._connectOnStart = dialog._connectOnStart->isChecked();
	settings._reconnect = dialog._reconnect->isChecked();
	settings._reconnectDelay = dialog._reconnectDelay->value();
	settings.Reconnect();
	return true;
}

void Connection::Load(obs_data_t *obj)
{
	Item::Load(obj);
	_address = obs_data_get_string(obj, "address");
	_port = obs_data_get_int(obj, "port");
	_password = obs_data_get_string(obj, "password");
	_connectOnStart = obs_data_get_bool(obj, "connectOnStart");
	_reconnect = obs_data_get_bool(obj, "reconnect");
	_reconnectDelay = obs_data_get_int(obj, "reconnectDelay");

	if (_connectOnStart) {
		_client.Connect(GetURI(), _password, _reconnect,
				_reconnectDelay);
	}
}

void Connection::Save(obs_data_t *obj) const
{
	Item::Save(obj);
	obs_data_set_string(obj, "address", _address.c_str());
	obs_data_set_int(obj, "port", _port);
	obs_data_set_string(obj, "password", _password.c_str());
	obs_data_set_bool(obj, "connectOnStart", _connectOnStart);
	obs_data_set_bool(obj, "reconnect", _reconnect);
	obs_data_set_int(obj, "reconnectDelay", _reconnectDelay);
}

} // namespace advss
