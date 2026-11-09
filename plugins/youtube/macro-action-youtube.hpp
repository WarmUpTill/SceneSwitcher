#pragma once
#include "broadcast-selection.hpp"
#include "youtube-connection.hpp"

#include <duration-control.hpp>
#include <duration.hpp>
#include <file-selection.hpp>
#include <help-icon.hpp>
#include <macro-action-edit.hpp>
#include <variable-text-edit.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QTimer>

namespace advss {

struct YouTubeVideoCategory {
	std::string id;
	std::string title;
};

class MacroActionYouTube : public MacroAction {
public:
	MacroActionYouTube(Macro *m) : MacroAction(m) {}
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	std::string GetId() const { return id; };
	std::string GetShortDesc() const;
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	void ResolveVariablesToFixedValues();

	enum class Action {
		START_BROADCAST = 10,
		END_BROADCAST = 20,
		CREATE_BROADCAST = 30,
	};

	void SetAction(Action);
	Action GetAction() const { return _action; }

	std::weak_ptr<YouTubeConnection> _connection;
	YouTubeBroadcast _broadcast;
	StringVariable _title = obs_module_text(
		"AdvSceneSwitcher.action.youtube.create.title.default");
	StringVariable _description = "";
	std::string _privacy = "private";
	bool _autoStart = true;
	bool _autoStop = true;
	bool _madeForKids = false;
	bool _bindToObsStreamKey = true;
	std::string _categoryId;
	std::string _categoryTitle;
	StringVariable _thumbnail = "";
	std::string _latency = "normal";
	bool _dvr = true;
	bool _projection360 = false;
	bool _scheduleForLater = false;
	Duration _scheduledStartIn = 3600.0;

private:
	void CreateBroadcast(YouTubeConnection &connection);
	void SetupTempVars();

	Action _action = Action::START_BROADCAST;

	static bool _registered;
	static const std::string id;
};

class MacroActionYouTubeEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionYouTubeEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionYouTube> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionYouTubeEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionYouTube>(action));
	}

private slots:
	void ConnectionChanged(const QString &);
	void ActionChanged(int);
	void BroadcastChanged(const YouTubeBroadcast &);
	void TitleChanged();
	void DescriptionChanged();
	void PrivacyChanged(int);
	void CategoryChanged(int);
	void UpdateCategoryList();
	void ThumbnailChanged(const QString &);
	void LatencyChanged(int);
	void ScheduledStartInChanged(const Duration &);
	void CreateOptionsChanged();
	void CheckConnection();

signals:
	void HeaderInfoChanged(const QString &);

private:
	void PopulateCategories(const std::vector<YouTubeVideoCategory> &);
	void SetWidgetVisibility();

	YouTubeConnectionSelection *_connections;
	QComboBox *_actions;
	YouTubeBroadcastSelection *_broadcast;
	HelpIcon *_help;
	QWidget *_createSettings;
	VariableLineEdit *_title;
	VariableTextEdit *_description;
	QComboBox *_privacy;
	LazyFilterComboBox *_category;
	FileSelection *_thumbnail;
	QComboBox *_latency;
	QCheckBox *_scheduleForLater;
	DurationSelection *_scheduledStartIn;
	QCheckBox *_autoStart;
	QCheckBox *_autoStop;
	QCheckBox *_dvr;
	QCheckBox *_projection360;
	QCheckBox *_madeForKids;
	QCheckBox *_bindToObsStreamKey;
	QLabel *_connectionWarning;
	QTimer _connectionCheckTimer;

	std::shared_ptr<MacroActionYouTube> _entryData;
	bool _loading = true;
};

} // namespace advss
