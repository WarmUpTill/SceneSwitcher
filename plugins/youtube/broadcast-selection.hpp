#pragma once
#include <filter-combo-box.hpp>
#include <obs-data.h>
#include <variable-line-edit.hpp>

#include <memory>
#include <optional>
#include <QActionGroup>
#include <QLabel>
#include <QPushButton>
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
		SELECTED_IN_OBS = 4,
	};

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void ResolveVariables();
	std::optional<YouTubeBroadcastInfo>
	GetInfo(YouTubeConnection &connection, bool useCache = false) const;

private:
	Type _type = Type::SELECTED;
	StringVariable _id = "";
	std::string _selectedId;
	std::string _selectedTitle;

	friend class YouTubeBroadcastSelection;
};

std::string GetObsStreamId(YouTubeConnection &connection);

class LazyFilterComboBox : public FilterComboBox {
	Q_OBJECT

public:
	using FilterComboBox::FilterComboBox;
	void showPopup() override;

protected:
	void focusInEvent(QFocusEvent *event) override;

signals:
	void ItemsNeeded();
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
	void ModeChanged(QAction *);
	void UpdateBroadcastList();

signals:
	void BroadcastChanged(const YouTubeBroadcast &);

private:
	void PopulateSelection(const std::vector<YouTubeBroadcastInfo> &);
	void AddSelectionItem(const QString &text, const std::string &id,
			      const std::string &title);
	void AddModeAction(QMenu *menu, const char *text,
			   YouTubeBroadcast::Type type);
	void SetWidgetVisibility();

	LazyFilterComboBox *_selection;
	QLabel *_boundToObsStreamKey;
	QLabel *_selectedInObs;
	VariableLineEdit *_id;
	QPushButton *_mode;
	QActionGroup *_modes;
	YouTubeBroadcast _broadcast;
	std::weak_ptr<YouTubeConnection> _connection;
};

} // namespace advss
