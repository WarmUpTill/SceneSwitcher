#pragma once
#include <obs-data.h>
#include <variable-line-edit.hpp>

#include <memory>
#include <optional>
#include <QComboBox>
#include <QGridLayout>
#include <vector>

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
		BOUND_TO_OBS_STREAM_KEY = 0,
		ID = 2,
		SELECTED = 3,
	};

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void ResolveVariables();
	std::optional<YouTubeBroadcastInfo>
	GetInfo(YouTubeConnection &connection, bool useCache = false) const;

private:
	Type _type = Type::BOUND_TO_OBS_STREAM_KEY;
	StringVariable _id = "";
	std::string _selectedId;
	std::string _selectedTitle;

	friend class YouTubeBroadcastSelection;
};

class BroadcastComboBox : public QComboBox {
	Q_OBJECT

public:
	using QComboBox::QComboBox;
	void showPopup() override;

signals:
	void AboutToShowPopup();
};

class YouTubeBroadcastSelection : public QWidget {
	Q_OBJECT

public:
	YouTubeBroadcastSelection(QWidget *parent);
	void SetBroadcast(const YouTubeBroadcast &);
	void SetConnection(const std::weak_ptr<YouTubeConnection> &);

private slots:
	void SelectionChanged(int);
	void IdChanged();
	void UpdateBroadcastList();

signals:
	void BroadcastChanged(const YouTubeBroadcast &);

private:
	void PopulateSelection(const std::vector<YouTubeBroadcastInfo> &);
	void AddSelectionItem(const QString &text, YouTubeBroadcast::Type type,
			      const std::string &id = "",
			      const std::string &title = "");
	void SetWidgetVisibility();

	BroadcastComboBox *_selection;
	VariableLineEdit *_id;
	QGridLayout *_layout;
	YouTubeBroadcast _broadcast;
	std::weak_ptr<YouTubeConnection> _connection;
};

} // namespace advss
