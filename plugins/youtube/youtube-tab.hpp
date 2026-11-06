#pragma once
#include "resource-table.hpp"

#include <QCheckBox>
#include <QDialog>

namespace advss {

class YouTubeConnectionsTable final : public ResourceTable {
	Q_OBJECT

public:
	static YouTubeConnectionsTable *Create();

private slots:
	void Add();
	void Remove();

private:
	YouTubeConnectionsTable(QTabWidget *parent = nullptr);
};

class InvalidConnectionDialog : public QDialog {
	Q_OBJECT
public:
	static void ShowWarning(const QString &name);

private:
	InvalidConnectionDialog(const QString &name);
	QCheckBox *_doNotShowAgain;
};

} // namespace advss
