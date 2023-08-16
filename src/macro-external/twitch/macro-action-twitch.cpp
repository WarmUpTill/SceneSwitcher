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
};

void MacroActionTwitch::SetStreamTitle(const std::shared_ptr<TwitchToken> &token)
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
		blog(LOG_INFO, "Failed to set stream title");
	}
}

void MacroActionTwitch::SetStreamCategory(
	const std::shared_ptr<TwitchToken> &token)
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
		blog(LOG_INFO, "Failed to set stream category");
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
	return true;
}

bool MacroActionTwitch::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_token = GetWeakTwitchTokenByName(obs_data_get_string(obj, "token"));
	_text.Load(obj, "text");
	_category.Load(obj);
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
	  _layout(new QHBoxLayout())
{
	_text->setSizePolicy(QSizePolicy::MinimumExpanding,
			     QSizePolicy::Preferred);
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

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.twitch.entry"),
		     _layout,
		     {{"{{account}}", _tokens},
		      {"{{actions}}", _actions},
		      {"{{text}}", _text},
		      {"{{category}}", _category},
		      {"{{manualCategorySearch}}", _manualCategorySearch}});
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

void MacroActionTwitchEdit::SetupWidgetVisibility()
{
	_text->setVisible(_entryData->_action ==
			  MacroActionTwitch::Action::TITLE);
	_category->setVisible(_entryData->_action ==
			      MacroActionTwitch::Action::CATEGORY);
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
