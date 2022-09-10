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

static bool matchRegex(const RegexConfig &conf, const std::string &msg,
		       const std::string &expr)
{
	auto regex = conf.GetRegularExpression(expr);
	if (!regex.isValid()) {
		return false;
	}
	auto match = regex.match(QString::fromStdString(msg));
	return match.hasMatch();
}

bool MacroConditionWebsocket::CheckCondition()
{
	for (const auto &msg : switcher->websocketMessages) {
		if (_regex.Enabled()) {
			if (matchRegex(_regex, msg, _message)) {
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
	_regex.Save(obj);
	return true;
}

bool MacroConditionWebsocket::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_message = obs_data_get_string(obj, "message");
	_regex.Load(obj);
	// TOOD: remove in future version
	if (obs_data_has_user_value(obj, "useRegex")) {
		_regex.CreateBackwardsCompatibleRegex(
			obs_data_get_bool(obj, "useRegex"), false);
	}
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
	  _regex(new RegexConfigWidget(parent))
{
	QWidget::connect(_message, SIGNAL(textChanged()), this,
			 SLOT(MessageChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.websocket.entry")));
	mainLayout->addWidget(_message);
	mainLayout->addWidget(_regex);
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
	_regex->SetRegexConfig(_entryData->_regex);

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

void MacroConditionWebsocketEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_regex = conf;

	adjustSize();
	updateGeometry();
}
