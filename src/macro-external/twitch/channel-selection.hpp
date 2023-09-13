#pragma once
#include <obs-data.h>
#include <variable-line-edit.hpp>
#include <QPushButton>

namespace advss {

class TwitchToken;

struct TwitchChannel {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string GetUserID(const TwitchToken &);

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
