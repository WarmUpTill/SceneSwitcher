#include "macro-action-websocket.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroActionWebsocket::id = "websocket";

bool MacroActionWebsocket::_registered = MacroActionFactory::Register(
	MacroActionWebsocket::id,
	{MacroActionWebsocket::Create, MacroActionWebsocketEdit::Create,
	 "AdvSceneSwitcher.action.websocket"});

const static std::map<MacroActionWebsocket::API, std::string> apiTypes = {
	{MacroActionWebsocket::API::SCENE_SWITCHER,
	 "AdvSceneSwitcher.action.websocket.api.sceneSwitcher"},
	{MacroActionWebsocket::API::OBS_WEBSOCKET,
	 "AdvSceneSwitcher.action.websocket.api.obsWebsocket"},
	{MacroActionWebsocket::API::GENERIC_WEBSOCKET,
	 "AdvSceneSwitcher.action.websocket.api.genericWebsocket"},
};

const static std::map<MacroActionWebsocket::MessageType, std::string>
	messageTypes = {
		{MacroActionWebsocket::MessageType::REQUEST,
		 "AdvSceneSwitcher.action.websocket.type.request"},
		{MacroActionWebsocket::MessageType::EVENT,
		 "AdvSceneSwitcher.action.websocket.type.event"},
};

void MacroActionWebsocket::SendRequest(const std::string &msg)
{
	auto connection = _connection.lock();
	if (!connection) {
		return;
	}

	connection->SendMsg(msg);
}

bool MacroActionWebsocket::PerformAction()
{
	if (_api != MacroActionWebsocket::API::SCENE_SWITCHER) {
		SendRequest(_message);
		return true;
	}

	if (_type == MacroActionWebsocket::MessageType::REQUEST) {
		auto vendorRequest = ConstructVendorRequestMessage(_message);
		SendRequest(vendorRequest);
	} else {
		SendWebsocketEvent(_message);
	}
	return true;
}

void MacroActionWebsocket::LogAction() const
{
	if (_api == API::GENERIC_WEBSOCKET) {
		vblog(LOG_INFO,
		      "sent generic websocket message \"%s\" via \"%s\"",
		      _message.c_str(),
		      GetWeakConnectionName(_connection).c_str());
		return;
	}

	if (_api == API::OBS_WEBSOCKET) {
		vblog(LOG_INFO, "sent obs websocket message \"%s\" via \"%s\"",
		      _message.c_str(),
		      GetWeakConnectionName(_connection).c_str());
		return;
	}

	switch (_type) {
	case MacroActionWebsocket::MessageType::REQUEST:
		vblog(LOG_INFO, "sent scene switcher message \"%s\" via \"%s\"",
		      _message.c_str(),
		      GetWeakConnectionName(_connection).c_str());
		break;
	case MacroActionWebsocket::MessageType::EVENT:
		vblog(LOG_INFO,
		      "sent scene switcher event \"%s\" to connected clients",
		      _message.c_str());
		break;
	default:
		break;
	}
}

bool MacroActionWebsocket::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "api", static_cast<int>(_api));
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	_message.Save(obj, "message");
	obs_data_set_string(obj, "connection",
			    GetWeakConnectionName(_connection).c_str());
	return true;
}

bool MacroActionWebsocket::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_api = static_cast<API>(obs_data_get_int(obj, "api"));
	_type = static_cast<MessageType>(obs_data_get_int(obj, "type"));
	_message.Load(obj, "message");
	_connection =
		GetWeakConnectionByName(obs_data_get_string(obj, "connection"));
	return true;
}

std::string MacroActionWebsocket::GetShortDesc() const
{
	if (_type == MessageType::REQUEST) {
		return GetWeakConnectionName(_connection);
	}
	return "";
}

static inline void populateAPISelection(QComboBox *list)
{
	for (const auto &[_, name] : apiTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateMessageTypeSelection(QComboBox *list)
{
	for (const auto &[_, name] : messageTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionWebsocketEdit::MacroActionWebsocketEdit(
	QWidget *parent, std::shared_ptr<MacroActionWebsocket> entryData)
	: QWidget(parent),
	  _apiType(new QComboBox(this)),
	  _messageType(new QComboBox(this)),
	  _message(new VariableTextEdit(this)),
	  _connection(new ConnectionSelection(this)),
	  _editLayout(new QHBoxLayout()),
	  _settingsConflict(new QLabel())
{
	populateAPISelection(_apiType);
	populateMessageTypeSelection(_messageType);
	_settingsConflict->setWordWrap(true);

	QWidget::connect(_apiType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(APITypeChanged(int)));
	QWidget::connect(_messageType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MessageTypeChanged(int)));
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_editLayout);
	mainLayout->addWidget(_message);
	mainLayout->addWidget(_settingsConflict);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWebsocketEdit::CheckForSettingsConflict()
{
	auto connection = _entryData->_connection.lock();
	if (!connection) {
		_settingsConflict->hide();
		return;
	}

	if (_entryData->_api == MacroActionWebsocket::API::GENERIC_WEBSOCKET &&
	    connection->IsUsingOBSProtocol()) {
		_settingsConflict->show();
		_settingsConflict->setText(obs_module_text(
			"AdvSceneSwitcher.action.websocket.settingsConflictGeneric"));
	} else if (_entryData->_api !=
			   MacroActionWebsocket::API::GENERIC_WEBSOCKET &&
		   !connection->IsUsingOBSProtocol()) {
		_settingsConflict->show();
		_settingsConflict->setText(obs_module_text(
			"AdvSceneSwitcher.action.websocket.settingsConflictOBS"));
	} else {
		_settingsConflict->hide();
	}

	adjustSize();
	updateGeometry();
}

void MacroActionWebsocketEdit::SetWidgetVisibility()
{
	_messageType->setVisible(_entryData->_api ==
				 MacroActionWebsocket::API::SCENE_SWITCHER);
	switch (_entryData->_api) {
	case MacroActionWebsocket::API::SCENE_SWITCHER:
		if (_entryData->_type ==
		    MacroActionWebsocket::MessageType::REQUEST) {
			SetupRequestEdit();
		} else {
			SetupEventEdit();
		}
		break;
	case MacroActionWebsocket::API::OBS_WEBSOCKET:
	case MacroActionWebsocket::API::GENERIC_WEBSOCKET:
		SetupGenericEdit();
		break;
	default:
		break;
	}

	CheckForSettingsConflict();

	adjustSize();
	updateGeometry();
}

void MacroActionWebsocketEdit::SetupRequestEdit()
{
	ClearWidgets();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.websocket.entry.sceneSwitcher.request"),
		_editLayout,
		{{"{{api}}", _apiType},
		 {"{{type}}", _messageType},
		 {"{{connection}}", _connection}});
	_connection->show();
}

void MacroActionWebsocketEdit::SetupEventEdit()
{
	ClearWidgets();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.websocket.entry.sceneSwitcher.event"),
		_editLayout,
		{{"{{api}}", _apiType},
		 {"{{type}}", _messageType},
		 {"{{connection}}", _connection}});

	// Add widget to layout but hide it
	_editLayout->addWidget(_connection);
	_connection->hide();
}

void MacroActionWebsocketEdit::SetupGenericEdit()
{
	ClearWidgets();
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.websocket.entry.generic"),
		     _editLayout,
		     {{"{{api}}", _apiType},
		      {"{{type}}", _messageType},
		      {"{{connection}}", _connection}});
	_connection->show();

	// Add widget to layout but hide it
	_editLayout->addWidget(_messageType);
	_messageType->hide();
}

void MacroActionWebsocketEdit::ClearWidgets()
{
	_editLayout->removeWidget(_apiType);
	_editLayout->removeWidget(_messageType);
	_editLayout->removeWidget(_connection);
	ClearLayout(_editLayout);
}

void MacroActionWebsocketEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_apiType->setCurrentIndex(static_cast<int>(_entryData->_api));
	_messageType->setCurrentIndex(static_cast<int>(_entryData->_type));
	_message->setPlainText(_entryData->_message);
	_connection->SetConnection(_entryData->_connection);

	SetWidgetVisibility();
}

void MacroActionWebsocketEdit::APITypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_api = static_cast<MacroActionWebsocket::API>(index);
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWebsocketEdit::MessageTypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type =
		static_cast<MacroActionWebsocket::MessageType>(index);
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWebsocketEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = _message->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroActionWebsocketEdit::ConnectionSelectionChanged(
	const QString &connection)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_connection = GetWeakConnectionByQString(connection);
	CheckForSettingsConflict();
	emit(HeaderInfoChanged(connection));
}

} // namespace advss
