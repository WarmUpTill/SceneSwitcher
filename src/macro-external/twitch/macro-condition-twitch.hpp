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
		// Event based
		LIVE_EVENT_REGULAR = 10,
		LIVE_EVENT_PLAYLIST = 20,
		LIVE_EVENT_WATCHPARTY = 30,
		LIVE_EVENT_PREMIERE = 40,
		LIVE_EVENT_RERUN = 50,
		OFFLINE_EVENT = 60,
		CHANNEL_UPDATE_EVENT = 70,
		FOLLOW_EVENT = 80,
		SUBSCRIBE_EVENT = 90,
		SUBSCRIBE_END_EVENT = 100,
		SUBSCRIBE_GIFT_EVENT = 110,
		SUBSCRIBE_MESSAGE_EVENT = 120,
		CHEER_EVENT = 130,

		// Polling
		LIVE = 1000,
		TITLE = 1010,
		CATEGORY = 1020,
	};

	void SetCondition(const Condition &);
	Condition GetCondition() { return _condition; }

	std::weak_ptr<TwitchToken> _token;
	TwitchChannel _channel;
	StringVariable _streamTitle = obs_module_text(
		"AdvSceneSwitcher.condition.twitch.title.title");
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig();
	TwitchCategory _category;

private:
	bool CheckChannelLiveEvents(TwitchToken &);
	bool CheckChannelOfflineEvents(TwitchToken &);
	bool CheckChannelUpdateEvents(TwitchToken &);
	bool CheckChannelFollowEvents(TwitchToken &);
	bool CheckChannelSubscribeEvents(TwitchToken &);
	bool CheckChannelCheerEvents(TwitchToken &);

	bool IsUsingEventSubCondition();
	void SetupEventSubscriptions();
	void CheckEventSubscription(EventSub &);
	void AddChannelLiveEventSubscription();
	void AddChannelOfflineEventSubscription();
	void AddChannelUpdateEventSubscription();
	void AddChannelFollowEventSubscription();
	void AddChannelSubscribeEventSubscription();
	void AddChannelCheerEventSubscription();

	Condition _condition = Condition::LIVE;
	std::future<std::string> _subscriptionIDFuture;
	std::string _subscriptionID;
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

	FilterComboBox *_conditions;
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
