#pragma once
#include "macro-action-edit.hpp"
#include "connection-manager.hpp"
#include "variable-text-edit.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QStringList>

namespace advss {

class MacroActionWebsocket : public MacroAction {
public:
	MacroActionWebsocket(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionWebsocket>(m);
	}

	enum class API {
		SCENE_SWITCHER,
		OBS_WEBSOCKET,
		GENERIC_WEBSOCKET,
	};

	enum class MessageType {
		REQUEST,
		EVENT,
	};

	API _api = API::SCENE_SWITCHER;
	MessageType _type = MessageType::REQUEST;
	StringVariable _message = obs_module_text("AdvSceneSwitcher.enterText");
	std::weak_ptr<Connection> _connection;

private:
	void SendRequest(const std::string &msg);

	static bool _registered;
	static const std::string id;
};

class MacroActionWebsocketEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWebsocketEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWebsocket> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWebsocketEdit(
			parent, std::dynamic_pointer_cast<MacroActionWebsocket>(
					action));
	}

private slots:
	void APITypeChanged(int);
	void MessageTypeChanged(int);
	void MessageChanged();
	void ConnectionSelectionChanged(const QString &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionWebsocket> _entryData;

private:
	void CheckForSettingsConflict();
	void SetupWidgetVisibility();
	void SetupRequestEdit();
	void SetupEventEdit();
	void SetupGenericEdit();
	void ClearWidgets();

	QComboBox *_apiType;
	QComboBox *_messageType;
	VariableTextEdit *_message;
	ConnectionSelection *_connection;
	QHBoxLayout *_editLayout;
	QLabel *_settingsConflict;

	bool _loading = true;
};

} // namespace advss
