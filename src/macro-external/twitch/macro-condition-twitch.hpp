#pragma once
#include "macro-condition-edit.hpp"
#include "token.hpp"
#include "category-selection.hpp"
#include "channel-selection.hpp"

#include <variable-line-edit.hpp>
#include <regex-config.hpp>

namespace advss {

class MacroConditionTwitch : public MacroCondition {
public:
	MacroConditionTwitch(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionTwitch>(m);
	}
	bool ConditionIsSupportedByToken();

	enum class Condition {
		LIVE,
		LIVE_EVENT,
		TITLE,
		CATEGORY,
		//...
	};

	Condition _condition = Condition::LIVE;
	std::weak_ptr<TwitchToken> _token;
	TwitchChannel _channel;
	StringVariable _streamTitle = obs_module_text(
		"AdvSceneSwitcher.condition.twitch.title.title");
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig();
	TwitchCategory _category;

private:
	ChannelInfo GetChannelInfo(const std::shared_ptr<TwitchToken> &);

	static bool _registered;
	static const std::string id;
};

class MacroConditionTwitchEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionTwitchEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionTwitch> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionTwitchEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionTwitch>(cond));
	}

private slots:
	void ConditionChanged(int);
	void TwitchTokenChanged(const QString &);
	void StreamTitleChanged();
	void CategoreyChanged(const TwitchCategory &);
	void ChannelChanged(const TwitchChannel &);
	void RegexChanged(RegexConfig);
	void CheckTokenPermissions();

signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroConditionTwitch> _entryData;

private:
	void SetupWidgetVisibility();

	QComboBox *_conditions;
	TwitchConnectionSelection *_tokens;
	VariableLineEdit *_streamTitle;
	TwitchCategoryWidget *_category;
	TwitchChannelSelection *_channel;
	RegexConfigWidget *_regex;
	QHBoxLayout *_layout;
	QLabel *_tokenPermissionWarning;
	QTimer _tokenPermissionCheckTimer;
	bool _loading = true;
};

} // namespace advss
