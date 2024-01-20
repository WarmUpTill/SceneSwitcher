#include "layout-helpers.hpp"

#include <QLabel>

namespace advss {

static std::string
getNextDelim(const std::string &text,
	     const std::unordered_map<std::string, QWidget *> &placeholders)
{
	size_t pos = std::string::npos;
	std::string res = "";

	for (const auto &ph : placeholders) {
		size_t newPos = text.find(ph.first);
		if (newPos <= pos) {
			pos = newPos;
			res = ph.first;
		}
	}

	if (pos == std::string::npos) {
		return "";
	}

	return res;
}

void PlaceWidgets(std::string text, QBoxLayout *layout,
		  const std::unordered_map<std::string, QWidget *> &placeholders,
		  bool addStretch)
{
	std::vector<std::pair<std::string, QWidget *>> labelsWidgetsPairs;

	std::string delim = getNextDelim(text, placeholders);
	while (delim != "") {
		size_t pos = text.find(delim);
		if (pos != std::string::npos) {
			labelsWidgetsPairs.emplace_back(text.substr(0, pos),
							placeholders.at(delim));
			text.erase(0, pos + delim.length());
		}
		delim = getNextDelim(text, placeholders);
	}

	if (text != "") {
		labelsWidgetsPairs.emplace_back(text, nullptr);
	}

	for (auto &lw : labelsWidgetsPairs) {
		if (lw.first != "") {
			layout->addWidget(new QLabel(lw.first.c_str()));
		}
		if (lw.second) {
			layout->addWidget(lw.second);
		}
	}
	if (addStretch) {
		layout->addStretch();
	}
}

void DeleteLayoutItemWidget(QLayoutItem *item)
{
	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
			widget->deleteLater();
		}
		delete item;
	}
}

void ClearLayout(QLayout *layout, int afterIdx)
{
	QLayoutItem *item;
	while ((item = layout->takeAt(afterIdx))) {
		if (item->layout()) {
			ClearLayout(item->layout());
			delete item->layout();
		}
		DeleteLayoutItemWidget(item);
	}
}

void SetLayoutVisible(QLayout *layout, bool visible)
{
	if (!layout) {
		return;
	}
	for (int i = 0; i < layout->count(); ++i) {
		QWidget *widget = layout->itemAt(i)->widget();
		QLayout *nestedLayout = layout->itemAt(i)->layout();
		if (widget) {
			widget->setVisible(visible);
		}
		if (nestedLayout) {
			SetLayoutVisible(nestedLayout, visible);
		}
	}
}

void SetGridLayoutRowVisible(QGridLayout *layout, int row, bool visible)
{
	for (int col = 0; col < layout->columnCount(); col++) {
		auto item = layout->itemAtPosition(row, col);
		if (!item) {
			continue;
		}

		auto rowLayout = item->layout();
		if (rowLayout) {
			SetLayoutVisible(rowLayout, visible);
		}

		auto widget = item->widget();
		if (widget) {
			widget->setVisible(visible);
		}
	}

	if (!visible) {
		layout->setRowMinimumHeight(row, 0);
	}
}

void AddStretchIfNecessary(QBoxLayout *layout)
{
	int itemCount = layout->count();
	if (itemCount > 0) {
		auto lastItem = layout->itemAt(itemCount - 1);
		auto lastSpacer = dynamic_cast<QSpacerItem *>(lastItem);
		if (!lastSpacer) {
			layout->addStretch();
		}
	} else {
		layout->addStretch();
	}
}

void RemoveStretchIfPresent(QBoxLayout *layout)
{
	int itemCount = layout->count();
	if (itemCount > 0) {
		auto lastItem = layout->itemAt(itemCount - 1);
		auto lastSpacer = dynamic_cast<QSpacerItem *>(lastItem);
		if (lastSpacer) {
			layout->removeItem(lastItem);
			delete lastItem;
		}
	}
}

void MinimizeSizeOfColumn(QGridLayout *layout, int idx)
{
	if (idx >= layout->columnCount()) {
		return;
	}

	for (int i = 0; i < layout->columnCount(); i++) {
		if (i == idx) {
			layout->setColumnStretch(i, 0);
		} else {
			layout->setColumnStretch(i, 1);
		}
	}

	int columnWidth = 0;
	for (int row = 0; row < layout->rowCount(); row++) {
		auto item = layout->itemAtPosition(row, idx);
		if (!item) {
			continue;
		}
		auto widget = item->widget();
		if (!widget) {
			continue;
		}
		columnWidth = std::max(columnWidth,
				       widget->minimumSizeHint().width());
	}
	layout->setColumnMinimumWidth(idx, columnWidth);
}

} // namespace advss
