#include "macro-action-mqtt.hpp"
#include "layout-helpers.hpp"
#include "mqtt-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroActionMqtt::id = "mqtt";

bool MacroActionMqtt::_registered = MacroActionFactory::Register(
	MacroActionMqtt::id,
	{MacroActionMqtt::Create, MacroActionMqttEdit::Create,
	 "AdvSceneSwitcher.action.mqtt"});

bool MacroActionMqtt::PerformAction()
{
	auto connection = _connection.lock();
	if (!connection) {
		return true;
	}

	int qos = _qos;
	if (qos > 2 || qos < 0) {
		qos = 1;
		blog(LOG_WARNING,
		     "%s: use QoS 1 isntead of invalid QoS value %d", __func__,
		     (int)_qos);
	}

	(void)connection->SendMessage(_topic, _message, qos, _retain);
	return true;
}

void MacroActionMqtt::LogAction() const
{
	ablog(LOG_INFO,
	      "send MQTT message to \"%s\" (topic: \"%s\" - payload: \"%s\" - qos: %d - retain: %d)",
	      GetWeakMqttConnectionName(_connection).c_str(), _topic.c_str(),
	      _message.c_str(), (int)_qos, _retain);
}

bool MacroActionMqtt::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "connection",
			    GetWeakMqttConnectionName(_connection).c_str());
	_message.Save(obj, "message");
	_topic.Save(obj, "topic");
	_qos.Save(obj, "qos");
	obs_data_set_bool(obj, "retain", _retain);
	return true;
}

bool MacroActionMqtt::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	SetConnection(obs_data_get_string(obj, "connection"));
	_message.Load(obj, "message");
	_topic.Load(obj, "topic");
	_qos.Load(obj, "qos");
	_retain = obs_data_get_bool(obj, "retain");
	return true;
}

void MacroActionMqtt::SetConnection(const std::string &name)
{
	_connection = GetWeakMqttConnectionByName(name);
}

std::weak_ptr<MqttConnection> MacroActionMqtt::GetConnection() const
{
	return _connection;
}

std::string MacroActionMqtt::GetShortDesc() const
{
	return GetWeakMqttConnectionName(_connection);
}

void MacroActionMqtt::ResolveVariablesToFixedValues()
{
	_message.ResolveVariables();
	_topic.ResolveVariables();
	_qos.ResolveVariables();
}

std::shared_ptr<MacroAction> MacroActionMqtt::Create(Macro *m)
{
	return std::make_shared<MacroActionMqtt>(m);
}

std::shared_ptr<MacroAction> MacroActionMqtt::Copy() const
{
	return std::make_shared<MacroActionMqtt>(*this);
}

MacroActionMqttEdit::MacroActionMqttEdit(
	QWidget *parent, std::shared_ptr<MacroActionMqtt> entryData)
	: QWidget(parent),
	  _connection(new MqttConnectionSelection(this)),
	  _message(new VariableTextEdit(this, 5, 1, 1)),
	  _topic(new VariableLineEdit(this)),
	  _qos(new VariableSpinBox(this)),
	  _retain(new QCheckBox(this))
{
	_qos->setMinimum(0);
	_qos->setMaximum(2);

	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MqttMessageChanged()));
	QWidget::connect(_topic, SIGNAL(editingFinished()), this,
			 SLOT(MqttTopicChanged()));
	QWidget::connect(
		_qos,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(QoSChanged(const NumberVariable<int> &)));
	QWidget::connect(_retain, SIGNAL(stateChanged(int)), this,
			 SLOT(RetainChanged(int)));

	auto sendLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.mqtt.layout"),
		     sendLayout, {{"{{connection}}", _connection}});

	int row = 0;
	auto grid = new QGridLayout;
	grid->addWidget(new QLabel(obs_module_text(
				"AdvSceneSwitcher.action.mqtt.topic")),
			row, 0);
	grid->addWidget(_topic, row, 1);
	row++;
	grid->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.action.mqtt.qos")),
		row, 0);
	grid->addWidget(_qos, row, 1);
	row++;
	grid->addWidget(new QLabel(obs_module_text(
				"AdvSceneSwitcher.action.mqtt.retain")),
			row, 0);
	grid->addWidget(_retain, row, 1);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(sendLayout);
	mainLayout->addWidget(_message);
	mainLayout->addLayout(grid);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionMqttEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_connection->SetConnection(_entryData->GetConnection());
	_message->setPlainText(_entryData->_message);
	_topic->setText(_entryData->_topic);
	_qos->SetValue(_entryData->_qos);
	_retain->setChecked(_entryData->_retain);

	adjustSize();
	updateGeometry();
}

void MacroActionMqttEdit::ConnectionSelectionChanged(const QString &connection)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetConnection(connection.toStdString());
	emit(HeaderInfoChanged(connection));
}

void MacroActionMqttEdit::MqttMessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_message = _message->toPlainText().toStdString();
}

void MacroActionMqttEdit::MqttTopicChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_topic = _topic->text().toStdString();
}

void MacroActionMqttEdit::QoSChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_qos = value;
}

void MacroActionMqttEdit::RetainChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_retain = value;
}

} // namespace advss
