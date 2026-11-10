#pragma once
#include "broadcast-selection.hpp"
#include "youtube-connection.hpp"

#include <macro-condition-edit.hpp>

#include <QComboBox>
#include <QLabel>
#include <QTimer>

namespace advss {

class MacroConditionYouTube : public MacroCondition {
public:
	MacroConditionYouTube(Macro *m) : MacroCondition(m) {}
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionYouTube>(m);
	}
	std::string GetId() const { return id; };
	std::string GetShortDesc() const;
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	enum class Condition {
		BROADCAST_STATUS = 10,
		STREAM_HEALTH = 20,
	};

	Condition _condition = Condition::BROADCAST_STATUS;
	std::weak_ptr<YouTubeConnection> _connection;
	YouTubeBroadcast _broadcast;
	std::string _broadcastStatus = "live";
	std::string _streamHealth = "good";

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroConditionYouTubeEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionYouTubeEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionYouTube> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionYouTubeEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionYouTube>(cond));
	}

private slots:
	void ConnectionChanged(const QString &);
	void ConditionChanged(int);
	void BroadcastStatusChanged(int);
	void StreamHealthChanged(int);
	void BroadcastChanged(const YouTubeBroadcast &);
	void CheckConnection();

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	YouTubeConnectionSelection *_connections;
	QComboBox *_conditions;
	QComboBox *_broadcastStatus;
	QComboBox *_streamHealth;
	YouTubeBroadcastSelection *_broadcast;
	QLabel *_connectionWarning;
	QTimer _connectionCheckTimer;

	std::shared_ptr<MacroConditionYouTube> _entryData;
	bool _loading = true;
};

} // namespace advss
