#include "macro-condition-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <utility.hpp>

namespace advss {

const std::string MacroConditionTwitch::id = "twitch";

bool MacroConditionTwitch::_registered = MacroConditionFactory::Register(
	MacroConditionTwitch::id,
	{MacroConditionTwitch::Create, MacroConditionTwitchEdit::Create,
	 "AdvSceneSwitcher.condition.twitch"});

const static std::map<MacroConditionTwitch::Condition, std::string>
	conditionTypes = {
		{MacroConditionTwitch::Condition::LIVE,
		 "AdvSceneSwitcher.condition.twitch.type.channelIsLive"},
		{MacroConditionTwitch::Condition::LIVE_EVENT,
		 "AdvSceneSwitcher.condition.twitch.type.channelWentLive"},
		{MacroConditionTwitch::Condition::TITLE,
		 "AdvSceneSwitcher.condition.twitch.type.channelTitle"},
};

static bool titleMatches(const RegexConfig &conf, const std::string &title,
			 const std::string &expr)
{
	if (!conf.Enabled()) {
		return title == expr;
	}

	auto regex = conf.GetRegularExpression(expr);
	if (!regex.isValid()) {
		return false;
	}
	auto match = regex.match(QString::fromStdString(title));
	return match.hasMatch();
}

bool MacroConditionTwitch::CheckCondition()
{
	auto token = _token.lock();
	if (!token) {
		return false;
	}

	switch (_condition) {
	case MacroConditionTwitch::Condition::LIVE: {
		auto info = _channel.GetLiveInfo(*token);
		if (!info) {
			return false;
		}
		return info->IsLive();
	}
	case MacroConditionTwitch::Condition::LIVE_EVENT:
		// TODO
		break;
	case MacroConditionTwitch::Condition::TITLE: {
		auto info = _channel.GetInfo(*token);
		if (!info) {
			return false;
		}
		return titleMatches(_regex, info->title, _streamTitle);
	}
	default:
		break;
	}

	return false;
}

bool MacroConditionTwitch::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "token",
			    GetWeakTwitchTokenName(_token).c_str());
	_streamTitle.Save(obj, "streamTitle");
	_category.Save(obj);
	_channel.Save(obj);
	_regex.Save(obj);
	return true;
}

bool MacroConditionTwitch::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_streamTitle.Load(obj, "streamTitle");
	_category.Load(obj);
	_channel.Load(obj);
	_regex.Load(obj);
	return true;
}

std::string MacroConditionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

bool MacroConditionTwitch::ConditionIsSupportedByToken()
{
	static const std::unordered_map<Condition, TokenOption> requiredOption =
		{
			{Condition::LIVE, {""}},
			{Condition::LIVE_EVENT, {""}},
			{Condition::TITLE, {""}},
		};
	auto token = _token.lock();
	if (!token) {
		return false;
	}
	auto option = requiredOption.find(_condition);
	assert(option != requiredOption.end());
	if (option == requiredOption.end()) {
		return false;
	}
	return option->second.apiId.empty() ||
	       token->OptionIsEnabled(option->second);
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionTwitchEdit::MacroConditionTwitchEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTwitch> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _streamTitle(new VariableLineEdit(this)),
	  _category(new TwitchCategoryWidget(this)),
	  _channel(new TwitchChannelSelection(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _layout(new QHBoxLayout()),
	  _tokenPermissionWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.twitchToken.permissionsInsufficient")))
{
	_streamTitle->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_streamTitle->setMaxLength(140);

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(_streamTitle, SIGNAL(editingFinished()), this,
			 SLOT(StreamTitleChanged()));
	QWidget::connect(_category,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QWidget::connect(_channel,
			 SIGNAL(ChannelChanged(const TwitchChannel &)), this,
			 SLOT(ChannelChanged(const TwitchChannel &)));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(&_tokenPermissionCheckTimer, SIGNAL(timeout()), this,
			 SLOT(CheckTokenPermissions()));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.twitch.entry"),
		     _layout,
		     {{"{{conditions}}", _conditions},
		      {"{{streamTitle}}", _streamTitle},
		      {"{{category}}", _category},
		      {"{{regex}}", _regex},
		      {"{{channel}}", _channel}});
	_layout->setContentsMargins(0, 0, 0, 0);

	auto accountLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.twitch.entry.account"),
		     accountLayout, {{"{{account}}", _tokens}});
	accountLayout->setContentsMargins(0, 0, 0, 0);

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(_layout);
	mainLayout->addLayout(accountLayout);
	mainLayout->addWidget(_tokenPermissionWarning);
	setLayout(mainLayout);

	_tokenPermissionCheckTimer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	SetupWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroConditionTwitchEdit::StreamTitleChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_streamTitle = _streamTitle->text().toStdString();
}

void MacroConditionTwitchEdit::CategoreyChanged(const TwitchCategory &category)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_category = category;
}

void MacroConditionTwitchEdit::CheckTokenPermissions()
{
	_tokenPermissionWarning->setVisible(
		_entryData && !_entryData->ConditionIsSupportedByToken());
	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::SetupWidgetVisibility()
{
	_streamTitle->setVisible(_entryData->_condition ==
				 MacroConditionTwitch::Condition::TITLE);
	_regex->setVisible(_entryData->_condition ==
			   MacroConditionTwitch::Condition::TITLE);
	_category->setVisible(_entryData->_condition ==
			      MacroConditionTwitch::Condition::CATEGORY);
	if (_entryData->_condition == MacroConditionTwitch::Condition::TITLE) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}

	_tokenPermissionWarning->setVisible(
		!_entryData->ConditionIsSupportedByToken());

	adjustSize();
	updateGeometry();
}

void MacroConditionTwitchEdit::ChannelChanged(const TwitchChannel &channel)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_channel = channel;
}

void MacroConditionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_tokens->SetToken(_entryData->_token);
	_streamTitle->setText(_entryData->_streamTitle);
	_category->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);
	_channel->SetChannel(_entryData->_channel);
	_regex->SetRegexConfig(_entryData->_regex);
	SetupWidgetVisibility();
}

void MacroConditionTwitchEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionTwitch::Condition>(value);
	SetupWidgetVisibility();
}

void MacroConditionTwitchEdit::RegexChanged(RegexConfig conf)
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
