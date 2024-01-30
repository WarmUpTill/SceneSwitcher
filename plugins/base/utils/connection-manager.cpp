#include "connection-manager.hpp"
#include "layout-helpers.hpp"
#include "name-dialog.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <algorithm>
#include <QAction>
#include <QMenu>

Q_DECLARE_METATYPE(advss::Connection *);

namespace advss {

static std::deque<std::shared_ptr<Item>> connections;
static void saveConnections(obs_data_t *obj);
static void loadConnections(obs_data_t *obj);
static bool setup();
static bool setupDone = setup();

bool setup()
{
	AddSaveStep(saveConnections);
	AddLoadStep(loadConnections);
	return true;
}

static void saveConnections(obs_data_t *obj)
{
	obs_data_array_t *connectionArray = obs_data_array_create();
	for (const auto &c : connections) {
		obs_data_t *array_obj = obs_data_create();
		c->Save(array_obj);
		obs_data_array_push_back(connectionArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "websocketConnections", connectionArray);
	obs_data_array_release(connectionArray);
}

static void loadConnections(obs_data_t *obj)
{
	connections.clear();

	obs_data_array_t *connectionArray = nullptr;
	// TODO: remove this fallback at some point in the future
	if (!obs_data_has_user_value(obj, "websocketConnections")) {
		connectionArray = obs_data_get_array(obj, "connections");
	} else {
		connectionArray =
			obs_data_get_array(obj, "websocketConnections");
	}
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

Connection::Connection(bool useCustomURI, std::string customURI,
		       std::string name, std::string address, uint64_t port,
		       std::string pass, bool connectOnStart, bool reconnect,
		       int reconnectDelay, bool useOBSWebsocketProtocol)
	: Item(name),
	  _useCustomURI(useCustomURI),
	  _customURI(customURI),
	  _address(address),
	  _port(port),
	  _password(pass),
	  _connectOnStart(connectOnStart),
	  _reconnect(reconnect),
	  _reconnectDelay(reconnectDelay),
	  _useOBSWSProtocol(useOBSWebsocketProtocol),
	  _client(useOBSWebsocketProtocol)
{
}

Connection::Connection(const Connection &other) : Item(other)
{
	_useCustomURI = other._useCustomURI;
	_customURI = other._customURI;
	_name = other._name;
	_address = other._address;
	_port = other._port;
	_password = other._password;
	_connectOnStart = other._connectOnStart;
	_reconnect = other._reconnect;
	_reconnectDelay = other._reconnectDelay;
	_useOBSWSProtocol = other._useOBSWSProtocol;
	_client.UseOBSWebsocketProtocol(_useOBSWSProtocol);
}

Connection &Connection::operator=(const Connection &other)
{
	if (this != &other) {
		_useCustomURI = other._useCustomURI;
		_customURI = other._customURI;
		_name = other._name;
		_address = other._address;
		_port = other._port;
		_password = other._password;
		_connectOnStart = other._connectOnStart;
		_reconnect = other._reconnect;
		_reconnectDelay = other._reconnectDelay;
		_client.UseOBSWebsocketProtocol(_useOBSWSProtocol);
		_useOBSWSProtocol = other._useOBSWSProtocol;
		_client.Disconnect();
	}
	return *this;
}

Connection::~Connection()
{
	_client.Disconnect();
}

static std::string constructUri(std::string addr, int port)
{
	return "ws://" + addr + ":" + std::to_string(port);
}

std::string Connection::GetURI()
{
	if (_useCustomURI) {
		return _customURI;
	}
	return constructUri(_address, _port);
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

void Connection::Load(obs_data_t *obj)
{
	Item::Load(obj);

	if (obs_data_has_user_value(obj, "version")) {
		UseOBSWebsocketProtocol(
			obs_data_get_bool(obj, "useOBSWSProtocol"));
	} else {
		// TODO: Remove this fallback in future version
		_useOBSWSProtocol = true;
	}
	_client.UseOBSWebsocketProtocol(_useOBSWSProtocol);

	_useCustomURI = obs_data_get_bool(obj, "useCustomURI");
	_customURI = obs_data_get_string(obj, "customURI");
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
	obs_data_set_bool(obj, "useCustomURI", _useCustomURI);
	obs_data_set_string(obj, "customURI", _customURI.c_str());
	obs_data_set_bool(obj, "useOBSWSProtocol", _useOBSWSProtocol);
	obs_data_set_string(obj, "address", _address.c_str());
	obs_data_set_int(obj, "port", _port);
	obs_data_set_string(obj, "password", _password.c_str());
	obs_data_set_bool(obj, "connectOnStart", _connectOnStart);
	obs_data_set_bool(obj, "reconnect", _reconnect);
	obs_data_set_int(obj, "reconnectDelay", _reconnectDelay);
	obs_data_set_int(obj, "version", 1);
}

WebsocketMessageBuffer Connection::RegisterForEvents()
{
	return _client.RegisterForEvents();
}

void Connection::UseOBSWebsocketProtocol(bool useOBSWSProtocol)
{
	_useOBSWSProtocol = useOBSWSProtocol;
	_client.UseOBSWebsocketProtocol(useOBSWSProtocol);
}

Connection *GetConnectionByName(const QString &name)
{
	return GetConnectionByName(name.toStdString());
}

Connection *GetConnectionByName(const std::string &name)
{
	for (auto &con : connections) {
		if (con->Name() == name) {
			return dynamic_cast<Connection *>(con.get());
		}
	}
	return nullptr;
}

std::weak_ptr<Connection> GetWeakConnectionByName(const std::string &name)
{
	for (const auto &c : connections) {
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
		return obs_module_text("AdvSceneSwitcher.connection.invalid");
	}
	return con->Name();
}

std::deque<std::shared_ptr<Item>> &GetConnections()
{
	return connections;
}

static bool ConnectionNameAvailable(const QString &name)
{
	return !GetConnectionByName(name);
}

static bool ConnectionNameAvailable(const std::string &name)
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
	: ItemSelection(connections, Connection::Create, AskForSettingsWrapper,
			"AdvSceneSwitcher.connection.select",
			"AdvSceneSwitcher.connection.add",
			"AdvSceneSwitcher.item.nameNotAvailable",
			"AdvSceneSwitcher.connection.configure", parent)
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
		_selection->setCurrentIndex(-1);
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
		_selection->setCurrentIndex(-1);
	}
}

ConnectionSettingsDialog::ConnectionSettingsDialog(QWidget *parent,
						   const Connection &settings)
	: ItemSettingsDialog(settings, connections,
			     "AdvSceneSwitcher.connection.select",
			     "AdvSceneSwitcher.connection.add",
			     "AdvSceneSwitcher.item.nameNotAvailable", parent),
	  _useCustomURI(new QCheckBox()),
	  _customUri(new QLineEdit()),
	  _address(new QLineEdit()),
	  _port(new QSpinBox()),
	  _password(new QLineEdit()),
	  _showPassword(new QPushButton()),
	  _connectOnStart(new QCheckBox()),
	  _reconnect(new QCheckBox()),
	  _reconnectDelay(new QSpinBox()),
	  _useOBSWSProtocol(new QCheckBox()),
	  _test(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.connection.test"))),
	  _status(new QLabel()),
	  _layout(new QGridLayout())
{
	_port->setMaximum(65535);
	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");
	_reconnectDelay->setMaximum(9999);
	_reconnectDelay->setSuffix("s");

	_useCustomURI->setChecked(settings._useCustomURI);
	_customUri->setText(QString::fromStdString(settings._customURI));
	_address->setText(QString::fromStdString(settings._address));
	_port->setValue(settings._port);
	_password->setText(QString::fromStdString(settings._password));
	_connectOnStart->setChecked(settings._connectOnStart);
	_reconnect->setChecked(settings._reconnect);
	_reconnectDelay->setValue(settings._reconnectDelay);
	_useOBSWSProtocol->setChecked(settings._useOBSWSProtocol);

	QWidget::connect(_useCustomURI, SIGNAL(stateChanged(int)), this,
			 SLOT(UseCustomURIChanged(int)));
	QWidget::connect(_useOBSWSProtocol, SIGNAL(stateChanged(int)), this,
			 SLOT(ProtocolChanged(int)));
	QWidget::connect(_reconnect, SIGNAL(stateChanged(int)), this,
			 SLOT(ReconnectChanged(int)));
	QWidget::connect(_showPassword, SIGNAL(pressed()), this,
			 SLOT(ShowPassword()));
	QWidget::connect(_showPassword, SIGNAL(released()), this,
			 SLOT(HidePassword()));
	QWidget::connect(_test, SIGNAL(clicked()), this,
			 SLOT(TestConnection()));

	int row = 0;
	_layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.connection.name")),
		row, 0);
	QHBoxLayout *nameLayout = new QHBoxLayout;
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_layout->addLayout(nameLayout, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.useCustomURI")),
			   row, 0);
	_layout->addWidget(_useCustomURI, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.customURI")),
			   row, 0);
	_layout->addWidget(_customUri, row, 1);
	_customURIRow = row;
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.address")),
			   row, 0);
	_layout->addWidget(_address, row, 1);
	_addressRow = row;
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.connection.port")),
		row, 0);
	_layout->addWidget(_port, row, 1);
	_portRow = row;
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.password")),
			   row, 0);
	auto passLayout = new QHBoxLayout;
	passLayout->addWidget(_password);
	passLayout->addWidget(_showPassword);
	_layout->addLayout(passLayout, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.connection.connectOnStart")),
		row, 0);
	_layout->addWidget(_connectOnStart, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.reconnect")),
			   row, 0);
	_layout->addWidget(_reconnect, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.connection.reconnectDelay")),
		row, 0);
	_layout->addWidget(_reconnectDelay, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.connection.useOBSWebsocketProtocol")),
		row, 0);
	_layout->addWidget(_useOBSWSProtocol, row, 1);
	++row;
	_layout->addWidget(_test, row, 0);
	_layout->addWidget(_status, row, 1);
	++row;
	_layout->addWidget(_buttonbox, row, 0, 1, -1);
	setLayout(_layout);

	MinimizeSizeOfColumn(_layout, 0);
	ReconnectChanged(_reconnect->isChecked());
	ProtocolChanged(_useOBSWSProtocol->isChecked());
	HidePassword();
	UseCustomURIChanged(settings._useCustomURI);
}

void ConnectionSettingsDialog::UseCustomURIChanged(int state)
{
	SetGridLayoutRowVisible(_layout, _addressRow, !state);
	SetGridLayoutRowVisible(_layout, _portRow, !state);
	SetGridLayoutRowVisible(_layout, _customURIRow, state);

	adjustSize();
	updateGeometry();
}

void ConnectionSettingsDialog::ProtocolChanged(int state)
{
	_password->setEnabled(state);
	_showPassword->setEnabled(state);
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
	SetButtonIcon(_showPassword, ":res/images/visible.svg");
	_password->setEchoMode(QLineEdit::Normal);
}

void ConnectionSettingsDialog::HidePassword()
{
	SetButtonIcon(_showPassword, ":res/images/invisible.svg");
	_password->setEchoMode(QLineEdit::PasswordEchoOnEdit);
}

void ConnectionSettingsDialog::TestConnection()
{
	_testConnection.UseOBSWebsocketProtocol(_useOBSWSProtocol->isChecked());
	_testConnection.Disconnect();
	std::string uri = _useCustomURI->isChecked()
				  ? _customUri->text().toStdString()
				  : constructUri(_address->text().toStdString(),
						 _port->value());
	_testConnection.Connect(uri, _password->text().toStdString(), false);
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
	settings._useCustomURI = dialog._useCustomURI->isChecked();
	settings._customURI = dialog._customUri->text().toStdString();
	settings._address = dialog._address->text().toStdString();
	settings._port = dialog._port->value();
	settings._password = dialog._password->text().toStdString();
	settings._connectOnStart = dialog._connectOnStart->isChecked();
	settings._reconnect = dialog._reconnect->isChecked();
	settings._reconnectDelay = dialog._reconnectDelay->value();
	settings.UseOBSWebsocketProtocol(dialog._useOBSWSProtocol->isChecked());
	settings.Reconnect();
	return true;
}

} // namespace advss
