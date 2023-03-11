#pragma once
#include "macro-action-edit.hpp"
#include "connection-manager.hpp"
#include "variable-text-edit.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QStringList>

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

	enum class Type {
		REQUEST,
		EVENT,
	};

	Type _type = Type::REQUEST;
	StringVariable _message = obs_module_text("AdvSceneSwitcher.enterText");
	std::string _connection;

private:
	void SendRequest();

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
	void ActionChanged(int);
	void MessageChanged();
	void ConnectionSelectionChanged(const QString &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionWebsocket> _entryData;

private:
	void SetupRequestEdit();
	void SetupEventEdit();

	QComboBox *_actions;
	VariableTextEdit *_message;
	ConnectionSelection *_connection;
	QHBoxLayout *_editLayout;
	bool _loading = true;
};
