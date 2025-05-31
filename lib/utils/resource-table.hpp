#pragma once
#include "export-symbol-helper.hpp"

#include <QLabel>
#include <QString>
#include <QToolButton>
#include <QTableWidget>

class QResizeEvent;

namespace advss {

class ADVSS_EXPORT ResourceTable : public QWidget {
	Q_OBJECT

public:
	ResourceTable(QTabWidget *parent, const QString &help,
		      const QString &addToolTip, const QString &removeToolTip,
		      const QStringList &headers,
		      const std::function<void()> &openSettings);

	QTableWidget *Table() const { return _table; }
	void SetHelpVisible(bool) const;
	void HighlightAddButton(bool);

protected slots:
	virtual void Add() {}
	virtual void Remove() {}

protected:
	void resizeEvent(QResizeEvent *event);

private:
	QTableWidget *_table;
	QToolButton *_add;
	QToolButton *_remove;
	QLabel *_help;

	QObject *_highlightConnection = nullptr;
};

EXPORT void AddItemTableRow(QTableWidget *table, const QStringList &cells);
EXPORT void UpdateItemTableRow(QTableWidget *table, int row,
			       const QStringList &cells);
EXPORT void RenameItemTableRow(QTableWidget *table, const QString &oldName,
			       const QString &newName);
EXPORT void RemoveItemTableRow(QTableWidget *table, const QString &name);

} // namespace advss
