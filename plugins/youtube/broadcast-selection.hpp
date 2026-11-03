#pragma once
#include <obs-data.h>
#include <variable-line-edit.hpp>

#include <optional>
#include <QComboBox>

namespace advss {

class YouTubeConnection;

struct YouTubeBroadcastInfo {
	std::string id;
	std::string title;
	std::string scheduledStartTime;
	std::string lifeCycleStatus;
	std::string boundStreamId;
	bool monitorStreamEnabled = false;
};

class YouTubeBroadcast {
public:
	enum class Type {
		BOUND_TO_OBS_STREAM_KEY,
		NEXT_UPCOMING,
		ID,
	};

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void ResolveVariables();
	std::optional<YouTubeBroadcastInfo>
	GetInfo(YouTubeConnection &connection) const;

private:
	Type _type = Type::BOUND_TO_OBS_STREAM_KEY;
	StringVariable _id = "";

	friend class YouTubeBroadcastSelection;
};

class YouTubeBroadcastSelection : public QWidget {
	Q_OBJECT

public:
	YouTubeBroadcastSelection(QWidget *parent);
	void SetBroadcast(const YouTubeBroadcast &);

private slots:
	void TypeChanged(int);
	void IdChanged();

signals:
	void BroadcastChanged(const YouTubeBroadcast &);

private:
	void SetWidgetVisibility();

	QComboBox *_type;
	VariableLineEdit *_id;
	YouTubeBroadcast _broadcast;
};

} // namespace advss
