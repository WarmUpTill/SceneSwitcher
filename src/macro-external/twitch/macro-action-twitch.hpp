#pragma once
#include "macro-action-edit.hpp"
#include "token.hpp"
#include "category-selection.hpp"
#include "channel-selection.hpp"

#include <variable-line-edit.hpp>
#include <variable-text-edit.hpp>
#include <duration-control.hpp>

namespace advss {

class MacroActionTwitch : public MacroAction {
public:
	MacroActionTwitch(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionTwitch>(m);
	}
	bool ActionIsSupportedByToken();

	enum class Action {
		TITLE,
		CATEGORY,
		MARKER,
		CLIP,
		COMMERCIAL,
		ANNOUNCEMENT,
		ENABLE_EMOTE_ONLY,
		DISABLE_EMOTE_ONLY,
		RAID,
	};

	enum class AnnouncementColor {
		PRIMARY,
		BLUE,
		GREEN,
		ORANGE,
		PURPLE,
	};

	Action _action = Action::TITLE;
	std::weak_ptr<TwitchToken> _token;
	StringVariable _streamTitle =
		obs_module_text("AdvSceneSwitcher.action.twitch.title.title");
	TwitchCategory _category;
	StringVariable _markerDescription = obs_module_text(
		"AdvSceneSwitcher.action.twitch.marker.description");
	bool _clipHasDelay = false;
	Duration _duration = 60;
	StringVariable _announcementMessage = obs_module_text(
		"AdvSceneSwitcher.action.twitch.announcement.message");
	AnnouncementColor _announcementColor = AnnouncementColor::PRIMARY;
	TwitchChannel _channel;

private:
	void SetStreamTitle(const std::shared_ptr<TwitchToken> &) const;
	void SetStreamCategory(const std::shared_ptr<TwitchToken> &) const;
	void CreateStreamMarker(const std::shared_ptr<TwitchToken> &) const;
	void CreateStreamClip(const std::shared_ptr<TwitchToken> &) const;
	void StartCommercial(const std::shared_ptr<TwitchToken> &) const;
	void SendChatAnnouncement(const std::shared_ptr<TwitchToken> &) const;
	void SetChatEmoteOnlyMode(const std::shared_ptr<TwitchToken> &,
				  bool enable) const;
	void StartRaid(const std::shared_ptr<TwitchToken> &);

	static bool _registered;
	static const std::string id;
};

class MacroActionTwitchEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTwitchEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTwitch> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTwitchEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTwitch>(action));
	}

private slots:
	void ActionChanged(int);
	void TwitchTokenChanged(const QString &);
	void CheckTokenPermissions();
	void StreamTitleChanged();
	void CategoreyChanged(const TwitchCategory &);
	void MarkerDescriptionChanged();
	void ClipHasDelayChanged(int state);
	void DurationChanged(const Duration &);
	void AnnouncementMessageChanged();
	void AnnouncementColorChanged(int index);
	void ChannelChanged(const TwitchChannel &);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionTwitch> _entryData;

private:
	void SetupWidgetVisibility();

	QHBoxLayout *_layout;
	QComboBox *_actions;
	TwitchConnectionSelection *_tokens;
	QLabel *_tokenPermissionWarning;
	QTimer _tokenPermissionCheckTimer;

	VariableLineEdit *_streamTitle;
	TwitchCategoryWidget *_category;
	VariableLineEdit *_markerDescription;
	QCheckBox *_clipHasDelay;
	DurationSelection *_duration;
	VariableTextEdit *_announcementMessage;
	QComboBox *_announcementColor;
	TwitchChannelSelection *_channel;

	bool _loading = true;
};

} // namespace advss
