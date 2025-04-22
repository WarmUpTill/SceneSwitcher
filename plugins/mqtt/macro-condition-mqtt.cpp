#include "macro-condition-mqtt.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "ui-helpers.hpp"

namespace advss {

const std::string MacroConditionMqtt::id = "mqtt";

bool MacroConditionMqtt::_registered = MacroConditionFactory::Register(
	MacroConditionMqtt::id,
	{MacroConditionMqtt::Create, MacroConditionMqttEdit::Create,
	 "AdvSceneSwitcher.condition.mqtt"});

bool MacroConditionMqtt::CheckCondition()
{
	if (!_messageBuffer) {
		return false;
	}

	const bool macroWasPausedSinceLastCheck =
		MacroWasPausedSince(GetMacro(), _lastCheck);
	_lastCheck = std::chrono::high_resolution_clock::now();
	if (macroWasPausedSinceLastCheck) {
		_messageBuffer->Clear();
		return false;
	}

	const auto messageMatches = [this](const std::string &message) -> bool {
		if (_regex.Enabled()) {
			return _regex.Matches(message, _message);
		}
		return message != std::string(_message);
	};

	while (!_messageBuffer->Empty()) {
		auto message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}

		if (!messageMatches(*message)) {
			continue;
		}

		SetTempVarValue("message", *message);
		if (_clearBufferOnMatch) {
			_messageBuffer->Clear();
		}
		return true;
	}

	return false;
}

bool MacroConditionMqtt::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_message.Save(obj, "message");
	_regex.Save(obj);
	obs_data_set_string(obj, "connection",
			    GetWeakMqttConnectionName(_connection).c_str());
	obs_data_set_bool(obj, "clearBufferOnMatch", _clearBufferOnMatch);
	return true;
}

bool MacroConditionMqtt::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_message.Load(obj, "message");
	_regex.Load(obj);
	_clearBufferOnMatch = obs_data_get_bool(obj, "clearBufferOnMatch");
	SetConnection(obs_data_get_string(obj, "connection"));
	return true;
}

std::string MacroConditionMqtt::GetShortDesc() const
{
	return GetWeakMqttConnectionName(_connection);
}

void MacroConditionMqtt::SetConnection(const std::string &name)
{
	_connection = GetWeakMqttConnectionByName(name);
	auto connection = _connection.lock();
	if (!connection) {
		return;
	}
	_messageBuffer = connection->RegisterForEvents();
}

std::weak_ptr<MqttConnection> MacroConditionMqtt::GetConnection() const
{
	return _connection;
}

void MacroConditionMqtt::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("message",
		   obs_module_text("AdvSceneSwitcher.tempVar.mqtt.message"));
}

MacroConditionMqttEdit::MacroConditionMqttEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMqtt> entryData)
	: QWidget(parent),
	  _connection(new MqttConnectionSelection(this)),
	  _message(new VariableTextEdit(this, 5, 1, 1)),
	  _regex(new RegexConfigWidget(parent)),
	  _listen(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.mqttConnection.startListen"))),
	  _clearBufferOnMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.clearBufferOnMatch")))
{
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MqttMessageChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));
	QWidget::connect(_listen, SIGNAL(clicked()), this,
			 SLOT(ToggleListen()));
	QWidget::connect(_clearBufferOnMatch, SIGNAL(stateChanged(int)), this,
			 SLOT(ClearBufferOnMatchChanged(int)));
	QWidget::connect(&_listenTimer, SIGNAL(timeout()), this,
			 SLOT(SetMessageSelectionToLastReceived()));

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.mqtt.layout.match"),
		entryLayout,
		{{"{{connection}}", _connection}, {"{{regex}}", _regex}});
	auto listenLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.mqtt.layout.listen"),
		     listenLayout, {{"{{listenButton}}", _listen}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_message);
	mainLayout->addLayout(listenLayout);
	mainLayout->addWidget(_clearBufferOnMatch);
	setLayout(mainLayout);

	_listenTimer.setInterval(100);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

MacroConditionMqttEdit::~MacroConditionMqttEdit()
{
	EnableListening(false);
}

void MacroConditionMqttEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_message->setPlainText(_entryData->_message);
	_connection->SetConnection(_entryData->GetConnection());
	_regex->SetRegexConfig(_entryData->_regex);
	_clearBufferOnMatch->setChecked(_entryData->_clearBufferOnMatch);

	adjustSize();
	updateGeometry();
}

void MacroConditionMqttEdit::ConnectionSelectionChanged(
	const QString &connection)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetConnection(connection.toStdString());
	emit(HeaderInfoChanged(connection));
}

void MacroConditionMqttEdit::MqttMessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_message = _message->toPlainText().toStdString();
}

void MacroConditionMqttEdit::ClearBufferOnMatchChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_clearBufferOnMatch = value;
}

void MacroConditionMqttEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionMqttEdit::EnableListening(bool enable)
{
	if (_currentlyListening == enable) {
		return;
	}
	if (enable) {
		auto connectionItem = _connection->GetCurrentItem();
		if (!connectionItem) {
			return;
		}
		auto weakConnection = GetWeakMqttConnectionByQString(
			QString::fromStdString(connectionItem->Name()));
		auto connection = weakConnection.lock();
		if (!connection) {
			return;
		}
		_messageBuffer = connection->RegisterForEvents();
		_listenTimer.start();
	} else {
		_messageBuffer.reset();
		_listenTimer.stop();
	}
}

void MacroConditionMqttEdit::ToggleListen()
{
	if (!_entryData) {
		return;
	}

	_listen->setText(
		_currentlyListening
			? obs_module_text(
				  "AdvSceneSwitcher.mqttConnection.startListen")
			: obs_module_text(
				  "AdvSceneSwitcher.mqttConnection.stopListen"));
	EnableListening(!_currentlyListening);
	_currentlyListening = !_currentlyListening;
	_message->setDisabled(_currentlyListening);
}

void MacroConditionMqttEdit::SetMessageSelectionToLastReceived()
{
	auto lock = LockContext();
	if (!_entryData || !_messageBuffer || _messageBuffer->Empty()) {
		return;
	}

	std::optional<std::string> message;
	while (!_messageBuffer->Empty()) {
		message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}
	}

	if (!message) {
		return;
	}

	const QSignalBlocker blocker(_message);
	_message->setPlainText(*message);
	_entryData->_message = *message;
}

} // namespace advss
