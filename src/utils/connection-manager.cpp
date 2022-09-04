#include "connection-manager.hpp"
#include "utility.hpp"
#include "name-dialog.hpp"
#include "switcher-data-structs.hpp"

#include <algorithm>
#include <QAction>
#include <QMenu>
#include <QLayout>
#include <obs-module.h>

Q_DECLARE_METATYPE(Connection *);

void SwitcherData::saveConnections(obs_data_t *obj)
{
	obs_data_array_t *connectionArray = obs_data_array_create();
	for (Connection &c : connections) {
		obs_data_t *array_obj = obs_data_create();
		c.Save(array_obj);
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
		connections.emplace_back();
		connections.back().Load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(connectionArray);
}

Connection::Connection(std::string name, std::string address, uint64_t port,
		       std::string pass, bool connectOnStart, bool reconnect,
		       int reconnectDelay)
	: _name(name),
	  _address(address),
	  _port(port),
	  _password(pass),
	  _connectOnStart(connectOnStart),
	  _reconnect(reconnect),
	  _reconnectDelay(reconnectDelay)
{
}

Connection::Connection(const Connection &other)
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
		if (con.GetName() == name) {
			return &con;
		}
	}
	return nullptr;
}

bool ConnectionNameAvailable(const QString &name)
{
	return !GetConnectionByName(name);
}

bool ConnectionNameAvailable(const std::string &name)
{
	return ConnectionNameAvailable(QString::fromStdString(name));
}

void setButtonIcon(QPushButton *button, const char *path)
{
	QIcon icon;
	icon.addFile(QString::fromUtf8(path), QSize(), QIcon::Normal,
		     QIcon::Off);
	button->setIcon(icon);
}

ConnectionSelection::ConnectionSelection(QWidget *parent)
	: QWidget(parent), _selection(new QComboBox), _modify(new QPushButton)
{
	_modify->setMaximumWidth(22);
	setButtonIcon(_modify, ":/settings/images/settings/general.svg");
	_modify->setFlat(true);

	// Connect to slots
	QWidget::connect(_selection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ChangeSelection(const QString &)));
	QWidget::connect(_modify, SIGNAL(clicked()), this,
			 SLOT(ModifyButtonClicked()));
	QWidget::connect(
		window(),
		SIGNAL(ConnectionRenamed(const QString &, const QString &)),
		this, SLOT(RenameConnection(const QString &, const QString &)));
	QWidget::connect(window(), SIGNAL(ConnectionAdded(const QString &)),
			 this, SLOT(AddConnection(const QString &)));
	QWidget::connect(window(), SIGNAL(ConnectionRemoved(const QString &)),
			 this, SLOT(RemoveConnection(const QString &)));

	// Forward signals
	QWidget::connect(
		this,
		SIGNAL(ConnectionRenamed(const QString &, const QString &)),
		window(),
		SIGNAL(ConnectionRenamed(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ConnectionAdded(const QString &)),
			 window(), SIGNAL(ConnectionAdded(const QString &)));
	QWidget::connect(this, SIGNAL(ConnectionRemoved(const QString &)),
			 window(), SIGNAL(ConnectionRemoved(const QString &)));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(_selection);
	layout->addWidget(_modify);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	for (const auto &c : switcher->connections) {
		_selection->addItem(QString::fromStdString(c._name));
	}
	_selection->model()->sort(0);
	addSelectionEntry(
		_selection,
		obs_module_text("AdvSceneSwitcher.connection.select"));
	_selection->insertSeparator(_selection->count());
	_selection->addItem(obs_module_text("AdvSceneSwitcher.connection.add"));
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

void ConnectionSelection::ChangeSelection(const QString &sel)
{
	if (sel == obs_module_text("AdvSceneSwitcher.connection.add")) {
		Connection connection;
		bool accepted = ConnectionSettingsDialog::AskForSettings(
			this, connection);
		if (!accepted) {
			_selection->setCurrentIndex(0);
			return;
		}
		switcher->connections.emplace_back(connection);
		const QSignalBlocker b(_selection);
		const QString name = QString::fromStdString(connection._name);
		AddConnection(name);
		_selection->setCurrentText(name);
		emit ConnectionAdded(name);
		emit SelectionChanged(name);
		return;
	}
	auto connection = GetCurrentConnection();
	if (connection) {
		emit SelectionChanged(
			QString::fromStdString(connection->_name));
	} else {
		emit SelectionChanged("");
	}
}

void ConnectionSelection::ModifyButtonClicked()
{
	auto connection = GetCurrentConnection();
	if (!connection) {
		return;
	}
	auto properties = [&]() {
		const auto oldName = connection->_name;
		bool accepted = ConnectionSettingsDialog::AskForSettings(
			this, *connection);
		if (!accepted) {
			return;
		}
		if (oldName != connection->_name) {
			emit ConnectionRenamed(
				QString::fromStdString(oldName),
				QString::fromStdString(connection->_name));
		}
	};

	QMenu menu(this);

	QAction *action = new QAction(
		obs_module_text("AdvSceneSwitcher.connection.rename"), &menu);
	connect(action, SIGNAL(triggered()), this, SLOT(RenameConnection()));
	action->setProperty("connetion", QVariant::fromValue(connection));
	menu.addAction(action);

	action = new QAction(
		obs_module_text("AdvSceneSwitcher.connection.remove"), &menu);
	connect(action, SIGNAL(triggered()), this, SLOT(RemoveConnection()));
	menu.addAction(action);

	action = new QAction(
		obs_module_text("AdvSceneSwitcher.connection.properties"),
		&menu);
	connect(action, &QAction::triggered, properties);
	menu.addAction(action);

	menu.exec(QCursor::pos());
}

void ConnectionSelection::RenameConnection()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	QVariant variant = action->property("connetion");
	Connection *connection = variant.value<Connection *>();

	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.windowTitle"),
		obs_module_text("AdvSceneSwitcher.connection.newName"), name,
		QString::fromStdString(name));
	if (!accepted) {
		return;
	}
	if (name.empty()) {
		DisplayMessage("AdvSceneSwitcher.connection.emptyName");
		return;
	}
	if (_selection->currentText().toStdString() != name &&
	    !ConnectionNameAvailable(name)) {
		DisplayMessage("AdvSceneSwitcher.connection.nameNotAvailable");
		return;
	}

	const auto oldName = connection->_name;
	connection->_name = name;
	emit ConnectionRenamed(QString::fromStdString(oldName),
			       QString::fromStdString(name));
}

void ConnectionSelection::RenameConnection(const QString &oldName,
					   const QString &name)
{
	int idx = _selection->findText(oldName);
	if (idx == -1) {
		return;
	}
	_selection->setItemText(idx, name);
}

void ConnectionSelection::AddConnection(const QString &name)
{
	if (_selection->findText(name) == -1) {
		_selection->insertItem(1, name);
	}
}

void ConnectionSelection::RemoveConnection()
{
	auto connection = GetCurrentConnection();
	if (!connection) {
		return;
	}

	int idx =
		_selection->findText(QString::fromStdString(connection->_name));
	if (idx == -1 || idx == _selection->count()) {
		return;
	}

	emit ConnectionRemoved(QString::fromStdString(connection->_name));

	for (auto it = switcher->connections.begin();
	     it != switcher->connections.end(); ++it) {
		if (it->_name == connection->_name) {
			switcher->connections.erase(it);
			break;
		}
	}
}

void ConnectionSelection::RemoveConnection(const QString &name)
{
	const int idx = _selection->findText(name);
	if (idx == _selection->currentIndex()) {
		_selection->setCurrentIndex(0);
	}
	_selection->removeItem(idx);
}

Connection *ConnectionSelection::GetCurrentConnection()
{
	return GetConnectionByName(_selection->currentText());
}

ConnectionSettingsDialog::ConnectionSettingsDialog(QWidget *parent,
						   const Connection &settings)
	: QDialog(parent),
	  _name(new QLineEdit()),
	  _nameHint(new QLabel),
	  _address(new QLineEdit()),
	  _port(new QSpinBox()),
	  _password(new QLineEdit()),
	  _showPassword(new QPushButton()),
	  _connectOnStart(new QCheckBox()),
	  _reconnect(new QCheckBox()),
	  _reconnectDelay(new QSpinBox()),
	  _test(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.connection.test"))),
	  _status(new QLabel()),
	  _buttonbox(new QDialogButtonBox(QDialogButtonBox::Ok |
					  QDialogButtonBox::Cancel))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);

	_port->setMaximum(65535);
	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");
	_reconnectDelay->setMaximum(9999);
	_reconnectDelay->setSuffix("s");
	_buttonbox->setCenterButtons(true);
	_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(true);

	_name->setText(QString::fromStdString(settings._name));
	_address->setText(QString::fromStdString(settings._address));
	_port->setValue(settings._port);
	_password->setText(QString::fromStdString(settings._password));
	_connectOnStart->setChecked(settings._connectOnStart);
	_reconnect->setChecked(settings._reconnect);
	_reconnectDelay->setValue(settings._reconnectDelay);

	QWidget::connect(_name, SIGNAL(textEdited(const QString &)), this,
			 SLOT(NameChanged(const QString &)));
	QWidget::connect(_reconnect, SIGNAL(stateChanged(int)), this,
			 SLOT(ReconnectChanged(int)));
	QWidget::connect(_showPassword, SIGNAL(pressed()), this,
			 SLOT(ShowPassword()));
	QWidget::connect(_showPassword, SIGNAL(released()), this,
			 SLOT(HidePassword()));
	QWidget::connect(_test, SIGNAL(clicked()), this,
			 SLOT(TestConnection()));

	QWidget::connect(_buttonbox, &QDialogButtonBox::accepted, this,
			 &QDialog::accept);
	QWidget::connect(_buttonbox, &QDialogButtonBox::rejected, this,
			 &QDialog::reject);

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

	NameChanged(_name->text());
	ReconnectChanged(_reconnect->isChecked());
	HidePassword();
}

void ConnectionSettingsDialog::NameChanged(const QString &text)
{

	if (text != _name->text() && !ConnectionNameAvailable(text)) {
		SetNameWarning(obs_module_text(
			"AdvSceneSwitcher.connection.nameNotAvailable"));
		return;
	}
	if (text.isEmpty()) {
		SetNameWarning(obs_module_text(
			"AdvSceneSwitcher.connection.emptyName"));
		return;
	}
	if (text == obs_module_text("AdvSceneSwitcher.connection.select") ||
	    text == obs_module_text("AdvSceneSwitcher.connection.add")) {
		SetNameWarning(obs_module_text(
			"AdvSceneSwitcher.connection.nameReserved"));
		return;
	}
	SetNameWarning("");
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

void ConnectionSettingsDialog::SetNameWarning(const QString warn)
{
	if (warn.isEmpty()) {
		_nameHint->hide();
		_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(false);
		return;
	}
	_nameHint->setText(warn);
	_nameHint->show();
	_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(true);
}

void Connection::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "name");
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

void Connection::Save(obs_data_t *obj)
{
	obs_data_set_string(obj, "name", _name.c_str());
	obs_data_set_string(obj, "address", _address.c_str());
	obs_data_set_int(obj, "port", _port);
	obs_data_set_string(obj, "password", _password.c_str());
	obs_data_set_bool(obj, "connectOnStart", _connectOnStart);
	obs_data_set_bool(obj, "reconnect", _reconnect);
	obs_data_set_int(obj, "reconnectDelay", _reconnectDelay);
}
