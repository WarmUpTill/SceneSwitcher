#include "resource-table.hpp"
#include "plugin-state-helpers.hpp"
#include "resource-table-hotkey-handler.hpp"
#include "ui-helpers.hpp"

#include <QGridLayout>
#include <QHeaderView>

namespace advss {

ResourceTable::ResourceTable(QTabWidget *parent, const QString &help,
			     const QString &addToolTip,
			     const QString &removeToolTip,
			     const QStringList &headers,
			     const std::function<void()> &openSettings)
	: QWidget(parent),
	  _table(new QTableWidget()),
	  _add(new QPushButton()),
	  _remove(new QPushButton()),
	  _help(new QLabel(help))
{
	_add->setMaximumWidth(22);
	_add->setProperty("themeID",
			  QVariant(QString::fromUtf8("addIconSmall")));
	_add->setFlat(true);
	_add->setToolTip(addToolTip);

	_remove->setMaximumWidth(22);
	_remove->setProperty("themeID",
			     QVariant(QString::fromUtf8("removeIconSmall")));
	_remove->setFlat(true);
	_remove->setToolTip(removeToolTip);

	_help->setWordWrap(true);
	_help->setAlignment(Qt::AlignCenter);

	_table->setColumnCount(headers.size());
	_table->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeMode::Interactive);
	_table->setHorizontalHeaderLabels(headers);
	_table->verticalHeader()->hide();
	_table->setCornerButtonEnabled(false);
	_table->setShowGrid(false);
	_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_table->setSelectionBehavior(QAbstractItemView::SelectRows);

	auto helpAndTableLayout = new QGridLayout();
	helpAndTableLayout->setContentsMargins(0, 0, 0, 0);
	helpAndTableLayout->addWidget(_table, 0, 0);
	helpAndTableLayout->addWidget(_help, 0, 0, Qt::AlignCenter);

	auto controlLayout = new QHBoxLayout;
	controlLayout->setContentsMargins(0, 0, 0, 0);
	controlLayout->addWidget(_add);
	controlLayout->addWidget(_remove);
	controlLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->addLayout(helpAndTableLayout);
	layout->addLayout(controlLayout);
	setLayout(layout);

	QWidget::connect(_add, SIGNAL(clicked()), this, SLOT(Add()));
	QWidget::connect(_remove, SIGNAL(clicked()), this, SLOT(Remove()));
	QWidget::connect(_table, &QTableWidget::cellDoubleClicked,
			 [openSettings]() { openSettings(); });

	RegisterHotkeyFunction(this, Qt::Key_F2, openSettings);
	RegisterHotkeyFunction(this, Qt::Key_Delete, [this]() { Remove(); });
}

ResourceTable::~ResourceTable()
{
	DeregisterHotkeyFunctions(this);
}

void ResourceTable::SetHelpVisible(bool visible) const
{
	_help->setVisible(visible);
}

void ResourceTable::HighlightAddButton(bool enable)
{
	if (_highlightConnection) {
		_highlightConnection->deleteLater();
		_highlightConnection = nullptr;
	}

	if (enable && HighlightUIElementsEnabled()) {
		_highlightConnection = HighlightWidget(_add, QColor(Qt::green));
	}
}

void ResourceTable::resizeEvent(QResizeEvent *)
{
	const auto columnCount = _table->columnCount();
	const auto columnSize = (_table->width() - 1) / columnCount;
	for (int i = 0; i < columnCount; ++i) {
		_table->horizontalHeader()->resizeSection(i, columnSize);
	}
}

void AddItemTableRow(QTableWidget *table, const QStringList &cellLabels)
{
	int row = table->rowCount();
	table->setRowCount(row + 1);

	int col = 0;
	for (const auto &cellLabel : cellLabels) {
		auto *item = new QTableWidgetItem(cellLabel);
		item->setToolTip(cellLabel);
		table->setItem(row, col, item);
		col++;
	}

	table->sortByColumn(0, Qt::AscendingOrder);
}

void UpdateItemTableRow(QTableWidget *table, int row,
			const QStringList &cellLabels)
{
	int col = 1; // Skip the name cell
	for (const auto &cellLabel : cellLabels) {
		auto item = table->item(row, col);
		item->setText(cellLabel);
		item->setToolTip(cellLabel);
		col++;
	}
}

void RenameItemTableRow(QTableWidget *table, const QString &oldName,
			const QString &newName)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		if (item->text() == oldName) {
			item->setText(newName);
			table->sortByColumn(0, Qt::AscendingOrder);
			return;
		}
	}
	assert(false);
}

void RemoveItemTableRow(QTableWidget *table, const QString &name)
{
	for (int row = 0; row < table->rowCount(); ++row) {
		auto item = table->item(row, 0);
		if (item && item->text() == name) {
			table->removeRow(row);
			return;
		}
	}
	table->sortByColumn(0, Qt::AscendingOrder);
}

} // namespace advss
