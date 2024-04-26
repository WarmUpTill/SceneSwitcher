#pragma once
#include "resource-table.hpp"

namespace advss {

class VariableTable final : public ResourceTable {
	Q_OBJECT

public:
	static VariableTable *Create();

private slots:
	void Add();
	void Remove();

private:
	VariableTable(QTabWidget *parent = nullptr);
};

} // namespace advss
