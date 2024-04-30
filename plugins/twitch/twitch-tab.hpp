#pragma once
#include "resource-table.hpp"

namespace advss {

class TwitchConnectionsTable final : public ResourceTable {
	Q_OBJECT

public:
	static TwitchConnectionsTable *Create();

private slots:
	void Add();
	void Remove();

private:
	TwitchConnectionsTable(QTabWidget *parent = nullptr);
};

} // namespace advss
