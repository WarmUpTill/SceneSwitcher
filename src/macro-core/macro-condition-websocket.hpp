#pragma once
#include "macro.hpp"
#include "connection-manager.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"

#include <QCheckBox>

class MacroConditionWebsocket : public MacroCondition {
public:
	MacroConditionWebsocket(Macro *m) : MacroCondition(m) {}
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

	Type _type = Type::REQUEST;
	VariableResolvingString _message =
		obs_module_text("AdvSceneSwitcher.enterText");
	RegexConfig _regex;
	std::string _connection;

private:
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
	void RegexChanged(RegexConfig);
	void ConnectionSelectionChanged(const QString &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroConditionWebsocket> _entryData;

private:
	void SetupRequestEdit();
	void SetupEventEdit();

	QComboBox *_conditions;
	VariableTextEdit *_message;
	RegexConfigWidget *_regex;
	ConnectionSelection *_connection;
	QHBoxLayout *_editLayout;
	bool _loading = true;
};
