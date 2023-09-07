#include "macro-action-twitch.hpp"
#include "twitch-helpers.hpp"

#include <log-helper.hpp>
#include <utility.hpp>

namespace advss {

const std::string MacroActionTwitch::id = "twitch";

bool MacroActionTwitch::_registered = MacroActionFactory::Register(
	MacroActionTwitch::id,
	{MacroActionTwitch::Create, MacroActionTwitchEdit::Create,
	 "AdvSceneSwitcher.action.twitch"});

const static std::map<MacroActionTwitch::Action, std::string> actionTypes = {
	{MacroActionTwitch::Action::TITLE,
	 "AdvSceneSwitcher.action.twitch.type.title"},
	{MacroActionTwitch::Action::CATEGORY,
	 "AdvSceneSwitcher.action.twitch.type.category"},
	{MacroActionTwitch::Action::COMMERCIAL,
	 "AdvSceneSwitcher.action.twitch.type.commercial"},
};

void MacroActionTwitch::SetStreamTitle(
	const std::shared_ptr<TwitchToken> &token) const
{
	if (std::string(_text).empty()) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "title", _text.c_str());
	auto result = SendPatchRequest(
		"https://api.twitch.tv",
		std::string("/helix/channels?broadcaster_id=") +
			token->GetUserID(),
		*token, data.Get());
	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream title! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::SetStreamCategory(
	const std::shared_ptr<TwitchToken> &token) const
{
	if (_category.id == -1) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "game_id",
			    std::to_string(_category.id).c_str());
	auto result = SendPatchRequest(
		"https://api.twitch.tv",
		std::string("/helix/channels?broadcaster_id=") +
			token->GetUserID(),
		*token, data.Get());
	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream category! (%d)",
		     result.status);
	}
}

void MacroActionTwitch::StartCommercial(
	const std::shared_ptr<TwitchToken> &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "broadcaster_id", token->GetUserID().c_str());
	obs_data_set_int(data, "length", _duration.Seconds());
	auto result = SendPostRequest("https://api.twitch.tv",
				      "/helix/channels/commercial", *token,
				      data.Get());
	if (result.status != 200) {
		OBSDataArrayAutoRelease replyArray =
			obs_data_get_array(result.data, "data");
		OBSDataAutoRelease replyData =
			obs_data_array_item(replyArray, 0);
		blog(LOG_INFO,
		     "Failed to start commercial! (%d)\n"
		     "length: %d\n"
		     "message: %s\n"
		     "retry_after: %d\n",
		     result.status, obs_data_get_int(replyData, "length"),
		     obs_data_get_string(replyData, "message"),
		     obs_data_get_int(replyData, "retry_after"));
	}
}

bool MacroActionTwitch::PerformAction()
{
	auto token = _token.lock();
	if (!token) {
		return true;
	}

	switch (_action) {
	case MacroActionTwitch::Action::TITLE:
		SetStreamTitle(token);
		break;
	case MacroActionTwitch::Action::CATEGORY:
		SetStreamCategory(token);
		break;
	case MacroActionTwitch::Action::COMMERCIAL:
		StartCommercial(token);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionTwitch::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" with token for \"%s\"",
		      it->second.c_str(),
		      GetWeakTwitchTokenName(_token).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown twitch action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionTwitch::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_string(obj, "token",
			    GetWeakTwitchTokenName(_token).c_str());
	_text.Save(obj, "text");
	_category.Save(obj);
	_duration.Save(obj);
	return true;
}

bool MacroActionTwitch::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_text.Load(obj, "text");
	_category.Load(obj);
	_duration.Load(obj);
	return true;
}

std::string MacroActionTwitch::GetShortDesc() const
{
	return GetWeakTwitchTokenName(_token);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionTwitchEdit::MacroActionTwitchEdit(
	QWidget *parent, std::shared_ptr<MacroActionTwitch> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _tokens(new TwitchConnectionSelection()),
	  _text(new VariableLineEdit(this)),
	  _category(new TwitchCategorySelection(this)),
	  _manualCategorySearch(new TwitchCategorySearchButton()),
	  _duration(new DurationSelection(this, false, 0)),
	  _layout(new QHBoxLayout())
{
	_text->setSizePolicy(QSizePolicy::MinimumExpanding,
			     QSizePolicy::Preferred);
	_duration->SpinBox()->setSuffix("s");
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_tokens, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(TwitchTokenChanged(const QString &)));
	QWidget::connect(_text, SIGNAL(editingFinished()), this,
			 SLOT(TextChanged()));
	QWidget::connect(_category,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QWidget::connect(_duration,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SLOT(CategoreyChanged(const TwitchCategory &)));
	QObject::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.twitch.entry"),
		     _layout,
		     {{"{{account}}", _tokens},
		      {"{{actions}}", _actions},
		      {"{{text}}", _text},
		      {"{{category}}", _category},
		      {"{{manualCategorySearch}}", _manualCategorySearch},
		      {"{{duration}}", _duration}});
	setLayout(_layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTwitchEdit::TwitchTokenChanged(const QString &token)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_token = GetWeakTwitchTokenByQString(token);
	_category->SetToken(_entryData->_token);
	_manualCategorySearch->SetToken(_entryData->_token);
	SetupWidgetVisibility();
	emit(HeaderInfoChanged(token));
}

void MacroActionTwitchEdit::TextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_text = _text->text().toStdString();
}

void MacroActionTwitchEdit::CategoreyChanged(const TwitchCategory &category)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_category = category;
}

void MacroActionTwitchEdit::DurationChanged(const Duration &duration)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_duration = duration;
}

void MacroActionTwitchEdit::SetupWidgetVisibility()
{
	_text->setVisible(_entryData->_action ==
			  MacroActionTwitch::Action::TITLE);
	_category->setVisible(_entryData->_action ==
			      MacroActionTwitch::Action::CATEGORY);
	_manualCategorySearch->setVisible(_entryData->_action ==
					  MacroActionTwitch::Action::CATEGORY);
	_duration->setVisible(_entryData->_action ==
			      MacroActionTwitch::Action::COMMERCIAL);
	if (_entryData->_action == MacroActionTwitch::Action::TITLE) {
		RemoveStretchIfPresent(_layout);
	} else {
		AddStretchIfNecessary(_layout);
	}

	adjustSize();
	updateGeometry();
}

void MacroActionTwitchEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_tokens->SetToken(_entryData->_token);
	_text->setText(_entryData->_text);
	_category->SetToken(_entryData->_token);
	_manualCategorySearch->SetToken(_entryData->_token);
	_category->SetCategory(_entryData->_category);
	_duration->SetDuration(_entryData->_duration);
	SetupWidgetVisibility();
}

void MacroActionTwitchEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionTwitch::Action>(value);
	SetupWidgetVisibility();
}

} // namespace advss
