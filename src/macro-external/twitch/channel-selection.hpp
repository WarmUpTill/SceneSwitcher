#pragma once
#include <obs-data.h>
#include <variable-line-edit.hpp>
#include <QPushButton>
#include <QDate>

namespace advss {

class TwitchToken;

// Information returned by the https://api.twitch.tv/helix/streams API call
// Channel must be live
struct ChannelLiveInfo {
	bool IsLive() const;

	std::string id;
	std::string user_id;
	std::string user_login;
	std::string user_name;
	std::string game_id;
	std::string game_name;
	std::string type;
	std::string title;
	std::vector<std::string> tags;
	int viewer_count;
	QDate started_at;
	std::string language;
	std::string thumbnail_url;
	bool is_mature;
};

// Information returned by the https://api.twitch.tv/helix/channels API call
// Channel can be offline
struct ChannelInfo {
	std::string broadcaster_id;
	std::string broadcaster_login;
	std::string broadcaster_name;
	std::string broadcaster_language;
	std::string game_name;
	std::string game_id;
	std::string title;
	uint32_t delay;
	std::vector<std::string> tags;
	std::vector<std::string> content_classification_labels;
	bool is_branded_content;
};

struct TwitchChannel {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetUserID(const TwitchToken &);
	bool IsValid() const;
	std::optional<ChannelLiveInfo> GetLiveInfo(const TwitchToken &);
	std::optional<ChannelInfo> GetInfo(const TwitchToken &);

private:
	std::string _id = "-1";
	StringVariable _name = "";
	std::string _lastResolvedValue = "";

	friend class TwitchChannelSelection;
};

class TwitchChannelSelection : public QWidget {
	Q_OBJECT

public:
	TwitchChannelSelection(QWidget *parent);
	void SetChannel(const TwitchChannel &);

private slots:
	void SelectionChanged();
	void OpenChannel();

signals:
	void ChannelChanged(const TwitchChannel &);

private:
	VariableLineEdit *_channelName;
	QPushButton *_openChannel;
};

} // namespace advss
