#include "mqtt-helpers.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <mqtt/async_client.h>
#include <obs.hpp>

#undef DispatchMessage

namespace advss {

void MqttMessage::Load(obs_data_t *data) {}

void MqttMessage::Save(obs_data_t *data) const {}

MqttConnection::MqttConnection(const std::string &name, const std::string &uri,
			       const std::string &username,
			       const std::string &password, bool connectOnStart,
			       bool reconnect, int reconnectDelay)
	: Item(name),
	  _uri(uri),
	  _username(username),
	  _password(password),
	  _connectOnStart(connectOnStart),
	  _reconnect(reconnect),
	  _reconnectDelay(reconnectDelay)
{
}

void MqttConnection::Reconnect()
{

	auto cli = std::make_shared<mqtt::async_client>(
		_uri, std::string("advss_") + _name);

	auto connOpts =
		mqtt::connect_options_builder::v5()
			.clean_start(false)
			.properties({{mqtt::property::SESSION_EXPIRY_INTERVAL,
				      604800}})
			.finalize();

	try {
		cli->start_consuming();

		// Connect to the server

		blog(LOG_INFO, "connecting to MQTT server...");
		auto tok = cli->connect(connOpts);

		// Getting the connect response will block waiting for the
		// connection to complete.
		auto rsp = tok->get_connect_response();

		// Make sure we were granted a v5 connection.
		if (rsp.get_mqtt_version() < MQTTVERSION_5) {
			blog(LOG_INFO, "did not get an MQTT v5 connection");
			return;
		}

		if (!rsp.is_session_present()) {
			blog(LOG_INFO,
			     "session not present on broker. subscribing...");
			cli->subscribe(
				   std::make_shared<mqtt::string_collection>(
					   _topics),
				   _qos)
				->wait();
		}

	} catch (...) {
	}
}

void MqttConnection::Load(obs_data_t *data)
{
	_uri = obs_data_get_string(data, "uri");
	_username = obs_data_get_string(data, "username");
	_password = obs_data_get_string(data, "password");
	_connectOnStart = obs_data_get_bool(data, "connectOnStart");
	_reconnect = obs_data_get_bool(data, "reconnect");
	_reconnectDelay = obs_data_get_bool(data, "reconnectDelay");
}

void MqttConnection::Save(obs_data_t *data) const
{
	obs_data_set_string(data, "uri", _uri.c_str());
	obs_data_set_string(data, "username", _username.c_str());
	obs_data_set_string(data, "password", _password.c_str());
	obs_data_set_bool(data, "connectOnStart", _connectOnStart);
	obs_data_set_bool(data, "reconnect", _reconnect);
	obs_data_set_bool(data, "reconnectDelay", _reconnectDelay);
}

MqttMessageBuffer MqttConnection::RegisterForEvents()
{
	return MqttMessageBuffer();
}

MqttConnectionSettingsDialog::MqttConnectionSettingsDialog(
	QWidget *parent, const MqttConnection &connection)
	: ItemSettingsDialog(
		  connection, GetMqttConnections(),
		  "AdvSceneSwitcher.mqttConnections.select",
		  "AdvSceneSwitcher.mqttConnections.add",
		  "AdvSceneSwitcher.mqttConnections.nameNotAvailable", true,
		  parent),
	  _uri(new QLineEdit()),
	  _username(new QLineEdit()),
	  _password(new QLineEdit()),
	  _showPassword(new QPushButton()),
	  _connectOnStart(new QCheckBox()),
	  _reconnect(new QCheckBox()),
	  _reconnectDelay(new QSpinBox()),
	  _test(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.connection.test"))),
	  _status(new QLabel()),
	  _currentConnection(connection)
{
	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");
	_uri->setText(QString::fromStdString(connection._uri));
	_username->setText(QString::fromStdString(connection._username));
	_password->setText(QString::fromStdString(connection._password));
	_reconnectDelay->setMaximum(9999);
	_reconnectDelay->setSuffix("s");
	_connectOnStart->setChecked(connection._connectOnStart);
	_reconnect->setChecked(connection._reconnect);
	_reconnectDelay->setValue(connection._reconnectDelay);

	QWidget::connect(_showPassword, SIGNAL(pressed()), this,
			 SLOT(ShowPassword()));
	QWidget::connect(_showPassword, SIGNAL(released()), this,
			 SLOT(HidePassword()));
	QWidget::connect(_reconnect, SIGNAL(stateChanged(int)), this,
			 SLOT(ReconnectChanged(int)));
	QWidget::connect(_test, SIGNAL(clicked()), this,
			 SLOT(TestConnection()));

	int row = 0;
	_layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.connection.name")),
		row, 0);
	auto nameLayout = new QHBoxLayout();
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_layout->addLayout(nameLayout, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.address")),
			   row, 0);
	_layout->addWidget(_uri, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.connection.username")),
			   row, 0);
	_layout->addWidget(_username, row, 1);
	++row;
	auto passLayout = new QHBoxLayout();
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
	_layout->addWidget(_test, row, 0);
	_layout->addWidget(_status, row, 1);
	++row;
	_layout->addWidget(_buttonbox, row, 0, 1, -1);
	setLayout(_layout);

	MinimizeSizeOfColumn(_layout, 0);

	ReconnectChanged(_reconnect->isChecked());
	HidePassword();
}

bool MqttConnectionSettingsDialog::AskForSettings(QWidget *parent,
						  MqttConnection &connection)
{
	MqttConnectionSettingsDialog dialog(parent, connection);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	connection._name = dialog._name->text().toStdString();
	connection._uri = dialog._uri->text().toStdString();
	connection._username = dialog._username->text().toStdString();
	connection._password = dialog._password->text().toStdString();
	connection._connectOnStart = dialog._connectOnStart->isChecked();
	connection._reconnect = dialog._reconnect->isChecked();
	connection._reconnectDelay = dialog._reconnectDelay->value();
	connection.Reconnect();
	return true;
}

void MqttConnectionSettingsDialog::ShowPassword()
{
	SetButtonIcon(_showPassword, GetThemeTypeName() == "Light"
					     ? ":res/images/visible.svg"
					     : "theme:Dark/visible.svg");
	_password->setEchoMode(QLineEdit::Normal);
}

void MqttConnectionSettingsDialog::HidePassword()
{
	SetButtonIcon(_showPassword, ":res/images/invisible.svg");
	_password->setEchoMode(QLineEdit::PasswordEchoOnEdit);
}

void MqttConnectionSettingsDialog::ReconnectChanged(int state)
{
	_reconnectDelay->setEnabled(state);
}

void MqttConnectionSettingsDialog::TestConnection()
{
	//TODO
	// _status ...
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	MqttConnection &connection = dynamic_cast<MqttConnection &>(settings);
	if (MqttConnectionSettingsDialog::AskForSettings(parent, connection)) {
		return true;
	}
	return false;
}

MqttConnectionSelection::MqttConnectionSelection(QWidget *parent)
	: ItemSelection(GetMqttConnections(), MqttConnection::Create,
			AskForSettingsWrapper,
			"AdvSceneSwitcher.mqttConnections.select",
			"AdvSceneSwitcher.mqttConnections.add",
			"AdvSceneSwitcher.item.nameNotAvailable",
			"AdvSceneSwitcher.mqttConnections.configure", parent)
{
	// Connect to slots
	QWidget::connect(MqttConnectionSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(RenameItem(const QString &, const QString &)));
	QWidget::connect(MqttConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(MqttConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(RemoveItem(const QString &)));

	// Forward signals
	QWidget::connect(this,
			 SIGNAL(ItemRenamed(const QString &, const QString &)),
			 MqttConnectionSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)),
			 MqttConnectionSignalManager::Instance(),
			 SIGNAL(Add(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)),
			 MqttConnectionSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)));
}

void MqttConnectionSelection::SetConnection(
	const std::weak_ptr<MqttConnection> &connection_)
{
	auto connection = connection_.lock();
	if (connection) {
		SetItem(connection->Name());
	} else {
		SetItem("");
	}
}

MqttConnectionSignalManager::MqttConnectionSignalManager(QObject *parent)
{
	QWidget::connect(this, SIGNAL(Add(const QString &)), this,
			 SLOT(OpenNewConnection(const QString &)));
}

MqttConnectionSignalManager *MqttConnectionSignalManager::Instance()
{
	static MqttConnectionSignalManager manager;
	return &manager;
}

void MqttConnectionSignalManager::OpenNewConnection(const QString &name)
{

	auto weakConnection = GetWeakMqttConnectionByName(name.toStdString());
	auto connection = weakConnection.lock();
	if (!connection) {
		return;
	}
	if (connection->ConnectOnStartup()) {
		connection->Reconnect();
	}
}

std::deque<std::shared_ptr<Item>> &GetMqttConnections()
{
	static std::deque<std::shared_ptr<Item>> connections;
	return connections;
}

MqttConnection *GetMqttConnectionByName(const QString &name)
{
	return GetMqttConnectionByName(name.toStdString());
}

MqttConnection *GetMqttConnectionByName(const std::string &name)
{
	for (const auto &connection : GetMqttConnections()) {
		if (connection->Name() == name) {
			return dynamic_cast<MqttConnection *>(connection.get());
		}
	}
	return nullptr;
}

std::weak_ptr<MqttConnection>
GetWeakMqttConnectionByName(const std::string &name)
{
	for (const auto &connection : GetMqttConnections()) {
		if (connection->Name() == name) {
			std::weak_ptr<MqttConnection> weak =
				std::dynamic_pointer_cast<MqttConnection>(
					connection);
			return weak;
		}
	}
	return std::weak_ptr<MqttConnection>();
}

std::weak_ptr<MqttConnection>
GetWeakMqttConnectionByQString(const QString &name)
{
	return GetWeakMqttConnectionByName(name.toStdString());
}

std::string GetWeakMqttConnectionName(std::weak_ptr<MqttConnection> connection)
{
	auto con = connection.lock();
	if (!con) {
		return obs_module_text("AdvSceneSwitcher.connection.invalid");
	}
	return con->Name();
}

std::string
GetWeakMqttConnectionName(const std::weak_ptr<MqttConnection> &connection_)
{
	auto connection = connection_.lock();
	if (!connection) {
		return obs_module_text("AdvSceneSwitcher.connection.invalid");
	}
	return connection->Name();
}

void SaveMqttConnections(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &connection : GetMqttConnections()) {
		OBSDataAutoRelease obj = obs_data_create();
		connection->Save(obj);
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(obj, "mqttConnections", array);
}

void LoadMqttConnections(obs_data_t *obj)
{
	GetMqttConnections().clear();

	OBSDataArrayAutoRelease array =
		obs_data_get_array(obj, "mqttConnections");
	size_t count = obs_data_array_count(array);

	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease obj = obs_data_array_item(array, i);
		auto connection = MqttConnection::Create();
		GetMqttConnections().emplace_back(connection);
		GetMqttConnections().back()->Load(obj);
	}
}

} // namespace advss
