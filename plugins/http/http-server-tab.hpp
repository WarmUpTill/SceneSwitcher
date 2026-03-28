#pragma once
#include "resource-table.hpp"

namespace advss {

class HttpServersTable final : public ResourceTable {
	Q_OBJECT

public:
	static HttpServersTable *Create();

private slots:
	void Add();
	void Remove();

private:
	HttpServersTable(QTabWidget *parent = nullptr);
};

} // namespace advss
