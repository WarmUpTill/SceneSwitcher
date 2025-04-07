#pragma once
#include "macro-condition-edit.hpp"
#include "mqtt-helpers.hpp"
#include "regex-config.hpp"
#include "variable-text-edit.hpp"

#include <QCheckBox>
#include <QPushButton>
#include <QTimer>

namespace advss {

class MacroConditionMqtt : public MacroCondition {
public:
	MacroConditionMqtt(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionMqtt>(m);
	}

	void SetConnection(const std::string &);
	std::weak_ptr<MqttConnection> GetConnection() const;

	StringVariable _message;
	RegexConfig _regex;
	bool _clearBufferOnMatch = true;

private:
	void SetupTempVars();

	std::weak_ptr<MqttConnection> _connection;
	MqttMessageBuffer _messageBuffer;
	std::chrono::high_resolution_clock::time_point _lastCheck{};
	static bool _registered;
	static const std::string id;
};

class MacroConditionMqttEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionMqttEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionMqtt> cond = nullptr);
	virtual ~MacroConditionMqttEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionMqttEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionMqtt>(cond));
	}

private slots:
	void ConnectionSelectionChanged(const QString &);
	void MqttMessageChanged();
	void ClearBufferOnMatchChanged(int);
	void RegexChanged(const RegexConfig &conf);
	void ToggleListen();
	void SetMessageSelectionToLastReceived();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void EnableListening(bool);

	MqttConnectionSelection *_connection;
	VariableTextEdit *_message;
	RegexConfigWidget *_regex;
	QPushButton *_listen;
	QCheckBox *_clearBufferOnMatch;

	std::shared_ptr<MacroConditionMqtt> _entryData;
	QTimer _listenTimer;
	MqttMessageBuffer _messageBuffer;
	bool _currentlyListening = false;
	bool _loading = true;
};

} // namespace advss
