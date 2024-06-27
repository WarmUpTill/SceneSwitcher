#include "macro-condition-websocket.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"

#include <regex>

namespace advss {

const std::string MacroConditionWebsocket::id = "websocket";

bool MacroConditionWebsocket::_registered = MacroConditionFactory::Register(
	MacroConditionWebsocket::id,
	{MacroConditionWebsocket::Create, MacroConditionWebsocketEdit::Create,
	 "AdvSceneSwitcher.condition.websocket"});

const static std::map<MacroConditionWebsocket::Type, std::string>
	conditionTypes = {
		{MacroConditionWebsocket::Type::REQUEST,
		 "AdvSceneSwitcher.condition.websocket.type.request"},
		{MacroConditionWebsocket::Type::EVENT,
		 "AdvSceneSwitcher.condition.websocket.type.event"},
};

MacroConditionWebsocket::MacroConditionWebsocket(Macro *m)
	: MacroCondition(m, true)
{
	_messageBuffer = RegisterForWebsocketMessages();
}

bool MacroConditionWebsocket::CheckCondition()
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

	while (!_messageBuffer->Empty()) {
		auto message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}
		if (_regex.Enabled()) {
			if (!_regex.Matches(*message, _message)) {
				continue;
			}

			SetTempVarValue("message", *message);
			SetVariableValue(*message);
			if (_clearBufferOnMatch) {
				_messageBuffer->Clear();
			}
			return true;
		} else {
			if (*message != std::string(_message)) {
				continue;
			}

			SetTempVarValue("message", *message);
			SetVariableValue(*message);
			if (_clearBufferOnMatch) {
				_messageBuffer->Clear();
			}
			return true;
		}
	}
	SetVariableValue("");
	return false;
}

bool MacroConditionWebsocket::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	_message.Save(obj, "message");
	_regex.Save(obj);
	obs_data_set_string(obj, "connection",
			    GetWeakConnectionName(_connection).c_str());
	obs_data_set_bool(obj, "clearBufferOnMatch", _clearBufferOnMatch);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionWebsocket::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_message.Load(obj, "message");
	_regex.Load(obj);
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "useRegex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "useRegex"), false);
	}
	_connection =
		GetWeakConnectionByName(obs_data_get_string(obj, "connection"));

	_clearBufferOnMatch = obs_data_get_bool(obj, "clearBufferOnMatch");
	if (!obs_data_has_user_value(obj, "version")) {
		_clearBufferOnMatch = true;
	}

	SetType(_type);
	return true;
}

std::string MacroConditionWebsocket::GetShortDesc() const
{
	if (_type == Type::REQUEST) {
		return "";
	}
	return GetWeakConnectionName(_connection);
}

void MacroConditionWebsocket::SetType(Type type)
{
	_type = type;
	if (_type == Type::REQUEST) {
		_messageBuffer = RegisterForWebsocketMessages();
		return;
	}

	auto connection = _connection.lock();
	if (!connection) {
		return;
	}
	_messageBuffer = connection->RegisterForEvents();
}

void MacroConditionWebsocket::SetConnection(const std::string &connectionName)
{
	_connection = GetWeakConnectionByName(connectionName);
	if (_type == Type::REQUEST) {
		// This should not really happen, but let's be safe
		return;
	}

	auto connection = _connection.lock();
	if (!connection) {
		return;
	}
	_messageBuffer = connection->RegisterForEvents();
}

std::weak_ptr<WSConnection> MacroConditionWebsocket::GetConnection() const
{
	return _connection;
}

void MacroConditionWebsocket::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"message",
		obs_module_text("AdvSceneSwitcher.tempVar.websocket.message"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.websocket.message.description"));
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionWebsocketEdit::MacroConditionWebsocketEdit(
	QWidget *parent, std::shared_ptr<MacroConditionWebsocket> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox(this)),
	  _message(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _connection(new WSConnectionSelection(this)),
	  _clearBufferOnMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.clearBufferOnMatch"))),
	  _editLayout(new QHBoxLayout())
{
	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));
	QWidget::connect(_clearBufferOnMatch, SIGNAL(stateChanged(int)), this,
			 SLOT(ClearBufferOnMatchChanged(int)));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_editLayout);
	mainLayout->addWidget(_message);
	auto regexLayout = new QHBoxLayout;
	regexLayout->addWidget(_regex);
	regexLayout->addStretch();
	regexLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(regexLayout);
	mainLayout->addWidget(_clearBufferOnMatch);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionWebsocketEdit::SetupRequestEdit()
{
	_editLayout->removeWidget(_conditions);
	_editLayout->removeWidget(_connection);
	ClearLayout(_editLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _conditions},
		{"{{connection}}", _connection},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.websocket.entry.request"),
		_editLayout, widgetPlaceholders);
	_connection->hide();
}

void MacroConditionWebsocketEdit::SetupEventEdit()
{
	_editLayout->removeWidget(_conditions);
	_editLayout->removeWidget(_connection);
	ClearLayout(_editLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _conditions},
		{"{{connection}}", _connection},
	};
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.websocket.entry.event"),
		_editLayout, widgetPlaceholders);
	_connection->show();
}

void MacroConditionWebsocketEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->GetType()));
	_message->setPlainText(_entryData->_message);
	_regex->SetRegexConfig(_entryData->_regex);
	_connection->SetConnection(_entryData->GetConnection());
	_clearBufferOnMatch->setChecked(_entryData->_clearBufferOnMatch);

	if (_entryData->GetType() == MacroConditionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::ConditionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetType(static_cast<MacroConditionWebsocket::Type>(index));
	if (_entryData->GetType() == MacroConditionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionWebsocketEdit::MessageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_message = _message->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::ConnectionSelectionChanged(
	const QString &connection)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetConnection(connection.toStdString());
	emit(HeaderInfoChanged(connection));
}

void MacroConditionWebsocketEdit::ClearBufferOnMatchChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_clearBufferOnMatch = value;
}

void MacroConditionWebsocketEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

} // namespace advss
