#pragma once
#include "resource-table.hpp"

namespace advss {

class ActionQueueTable final : public ResourceTable {
	Q_OBJECT

public:
	static ActionQueueTable *Create();

private slots:
	void Add();
	void Remove();

private:
	ActionQueueTable(QTabWidget *parent = nullptr);
};

} // namespace advss
