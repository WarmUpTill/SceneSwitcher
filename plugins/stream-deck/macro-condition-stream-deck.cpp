#include "macro-condition-stream-deck.hpp"
#include "layout-helpers.hpp"
#include "websocket-api.hpp"

#include <obs.hpp>

namespace advss {

const std::string MacroConditionStreamdeck::id = "streamdeck";

bool MacroConditionStreamdeck::_registered = MacroConditionFactory::Register(
	MacroConditionStreamdeck::id,
	{MacroConditionStreamdeck::Create, MacroConditionStreamdeckEdit::Create,
	 "AdvSceneSwitcher.condition.streamDeck"});

constexpr char streamDeckMessage[] = "StreamDeckKeyEvent";

static StreamDeckMessageDispatcher messageDispatcher;

static bool setup();
static bool setupDone = setup();
static void receiveStreamDeckMessage(obs_data_t *request_data, obs_data_t *);

bool setup()
{
	RegisterWebsocketRequest(streamDeckMessage, receiveStreamDeckMessage);
	return true;
}

static void printParseError(obs_data_t *data)
{
	blog(LOG_INFO, "received unexpected stream deck message: %s",
	     obs_data_get_json(data) ? obs_data_get_json(data) : "''");
}

void receiveStreamDeckMessage(obs_data_t *request_data, obs_data_t *)
{
	StreamDeckMessage message;
	OBSDataAutoRelease data = obs_data_get_obj(request_data, "data");
	if (!data) {
		printParseError(request_data);
		return;
	}

	if (!obs_data_has_user_value(data, "isKeyDownEvent")) {
		printParseError(request_data);
		return;
	}
	message.keyDown = obs_data_get_bool(data, "isKeyDownEvent");

	const bool isMultiAction = obs_data_get_bool(data, "isInMultiAction");
	if (!isMultiAction) {
		OBSDataAutoRelease coordinates =
			obs_data_get_obj(data, "coordinates");
		if (!coordinates && !isMultiAction) {
			printParseError(request_data);
			return;
		}

		message.row = obs_data_get_int(coordinates, "row");
		message.column = obs_data_get_int(coordinates, "column");
	} else {
		message.row = 0;
		message.column = 0;
		vblog(LOG_INFO,
		      "received multi action stream deck message"
		      " - setting coordinates to (%d, %d)",
		      message.row, message.column);
	}

	OBSDataAutoRelease settings = obs_data_get_obj(data, "settings");
	if (!settings) {
		printParseError(request_data);
		return;
	}
	message.data = obs_data_get_string(settings, "data");

	messageDispatcher.DispatchMessage(message);
}

StreamDeckMessageBuffer RegisterForStreamDeckMessages()
{
	return messageDispatcher.RegisterClient();
}

MacroConditionStreamdeck::MacroConditionStreamdeck(Macro *m)
	: MacroCondition(m, true)
{
	_messageBuffer = RegisterForStreamDeckMessages();
}

bool MacroConditionStreamdeck::MessageMatches(const StreamDeckMessage &message)
{
	const bool keyStateMatches = !_pattern.checkKeyState ||
				     ((message.keyDown && _pattern.keyDown) ||
				      (!message.keyDown && !_pattern.keyDown));
	const bool positionMatches = !_pattern.checkPosition ||
				     (message.row == _pattern.row &&
				      message.column == _pattern.column);
	const bool dataMatches =
		!_pattern.checkData ||
		(_pattern.regex.Enabled()
			 ? _pattern.regex.Matches(message.data, _pattern.data)
			 : message.data == std::string(_pattern.data));
	return keyStateMatches && positionMatches && dataMatches;
}

bool MacroConditionStreamdeck::CheckCondition()
{
	while (!_messageBuffer->Empty()) {
		auto message = _messageBuffer->ConsumeMessage();
		if (!message) {
			continue;
		}

		if (!message->keyDown) {
			_lastMatchingKeyIsStillPressed = false;
		}

		if (MessageMatches(*message)) {
			_lastMatchingKeyIsStillPressed = message->keyDown;
			SetTempVarValues(*message);
			return true;
		}
	}

	if (_lastMatchingKeyIsStillPressed) {
		return true;
	}

	return false;
}

bool MacroConditionStreamdeck::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_pattern.Save(obj);
	return true;
}

bool MacroConditionStreamdeck::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_pattern.Load(obj);
	return true;
}

void MacroConditionStreamdeck::StreamDeckMessagePattern::Save(
	obs_data_t *obj) const
{
	OBSDataAutoRelease dataObj = obs_data_create();
	obs_data_set_bool(dataObj, "checkKeyState", checkKeyState);
	obs_data_set_bool(dataObj, "keyDown", keyDown);
	obs_data_set_bool(dataObj, "checkPosition", checkPosition);
	row.Save(dataObj, "row");
	column.Save(dataObj, "column");
	obs_data_set_bool(dataObj, "checkData", checkData);
	data.Save(dataObj, "data");
	regex.Save(dataObj);
	obs_data_set_obj(obj, "pattern", dataObj);
}

void MacroConditionStreamdeck::StreamDeckMessagePattern::Load(obs_data_t *obj)
{
	OBSDataAutoRelease dataObj = obs_data_get_obj(obj, "pattern");
	checkKeyState = obs_data_get_bool(dataObj, "checkKeyState");
	keyDown = obs_data_get_bool(dataObj, "keyDown");
	checkPosition = obs_data_get_bool(dataObj, "checkPosition");
	row.Load(dataObj, "row");
	column.Load(dataObj, "column");
	checkData = obs_data_get_bool(dataObj, "checkData");
	data.Load(dataObj, "data");
	regex.Load(dataObj);
	obs_data_set_obj(obj, "pattern", dataObj);
}

void MacroConditionStreamdeck::SetTempVarValues(const StreamDeckMessage &message)
{
	SetTempVarValue("keyPressed", std::to_string(message.keyDown));
	SetTempVarValue("row", std::to_string(message.row));
	SetTempVarValue("column", std::to_string(message.column));
	SetTempVarValue("data", message.data);
}

void MacroConditionStreamdeck::SetupTempVars()
{
	AddTempvar(
		"keyPressed",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streamDeck.keyPressed"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streamDeck.keyPressed.description"));
	AddTempvar("row",
		   obs_module_text("AdvSceneSwitcher.tempVar.streamDeck.row"));
	AddTempvar(
		"column",
		obs_module_text("AdvSceneSwitcher.tempVar.streamDeck.column"));
	AddTempvar(
		"data",
		obs_module_text("AdvSceneSwitcher.tempVar.streamDeck.data"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streamDeck.data.description"));
}

MacroConditionStreamdeckEdit::MacroConditionStreamdeckEdit(
	QWidget *parent, std::shared_ptr<MacroConditionStreamdeck> entryData)
	: QWidget(parent),
	  _checkKeyState(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.streamDeck.checkKeyState"))),
	  _keyState(new QComboBox(this)),
	  _checkPosition(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.streamDeck.checkKeyPosition"))),
	  _row(new VariableSpinBox(this)),
	  _column(new VariableSpinBox(this)),
	  _checkData(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.streamDeck.checkData"))),
	  _data(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _listen(new QPushButton(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.streamDeck.startListen"),
		  this))
{
	_keyState->addItem(
		obs_module_text(
			"AdvSceneSwitcher.condition.streamDeck.keyState.pressed"),
		true);
	_keyState->addItem(
		obs_module_text(
			"AdvSceneSwitcher.condition.streamDeck.keyState.released"),
		false);

	connect(_checkKeyState, &QCheckBox::stateChanged, this,
		&MacroConditionStreamdeckEdit::CheckKeyStateChanged);
	connect(_keyState, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MacroConditionStreamdeckEdit::KeyStateChanged);
	connect(_checkPosition, &QCheckBox::stateChanged, this,
		&MacroConditionStreamdeckEdit::CheckPositionChanged);
	connect(_row,
		static_cast<void (VariableSpinBox::*)(const IntVariable &)>(
			&VariableSpinBox::NumberVariableChanged),
		this, &MacroConditionStreamdeckEdit::RowChanged);
	connect(_column,
		static_cast<void (VariableSpinBox::*)(const IntVariable &)>(
			&VariableSpinBox::NumberVariableChanged),
		this, &MacroConditionStreamdeckEdit::ColumnChanged);
	connect(_checkData, &QCheckBox::stateChanged, this,
		&MacroConditionStreamdeckEdit::CheckDataChanged);
	connect(_regex, &RegexConfigWidget::RegexConfigChanged, this,
		&MacroConditionStreamdeckEdit::RegexChanged);
	connect(_data, &VariableTextEdit::textChanged, this,
		&MacroConditionStreamdeckEdit::DataChanged);
	connect(_listen, &QPushButton::clicked, this,
		&MacroConditionStreamdeckEdit::ListenClicked);

	auto keyStateLayout = new QHBoxLayout();
	keyStateLayout->addWidget(_checkKeyState);
	keyStateLayout->addWidget(_keyState);
	keyStateLayout->addStretch();

	auto positionLayout = new QHBoxLayout();
	positionLayout->addWidget(_row);
	positionLayout->addWidget(_column);

	auto dataLayout = new QHBoxLayout();
	dataLayout->addWidget(_checkData);
	dataLayout->addWidget(_regex);
	dataLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->addLayout(keyStateLayout);
	layout->addWidget(_checkPosition);
	layout->addLayout(positionLayout);
	layout->addLayout(dataLayout);
	layout->addWidget(_data);
	layout->addWidget(_listen);
	auto label = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.streamDeck.pluginDownload"));
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	layout->addWidget(label);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionStreamdeckEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_checkKeyState->setChecked(_entryData->_pattern.checkKeyState);
	_keyState->setCurrentIndex(
		_keyState->findData(_entryData->_pattern.keyDown));
	_checkPosition->setChecked(_entryData->_pattern.checkPosition);
	_row->SetValue(_entryData->_pattern.row);
	_column->SetValue(_entryData->_pattern.column);
	_checkData->setChecked(_entryData->_pattern.checkData);
	_data->setPlainText(_entryData->_pattern.data);
	_regex->SetRegexConfig(_entryData->_pattern.regex);

	SetWidgetVisibility();
}

void MacroConditionStreamdeckEdit::CheckKeyStateChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.checkKeyState = state;
	SetWidgetVisibility();
}

void MacroConditionStreamdeckEdit::KeyStateChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.keyDown = _keyState->itemData(index).toBool();
}

void MacroConditionStreamdeckEdit::CheckPositionChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.checkPosition = state;
	SetWidgetVisibility();
}

void MacroConditionStreamdeckEdit::RowChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.row = value;
}

void MacroConditionStreamdeckEdit::ColumnChanged(const IntVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.column = value;
}

void MacroConditionStreamdeckEdit::CheckDataChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.checkData = state;
	SetWidgetVisibility();
}

void MacroConditionStreamdeckEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.regex = regex;
}

void MacroConditionStreamdeckEdit::DataChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pattern.data = _data->toPlainText().toStdString();
}

void MacroConditionStreamdeckEdit::ListenClicked()
{
	_isListening = !_isListening;

	_keyState->setDisabled(_isListening);
	_row->setDisabled(_isListening);
	_column->setDisabled(_isListening);
	_data->setDisabled(_isListening);
	_regex->setDisabled(_isListening);

	if (_isListening) {
		_messageBuffer = RegisterForStreamDeckMessages();
		_listen->setText(obs_module_text(
			"AdvSceneSwitcher.condition.streamDeck.stopListen"));
	} else {
		_updateListenSettings.stop();
		_messageBuffer.reset();
		_listen->setText(obs_module_text(
			"AdvSceneSwitcher.condition.streamDeck.startListen"));
		return;
	}

	connect(&_updateListenSettings, &QTimer::timeout, this, [this]() {
		if (_messageBuffer->Empty()) {
			return;
		}

		StreamDeckMessage lastMessageInBuffer;
		while (!_messageBuffer->Empty()) {
			auto message = _messageBuffer->ConsumeMessage();
			if (!message) {
				continue;
			}
			lastMessageInBuffer = *message;
		}

		_keyState->setCurrentIndex(
			_keyState->findData(lastMessageInBuffer.keyDown));
		_row->SetFixedValue(lastMessageInBuffer.row);
		_column->SetFixedValue(lastMessageInBuffer.column);
		_data->setPlainText(lastMessageInBuffer.data);
	});

	_updateListenSettings.start(300);
}

void MacroConditionStreamdeckEdit::SetWidgetVisibility()
{
	_keyState->setVisible(_checkKeyState->isChecked());
	_row->setVisible(_checkPosition->isChecked());
	_column->setVisible(_checkPosition->isChecked());
	_data->setVisible(_checkData->isChecked());
	_regex->setVisible(_checkData->isChecked());

	adjustSize();
	updateGeometry();
}

} // namespace advss
