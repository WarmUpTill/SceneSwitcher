#include "macro-action-websocket.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionWebsocket::id = "websocket";

bool MacroActionWebsocket::_registered = MacroActionFactory::Register(
	MacroActionWebsocket::id,
	{MacroActionWebsocket::Create, MacroActionWebsocketEdit::Create,
	 "AdvSceneSwitcher.action.websocket"});

bool MacroActionWebsocket::PerformAction()
{
	auto connection = GetConnectionByName(_connection);
	if (!connection) {
		return true;
	}
	connection->SendMsg(_message);
	return true;
}

void MacroActionWebsocket::LogAction()
{
	vblog(LOG_INFO, "sent msg \"%s\" via \"%s\"", _message.c_str(),
	      _connection.c_str());
}

bool MacroActionWebsocket::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "message", _message.c_str());
	obs_data_set_string(obj, "connection", _connection.c_str());
	return true;
}

bool MacroActionWebsocket::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_message = obs_data_get_string(obj, "message");
	_connection = obs_data_get_string(obj, "connection");
	return true;
}

std::string MacroActionWebsocket::GetShortDesc()
{
	return _connection;
}

MacroActionWebsocketEdit::MacroActionWebsocketEdit(
	QWidget *parent, std::shared_ptr<MacroActionWebsocket> entryData)
	: QWidget(parent),
	  _message(new ResizingPlainTextEdit(this)),
	  _connection(new ConnectionSelection(this))
{
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_connection, SIGNAL(SelectionChanged(const QString &)),
			 this,
			 SLOT(ConnectionSelectionChanged(const QString &)));

	auto *connectionLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{connection}}", _connection},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.websocket.entry"),
		     connectionLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(connectionLayout);
	mainLayout->addWidget(_message);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWebsocketEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_message->setPlainText(QString::fromStdString(_entryData->_message));
	_connection->SetConnection(_entryData->_connection);

	adjustSize();
	updateGeometry();
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
