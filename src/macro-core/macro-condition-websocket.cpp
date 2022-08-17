#include "macro-condition-edit.hpp"
#include "macro-condition-websocket.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionWebsocket::id = "websocket";

bool MacroConditionWebsocket::_registered = MacroConditionFactory::Register(
	MacroConditionWebsocket::id,
	{MacroConditionWebsocket::Create, MacroConditionWebsocketEdit::Create,
	 "AdvSceneSwitcher.condition.websocket"});

static bool matchRegex(const std::string &msg, const std::string &expr)
{
	try {
		std::regex regExpr(expr);
		return std::regex_match(msg, regExpr);
	} catch (const std::regex_error &) {
	}
	return false;
}

bool MacroConditionWebsocket::CheckCondition()
{
	for (const auto &msg : switcher->websocketMessages) {
		if (_useRegex) {
			if (matchRegex(msg, _message)) {
				return true;
			}
		} else {
			if (msg == _message) {
				return true;
			}
		}
	}
	return false;
}

bool MacroConditionWebsocket::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "message", _message.c_str());
	obs_data_set_bool(obj, "useRegex", _useRegex);
	return true;
}

bool MacroConditionWebsocket::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_message = obs_data_get_string(obj, "message");
	_useRegex = obs_data_get_bool(obj, "useRegex");
	return true;
}

std::string MacroConditionWebsocket::GetShortDesc()
{
	return "";
}

MacroConditionWebsocketEdit::MacroConditionWebsocketEdit(
	QWidget *parent, std::shared_ptr<MacroConditionWebsocket> entryData)
	: QWidget(parent),
	  _message(new ResizingPlainTextEdit(this)),
	  _useRegex(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.websocket.useRegex")))
{
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_useRegex, SIGNAL(stateChanged(int)), this,
			 SLOT(UseRegexChanged(int)));

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.websocket.entry")));
	mainLayout->addWidget(_message);
	mainLayout->addWidget(_useRegex);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionWebsocketEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_message->setPlainText(QString::fromStdString(_entryData->_message));
	_useRegex->setChecked(_entryData->_useRegex);

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::MessageChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_message = _message->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();
}

void MacroConditionWebsocketEdit::UseRegexChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_useRegex = state;
}
