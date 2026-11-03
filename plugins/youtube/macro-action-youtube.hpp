#pragma once
#include "broadcast-selection.hpp"
#include "youtube-connection.hpp"

#include <macro-action-edit.hpp>

#include <QComboBox>
#include <QLabel>
#include <QTimer>

namespace advss {

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
	};

	Action _action = Action::START_BROADCAST;
	std::weak_ptr<YouTubeConnection> _connection;
	YouTubeBroadcast _broadcast;

private:
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
	void CheckConnection();

signals:
	void HeaderInfoChanged(const QString &);

private:
	YouTubeConnectionSelection *_connections;
	QComboBox *_actions;
	YouTubeBroadcastSelection *_broadcast;
	QLabel *_connectionWarning;
	QTimer _connectionCheckTimer;

	std::shared_ptr<MacroActionYouTube> _entryData;
	bool _loading = true;
};

} // namespace advss
