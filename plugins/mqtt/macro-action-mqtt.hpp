#pragma once
#include "macro-action-edit.hpp"
#include "mqtt-helpers.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"
#include "variable-text-edit.hpp"

namespace advss {

class MacroActionMqtt : public MacroAction {
public:
	MacroActionMqtt(Macro *m) : MacroAction(m, true) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	void SetConnection(const std::string &name);
	std::weak_ptr<MqttConnection> GetConnection() const;
	std::string GetShortDesc() const;
	void ResolveVariablesToFixedValues();
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	StringVariable _message = "payload";
	StringVariable _topic = "/topic";
	IntVariable _qos = 1;
	bool _retain = false;

private:
	std::weak_ptr<MqttConnection> _connection;

	static bool _registered;
	static const std::string id;
};

class MacroActionMqttEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionMqttEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionMqtt> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionMqttEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionMqtt>(action));
	}

private slots:
	void ConnectionSelectionChanged(const QString &);
	void MqttMessageChanged();
	void MqttTopicChanged();
	void QoSChanged(const NumberVariable<int> &value);
	void RetainChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

private:
	MqttConnectionSelection *_connection;
	VariableTextEdit *_message;
	VariableLineEdit *_topic;
	VariableSpinBox *_qos;
	QCheckBox *_retain;

	std::shared_ptr<MacroActionMqtt> _entryData;
	bool _loading = true;
};

} // namespace advss
