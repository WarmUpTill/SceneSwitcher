#include "macro-action-websocket.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionWebsocket::id = "websocket";

bool MacroActionWebsocket::_registered = MacroActionFactory::Register(
	MacroActionWebsocket::id,
	{MacroActionWebsocket::Create, MacroActionWebsocketEdit::Create,
	 "AdvSceneSwitcher.action.websocket"});

static std::map<MacroActionWebsocket::Type, std::string> actionTypes = {
	{MacroActionWebsocket::Type::REQUEST,
	 "AdvSceneSwitcher.action.websocket.type.request"},
	{MacroActionWebsocket::Type::EVENT,
	 "AdvSceneSwitcher.action.websocket.type.event"},
};

void MacroActionWebsocket::SendRequest()
{
	auto connection = GetConnectionByName(_connection);
	if (!connection) {
		return;
	}
	connection->SendMsg(_message);
}

bool MacroActionWebsocket::PerformAction()
{
	switch (_type) {
	case MacroActionWebsocket::Type::REQUEST:
		SendRequest();
		break;
	case MacroActionWebsocket::Type::EVENT:
		SendWebsocketEvent(_message);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionWebsocket::LogAction()
{
	switch (_type) {
	case MacroActionWebsocket::Type::REQUEST:
		vblog(LOG_INFO, "sent msg \"%s\" via \"%s\"", _message.c_str(),
		      _connection.c_str());
		break;
	case MacroActionWebsocket::Type::EVENT:
		vblog(LOG_INFO, "sent event \"%s\" to connected clients",
		      _message.c_str());
		break;
	default:
		break;
	}
}

bool MacroActionWebsocket::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_string(obj, "message", _message.c_str());
	obs_data_set_string(obj, "connection", _connection.c_str());
	return true;
}

bool MacroActionWebsocket::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_message = obs_data_get_string(obj, "message");
	_connection = obs_data_get_string(obj, "connection");
	return true;
}

std::string MacroActionWebsocket::GetShortDesc()
{
	if (_type == Type::REQUEST) {
		return _connection;
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionWebsocketEdit::MacroActionWebsocketEdit(
	QWidget *parent, std::shared_ptr<MacroActionWebsocket> entryData)
	: QWidget(parent),
	  _actions(new QComboBox(this)),
	  _message(new ResizingPlainTextEdit(this)),
	  _connection(new ConnectionSelection(this)),
	  _editLayout(new QHBoxLayout())
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_editLayout);
	mainLayout->addWidget(_message);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWebsocketEdit::SetupRequestEdit()
{
	_editLayout->removeWidget(_actions);
	_editLayout->removeWidget(_connection);
	clearLayout(_editLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _actions},
		{"{{connection}}", _connection},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.websocket.entry.request"),
		     _editLayout, widgetPlaceholders);
	_connection->show();
}

void MacroActionWebsocketEdit::SetupEventEdit()
{
	_editLayout->removeWidget(_actions);
	_editLayout->removeWidget(_connection);
	clearLayout(_editLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _actions},
		{"{{connection}}", _connection},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.websocket.entry.event"),
		     _editLayout, widgetPlaceholders);
	_connection->hide();
}

void MacroActionWebsocketEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_message->setPlainText(QString::fromStdString(_entryData->_message));
	_connection->SetConnection(_entryData->_connection);

	if (_entryData->_type == MacroActionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}

	adjustSize();
	updateGeometry();
}

void MacroActionWebsocketEdit::ActionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroActionWebsocket::Type>(index);
	if (_entryData->_type == MacroActionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionWebsocketEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
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

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_connection = connection.toStdString();
	emit(HeaderInfoChanged(connection));
}
