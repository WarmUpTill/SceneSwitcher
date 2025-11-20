#pragma once
#include "resource-table.hpp"

#include <QCheckBox>
#include <QDialog>

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

class InvalidTokenDialog : public QDialog {
	Q_OBJECT
public:
	static void ShowWarning(const QString &name);

private:
	InvalidTokenDialog(const QString &name);
	QCheckBox *_doNotShowAgain;
};

} // namespace advss
