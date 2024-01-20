#include "macro-condition-websocket.hpp"
#include "layout-helpers.hpp"

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

bool MacroConditionWebsocket::CheckCondition()
{
	std::vector<std::string> messages;
	switch (_type) {
	case MacroConditionWebsocket::Type::REQUEST:
		messages = GetWebsocketMessages();
		break;
	case MacroConditionWebsocket::Type::EVENT: {
		auto connection = _connection.lock();
		if (!connection) {
			return false;
		}
		messages = connection->Events();
		break;
	}
	default:
		break;
	}

	for (const auto &msg : messages) {
		if (_regex.Enabled()) {
			if (_regex.Matches(msg, _message)) {
				SetVariableValue(msg);
				return true;
			}
		} else {
			if (msg == std::string(_message)) {
				SetVariableValue(msg);
				return true;
			}
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
	return true;
}

std::string MacroConditionWebsocket::GetShortDesc() const
{
	if (_type == Type::REQUEST) {
		return "";
	}
	return GetWeakConnectionName(_connection);
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
	  _connection(new ConnectionSelection(this)),
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

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_editLayout);
	mainLayout->addWidget(_message);
	auto regexLayout = new QHBoxLayout;
	regexLayout->addWidget(_regex);
	regexLayout->addStretch();
	regexLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(regexLayout);
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

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_message->setPlainText(_entryData->_message);
	_regex->SetRegexConfig(_entryData->_regex);
	_connection->SetConnection(_entryData->_connection);

	if (_entryData->_type == MacroConditionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type = static_cast<MacroConditionWebsocket::Type>(index);
	if (_entryData->_type == MacroConditionWebsocket::Type::REQUEST) {
		SetupRequestEdit();
	} else {
		SetupEventEdit();
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionWebsocketEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = _message->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::ConnectionSelectionChanged(
	const QString &connection)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_connection = GetWeakConnectionByQString(connection);
	emit(HeaderInfoChanged(connection));
}

void MacroConditionWebsocketEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}

} // namespace advss
