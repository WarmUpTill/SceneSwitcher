#pragma once
#include "resource-table.hpp"

namespace advss {

class WSConnectionsTable final : public ResourceTable {
	Q_OBJECT

public:
	static WSConnectionsTable *Create();

private slots:
	void Add();
	void Remove();

private:
	WSConnectionsTable(QTabWidget *parent = nullptr);
};

} // namespace advss
