#include "utility.hpp"

#include <QTextStream>
#include <sstream>

namespace advss {

std::pair<int, int> GetCursorPos()
{
	auto cursorPos = QCursor::pos();
	return {cursorPos.x(), cursorPos.y()};
}

bool ReplaceAll(std::string &str, const std::string &from,
		const std::string &to)
{
	if (from.empty()) {
		return false;
	}
	bool somethingWasReplaced = false;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
		somethingWasReplaced = true;
	}
	return somethingWasReplaced;
}

bool CompareIgnoringLineEnding(QString &s1, QString &s2)
{
	// Let QT deal with different types of lineendings
	QTextStream s1stream(&s1);
	QTextStream s2stream(&s2);

	while (!s1stream.atEnd() || !s2stream.atEnd()) {
		QString s1s = s1stream.readLine();
		QString s2s = s2stream.readLine();
		if (s1s != s2s) {
			return false;
		}
	}

	if (!s1stream.atEnd() && !s2stream.atEnd()) {
		return false;
	}

	return true;
}

std::string ToString(double value)
{
	std::stringstream stream;
	stream << value;
	return stream.str();
}

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QObject **addHighlight)
{
	if (!list || !newWidget) {
		return;
	}

	if (addHighlight && *addHighlight) {
		(*addHighlight)->deleteLater();
		*addHighlight = nullptr;
	}

	QListWidgetItem *item;
	item = new QListWidgetItem(list);
	list->addItem(item);
	item->setSizeHint(newWidget->minimumSizeHint());
	list->setItemWidget(item, newWidget);

	list->scrollToItem(item);
}

bool listMoveUp(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == 0) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index - 1, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index + 1);
	list->setCurrentRow(index - 1);
	return true;
}

bool listMoveDown(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == list->count() - 1) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index + 2, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index);
	list->setCurrentRow(index + 1);
	return true;
}

} // namespace advss
