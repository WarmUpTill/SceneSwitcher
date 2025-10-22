#include "mqtt-helpers.hpp"
#include "help-icon.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs.hpp>
#include <QTimer>

#undef DispatchMessage

namespace advss {

MqttConnection::~MqttConnection()
{
	Disconnect();
}

MqttConnection::MqttConnection(const MqttConnection &other)
	: Item(other._name),
	  _uri(other._uri),
	  _username(other._username),
	  _password(other._password),
	  _trustStore(other._trustStore),
	  _keyStore(other._keyStore),
	  _privateKey(other._privateKey),
	  _verifyServerCert(other._verifyServerCert),
	  _connectOnStart(other._connectOnStart),
	  _reconnect(other._reconnect),
	  _reconnectDelay(other._reconnectDelay)
{
}

MqttConnection::MqttConnection(const std::string &name, const std::string &uri,
			       const std::string &username,
			       const std::string &password,
			       const std::string &trustStore,
			       const std::string &keyStore,
			       const std::string &privateKey,
			       bool verifyServerCert, bool connectOnStart,
			       bool reconnect, int reconnectDelay)
	: Item(name),
	  _uri(uri),
	  _username(username),
	  _password(password),
	  _trustStore(trustStore),
	  _keyStore(keyStore),
	  _privateKey(privateKey),
	  _verifyServerCert(verifyServerCert),
	  _connectOnStart(connectOnStart),
	  _reconnect(reconnect),
	  _reconnectDelay(reconnectDelay)
{
}

void MqttConnection::Connect()
{
	static std::mutex mutex;
	std::scoped_lock<std::mutex> lock(mutex);

	if (_connecting) {
		return;
	}

	if (_thread.joinable()) {
		_thread.join();
	}

	_connecting = true;
	_disconnect = false;
	_thread = std::thread(&MqttConnection::ConnectThread, this);
}

void MqttConnection::Disconnect()
{
	_disconnect = true;
	{
		std::unique_lock<std::mutex> waitLck(_waitMtx);
		_cv.notify_all();
	}
	if (_thread.joinable()) {
		_thread.join();
	}
}

void MqttConnection::ConnectThread()
{
	using namespace std::chrono_literals;

	const auto waitBeforeReconnect = [this]() {
		std::unique_lock<std::mutex> lck(_waitMtx);
		vblog(LOG_INFO,
		      "trying to reconnect to MQTT server \"%s\" in %d seconds.",
		      _name.c_str(), _reconnectDelay);
		_cv.wait_for(lck, std::chrono::seconds(_reconnectDelay));
	};

	const auto logConnectionLost = [this](const std::string &cause) {
		blog(LOG_INFO, "MQTT connection \"%s\" lost: %s", _name.c_str(),
		     cause.c_str());
	};
	const auto dispatchMessage = [this](mqtt::const_message_ptr msg) {
		if (!msg) {
			return;
		}

		vblog(LOG_INFO, "MQTT connection \"%s\" received message: %s",
		      _name.c_str(), msg->to_string().c_str());
		_dispatcher.DispatchMessage(msg->to_string());
	};

	do {
		try {
			std::unique_lock<std::mutex> clientLock(_clientMtx);
			_client = std::make_shared<mqtt::async_client>(
				_uri, std::string("advss_") + _name);

			mqtt::ssl_options sslOptions;
			if (!_trustStore.empty()) {
				sslOptions.set_trust_store(_trustStore);
			}
			if (!_keyStore.empty()) {
				sslOptions.set_key_store(_keyStore);
			}
			if (!_privateKey.empty()) {
				sslOptions.set_private_key(_privateKey);
			}
			sslOptions.set_enable_server_cert_auth(
				_verifyServerCert);

#ifdef ENABLE_MQTT5_SUPPORT
			auto connOpts = mqtt::connect_options_builder::v5()
#else
			auto connOpts = mqtt::connect_options_builder()
#endif
						.clean_start(false)
						.clean_session(true)
						.connect_timeout(5s)
						.user_name(_username)
						.password(_password)
						.ssl(sslOptions)
						.finalize();

			_client->set_connection_lost_handler(logConnectionLost);
			_client->set_message_callback(dispatchMessage);
			_client->start_consuming();

			vblog(LOG_INFO, "connecting to MQTT server \"%s\" ...",
			      _name.c_str());
			auto tok = _client->connect(connOpts);
			auto rsp = tok->get_connect_response();
#ifdef ENABLE_MQTT5_SUPPORT
			if (rsp.get_mqtt_version() < MQTTVERSION_5) {
				blog(LOG_INFO,
				     "\"%s\" did not get an MQTT v5 connection",
				     _name.c_str());
				break;
			}
#endif

			if (!rsp.is_session_present()) {
				vblog(LOG_INFO,
				      "\"%s\" session not present on broker. subscribing...",
				      _name.c_str());
				const auto topics = std::make_shared<
					mqtt::string_collection>(_topics);
				_client->subscribe(topics, _qos)->wait();
			}

			blog(LOG_INFO,
			     "\"%s\" connection established to MQTT server!",
			     _name.c_str());
			_connected = true;

			clientLock.unlock();
			while (!_disconnect && _client->is_connected()) {
				std::unique_lock<std::mutex> lock(_waitMtx);
				_cv.wait_for(lock, std::chrono::seconds(
							   _reconnectDelay));
			}
			clientLock.lock();

			if (_client->is_connected()) {
				blog(LOG_INFO,
				     "Disconnecting from the MQTT server \"%s\"",
				     _name.c_str());
				_client->stop_consuming();
				_client->disconnect()->wait();
			} else {
				blog(LOG_INFO,
				     "MQTT client %s was disconnected",
				     _name.c_str());
			}
			_client.reset();
			_connected = false;
			if (!_reconnect || _disconnect) {
				break;
			}
			waitBeforeReconnect();
		} catch (const std::exception &e) {
			_lastError = e.what();
			blog(LOG_INFO, "%s %s", __func__, _lastError.c_str());
			_client.reset();
			_connected = false;
			if (!_reconnect || _disconnect) {
				break;
			}
			waitBeforeReconnect();
		}
	} while (_reconnect && !_disconnect);
	_connecting = false;
}

bool MqttConnection::SendMessage(const std::string &topic,
				 const std::string &payload, int qos,
				 bool retained)
{
	std::scoped_lock<std::mutex> lock(_clientMtx);

	if (!_client || !_client->is_connected()) {
		blog(LOG_WARNING,
		     "Cannot send message: MQTT client \"%s\" is not connected",
		     _name.c_str());
		Connect();
		return false;
	}

	try {
		auto msg = mqtt::make_message(topic, payload);
		msg->set_qos(qos);
		msg->set_retained(retained);
		_client->publish(msg);
		vblog(LOG_INFO, "Sent MQTT message on \"%s\": %s",
		      topic.c_str(), payload.c_str());
		return true;
	} catch (const mqtt::exception &e) {
		blog(LOG_WARNING, "MQTT send error for \"%s\": %s",
		     _name.c_str(), e.what());
		return false;
	}
}

void MqttConnection::Load(obs_data_t *data)
{
	Item::Load(data);
	_uri = obs_data_get_string(data, "uri");
	_username = obs_data_get_string(data, "username");
	_password = obs_data_get_string(data, "password");
	_trustStore = obs_data_get_string(data, "trustStore");
	_keyStore = obs_data_get_string(data, "keyStore");
	_privateKey = obs_data_get_string(data, "privateKey");
	_verifyServerCert = obs_data_get_bool(data, "verifyServerCert");
	_connectOnStart = obs_data_get_bool(data, "connectOnStart");
	_reconnect = obs_data_get_bool(data, "reconnect");
	_reconnectDelay = obs_data_get_int(data, "reconnectDelay");

	_topics.clear();
	OBSDataArrayAutoRelease array = obs_data_get_array(data, "topics");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease obj = obs_data_array_item(array, i);
		_topics.push_back(obs_data_get_string(obj, "topic"));
	}

	_qos.clear();
	array = obs_data_get_array(data, "qos");
	count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease obj = obs_data_array_item(array, i);
		_qos.push_back(obs_data_get_int(obj, "qos"));
	}

	if (ConnectOnStartup()) {
		Connect();
	}
}

void MqttConnection::Save(obs_data_t *data) const
{
	Item::Save(data);
	obs_data_set_string(data, "uri", _uri.c_str());
	obs_data_set_string(data, "username", _username.c_str());
	obs_data_set_string(data, "password", _password.c_str());
	obs_data_set_string(data, "trustStore", _trustStore.c_str());
	obs_data_set_string(data, "keyStore", _keyStore.c_str());
	obs_data_set_string(data, "privateKey", _privateKey.c_str());
	obs_data_set_bool(data, "verifyServerCert", _verifyServerCert);
	obs_data_set_bool(data, "connectOnStart", _connectOnStart);
	obs_data_set_bool(data, "reconnect", _reconnect);
	obs_data_set_int(data, "reconnectDelay", _reconnectDelay);

	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &topic : _topics) {
		OBSDataAutoRelease obj = obs_data_create();
		obs_data_set_string(obj, "topic", topic.c_str());
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(data, "topics", array);

	array = obs_data_array_create();
	for (const int qos : _qos) {
		OBSDataAutoRelease obj = obs_data_create();
		obs_data_set_int(obj, "qos", qos);
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(data, "qos", array);
}

MqttMessageBuffer MqttConnection::RegisterForEvents()
{
	return _dispatcher.RegisterClient();
}

QString MqttConnection::GetStatus() const
{
	if (_connected) {
		return obs_module_text(
			"AdvSceneSwitcher.mqttConnection.status.connected");
	}
	if (_connecting) {
		return obs_module_text(
			"AdvSceneSwitcher.mqttConnection.status.connecting");
	}
	if (_lastError.empty()) {
		return obs_module_text(
			"AdvSceneSwitcher.mqttConnection.status.disconnected");
	}
	QString status(obs_module_text(
		"AdvSceneSwitcher.mqttConnection.status.disconnected"));
	status += " (" + QString::fromStdString(_lastError) + ")";
	return status;
}

MqttConnectionSettingsDialog::MqttConnectionSettingsDialog(
	QWidget *parent, const MqttConnection &connection)
	: ItemSettingsDialog(connection, GetMqttConnections(),
			     "AdvSceneSwitcher.mqttConnection.select",
			     "AdvSceneSwitcher.mqttConnection.add",
			     "AdvSceneSwitcher.item.nameNotAvailable", true,
			     parent),
	  _uri(new QLineEdit()),
	  _username(new QLineEdit()),
	  _password(new QLineEdit()),
	  _showPassword(new QPushButton()),
	  _trustStore(new FileSelection()),
	  _keyStore(new FileSelection()),
	  _privateKey(new FileSelection()),
	  _verifyServerCert(new QCheckBox()),
	  _topics(new MqttTopicListWidget(this)),
	  _connectOnStart(new QCheckBox()),
	  _reconnect(new QCheckBox()),
	  _reconnectDelay(new QSpinBox()),
	  _status(new QLabel()),
	  _test(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.mqttConnection.test"))),
	  _layout(new QGridLayout())
{
	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");
	_uri->setText(QString::fromStdString(connection._uri));
	_username->setText(QString::fromStdString(connection._username));
	_password->setText(QString::fromStdString(connection._password));
	_trustStore->SetPath(QString::fromStdString(connection._trustStore));
	_keyStore->SetPath(QString::fromStdString(connection._keyStore));
	_privateKey->SetPath(QString::fromStdString(connection._privateKey));
	_verifyServerCert->setChecked(connection._verifyServerCert);
	_topics->SetValues(connection._topics, connection._qos);
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
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.mqttConnection.name")),
			   row, 0);
	auto nameLayout = new QHBoxLayout();
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	_layout->addLayout(nameLayout, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.mqttConnection.address")),
			   row, 0);
	_layout->addWidget(_uri, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.mqttConnection.username")),
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
			"AdvSceneSwitcher.mqttConnection.trustStore")),
		row, 0);
	auto trustStoreLayout = new QHBoxLayout();
	trustStoreLayout->addWidget(_trustStore);
	trustStoreLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.mqttConnection.trustStore.help")));
	_layout->addLayout(trustStoreLayout, row, 1);
	++row;
	_layout->addWidget(new QLabel(obs_module_text(
				   "AdvSceneSwitcher.mqttConnection.keyStore")),
			   row, 0);
	auto keyStoreLayout = new QHBoxLayout();
	keyStoreLayout->addWidget(_keyStore);
	keyStoreLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.mqttConnection.keyStore.help")));
	_layout->addLayout(keyStoreLayout, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.mqttConnection.privateKey")),
		row, 0);
	auto privateKeyLayout = new QHBoxLayout();
	privateKeyLayout->addWidget(_privateKey);
	privateKeyLayout->addWidget(new HelpIcon(obs_module_text(
		"AdvSceneSwitcher.mqttConnection.privateKey.help")));
	_layout->addLayout(privateKeyLayout, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.mqttConnection.verifyServerCert")),
		row, 0);
	_layout->addWidget(_verifyServerCert, row, 1);
	++row;
	_layout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.mqttConnection.topics")));
	++row;
	_layout->addWidget(_topics, row, 0, 1, -1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.mqttConnection.connectOnStart")),
		row, 0);
	_layout->addWidget(_connectOnStart, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.mqttConnection.reconnect")),
		row, 0);
	_layout->addWidget(_reconnect, row, 1);
	++row;
	_layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.mqttConnection.reconnectDelay")),
		row, 0);
	_layout->addWidget(_reconnectDelay, row, 1);
	++row;
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
	connection._trustStore = dialog._trustStore->GetPath().toStdString();
	connection._keyStore = dialog._keyStore->GetPath().toStdString();
	connection._privateKey = dialog._privateKey->GetPath().toStdString();
	connection._verifyServerCert = dialog._verifyServerCert->isChecked();
	connection._topics = dialog._topics->GetTopics();
	connection._qos = dialog._topics->GetQoS();
	connection._connectOnStart = dialog._connectOnStart->isChecked();
	connection._reconnect = dialog._reconnect->isChecked();
	connection._reconnectDelay = dialog._reconnectDelay->value();
	if (connection._connecting) {
		connection.Disconnect();
	}
	connection.Connect();
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
	if (_updateStatusTimer) {
		_updateStatusTimer->stop();
		_updateStatusTimer->deleteLater();
	}

	auto connection = std::make_shared<MqttConnection>();
	connection->_name = _name->text().toStdString();
	connection->_uri = _uri->text().toStdString();
	connection->_username = _username->text().toStdString();
	connection->_password = _password->text().toStdString();
	connection->_trustStore = _trustStore->GetPath().toStdString();
	connection->_keyStore = _keyStore->GetPath().toStdString();
	connection->_privateKey = _privateKey->GetPath().toStdString();
	connection->_verifyServerCert = _verifyServerCert->isChecked();
	connection->_topics = _topics->GetTopics();
	connection->_qos = _topics->GetQoS();
	connection->_connectOnStart = false;
	connection->_reconnect = false;
	connection->_reconnectDelay = _reconnectDelay->value();
	connection->Connect();

	_updateStatusTimer = new QTimer(this);
	_updateStatusTimer->setInterval(300);
	QObject::connect(_updateStatusTimer, &QTimer::timeout,
			 [this, connection]() {
				 _status->setText(connection->GetStatus());
			 });
	_updateStatusTimer->start();
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
			"AdvSceneSwitcher.mqttConnection.select",
			"AdvSceneSwitcher.mqttConnection.add",
			"AdvSceneSwitcher.item.nameNotAvailable",
			"AdvSceneSwitcher.mqttConnection.configure", parent)
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
	: QObject(parent)
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
		connection->Connect();
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

std::string
GetWeakMqttConnectionName(const std::weak_ptr<MqttConnection> &connection_)
{
	auto connection = connection_.lock();
	if (!connection) {
		return obs_module_text(
			"AdvSceneSwitcher.mqttConnection.invalid");
	}
	return connection->Name();
}

static void SaveMqttConnections(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &connection : GetMqttConnections()) {
		OBSDataAutoRelease obj = obs_data_create();
		connection->Save(obj);
		obs_data_array_push_back(array, obj);
	}
	obs_data_set_array(obj, "mqttConnections", array);
}

static void LoadMqttConnections(obs_data_t *obj)
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

static bool setup()
{
	AddSaveStep(SaveMqttConnections);
	AddLoadStep(LoadMqttConnections);
	return true;
}

static bool _ = setup();

} // namespace advss
