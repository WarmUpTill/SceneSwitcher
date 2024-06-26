#pragma once
#include "macro-condition-edit.hpp"
#include "connection-manager.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"
#include "websocket-helpers.hpp"

#include <QCheckBox>

namespace advss {

class MacroConditionWebsocket : public MacroCondition {
public:
	MacroConditionWebsocket(Macro *m);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionWebsocket>(m);
	}

	enum class Type {
		REQUEST,
		EVENT,
	};

	void SetType(Type);
	Type GetType() const { return _type; }
	void SetConnection(const std::string &);
	std::weak_ptr<WSConnection> GetConnection() const;
	StringVariable _message = obs_module_text("AdvSceneSwitcher.enterText");
	RegexConfig _regex;
	bool _clearBufferOnMatch = true;

private:
	void SetupTempVars();

	Type _type = Type::REQUEST;
	std::weak_ptr<WSConnection> _connection;

	WebsocketMessageBuffer _messageBuffer;
	std::chrono::high_resolution_clock::time_point _lastCheck{};

	static bool _registered;
	static const std::string id;
};

class MacroConditionWebsocketEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionWebsocketEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionWebsocket> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionWebsocketEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionWebsocket>(
				cond));
	}

private slots:
	void ConditionChanged(int);
	void MessageChanged();
	void RegexChanged(const RegexConfig &);
	void ConnectionSelectionChanged(const QString &);
	void ClearBufferOnMatchChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetupRequestEdit();
	void SetupEventEdit();

	QComboBox *_conditions;
	VariableTextEdit *_message;
	RegexConfigWidget *_regex;
	WSConnectionSelection *_connection;
	QCheckBox *_clearBufferOnMatch;
	QHBoxLayout *_editLayout;

	std::shared_ptr<MacroConditionWebsocket> _entryData;
	bool _loading = true;
};

} // namespace advss
