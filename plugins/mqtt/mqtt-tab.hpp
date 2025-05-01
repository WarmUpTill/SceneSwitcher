#pragma once
#include "resource-table.hpp"

namespace advss {

class MqttConnectionsTable final : public ResourceTable {
	Q_OBJECT

public:
	static MqttConnectionsTable *Create();

private slots:
	void Add();
	void Remove();

private:
	MqttConnectionsTable(QTabWidget *parent = nullptr);
};

} // namespace advss
