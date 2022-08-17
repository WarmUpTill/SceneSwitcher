#pragma once
#include "macro-action-edit.hpp"
#include "connection-manager.hpp"
#include "resizing-text-edit.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QStringList>

class MacroActionWebsocket : public MacroAction {
public:
	MacroActionWebsocket(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionWebsocket>(m);
	}

	std::string _message = obs_module_text("AdvSceneSwitcher.enterText");
	std::string _connection;

private:
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
	void MessageChanged();
	void ConnectionSelectionChanged(const QString &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionWebsocket> _entryData;

private:
	ResizingPlainTextEdit *_message;
	ConnectionSelection *_connection;
	bool _loading = true;
};
